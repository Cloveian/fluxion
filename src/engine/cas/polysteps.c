/*Fluxion: steps for a polynomial past x^2 that factors into lines and quadratics, x^3 - 2x^2 - x + 2 = 0. not in upstream PineappleCAS.*/

#include "steps.h"

#define FACTORS_MAX STEP_EQS_MAX
#define LIGHT (SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_RATIONAL | SIMP_EVAL | SIMP_LIKE_TERMS)

/*the degree of a factor, 1 or 2, 0 for anything else*/
static int factor_deg(pcas_ast_t *b, Symbol var) {
    pcas_ast_t *co[3];
    int deg = poly_coefs(b, var, co), i;

    for(i = 0; i < 3; i++)
        ast_Cleanup(co[i]);
    return deg == 1 || deg == 2 ? deg : 0;
}

/*the factors of g worth a zero, once each: a number is out, x in 2x^2(x-3) is x. false if one is past x^2 or there's too many*/
static bool factors_of(pcas_ast_t *g, Symbol var, pcas_ast_t **list, int *n) {
    pcas_ast_t *c = isoptype(g, OP_MULT) ? ast_ChildGet(g, 0) : g;
    int i;

    *n = 0;
    for(; c != NULL; c = isoptype(g, OP_MULT) ? c->next : NULL) {
        pcas_ast_t *b = c;

        if(!has_symbol(c, var))
            continue;
        if(isoptype(c, OP_POW) && ast_ChildGet(c, 1)->type == NODE_NUMBER)
            b = ast_ChildGet(c, 0);
        if(factor_deg(b, var) == 0)
            return false;
        for(i = 0; i < *n && !ast_Compare(list[i], b); i++)
            ;
        if(i < *n)
            continue;
        if(*n == FACTORS_MAX)
            return false;
        list[(*n)++] = b;
    }
    return *n >= 2;
}

/*var = each of roots on one line, takes them all. false when there's no room for the line*/
static bool roots_line(steps_t *out, Symbol var, pcas_ast_t **roots, int n) {
    step_t *l = n > 0 ? steps_eq(out, ast_MakeSymbol(var), roots[0]) : NULL;
    int i;

    for(i = 1; l != NULL && i < n; i++) {
        l->eq[i].left = ast_MakeSymbol(var);
        l->eq[i].right = roots[i];
        l->eqs = i + 1;
    }
    for(; l == NULL && i < n; i++)
        ast_Cleanup(roots[i]);
    return l != NULL;
}

static step_t *last_line(steps_t *s) {
    return &s->line[s->n - 1];
}

bool steps_poly(pcas_ast_t *left, pcas_ast_t *right, Symbol var, steps_t *out) {
    static steps_t t; /*a quadratic's own steps, off the stack*/
    pcas_ast_t *f, *g, *list[FACTORS_MAX], *roots[SOLVE_MAX] = {NULL}, *each[FACTORS_MAX] = {NULL};
    step_t *l;
    int n, i, lines = 0, k;
    bool ok = false;

    out->n = 0;
    l = steps_eq(out, ast_Copy(left), ast_Copy(right));
    if(l == NULL)
        return false;
    simplify(l->eq[0].left, SIMP_NORMALIZE | SIMP_EVAL);
    simplify(l->eq[0].right, SIMP_NORMALIZE | SIMP_EVAL);
    l->as_is = true; /*the way it was typed, the canonical order puts -x in front of x^3*/

    f = ast_MakeBinary(OP_ADD, ast_Copy(left), ast_MakeBinary(OP_MULT, ast_MakeInt(-1), ast_Copy(right)));
    if(f == NULL)
        goto fail;
    simplify(f, LIGHT);
    expand(f, EXP_ALL);
    simplify(f, LIGHT);
    g = ast_Copy(f);
    factor(g, FAC_POLYNOMIAL);
    if(g == NULL || !factors_of(g, var, list, &n))
        goto done;

    if(!is_ast_int(right, 0)) { /*everything on one side*/
        l->move = '_';
        l->by = ast_Copy(right);
        l = steps_eq(out, ast_Copy(f), ast_MakeInt(0));
        if(l == NULL)
            goto done;
    }
    steps_rule(l, "factor");
    l = steps_eq(out, ast_Copy(g), ast_MakeInt(0));
    if(l == NULL)
        goto done;

    steps_rule(l, "one factor has to be 0");
    l = steps_eq(out, ast_Copy(list[0]), ast_MakeInt(0));
    for(i = 1; l != NULL && i < n; i++) {
        l->eq[i].left = ast_Copy(list[i]);
        l->eq[i].right = ast_MakeInt(0);
        l->eqs = i + 1;
    }
    if(l == NULL)
        goto done;

    /*the lines first, x = each on one line*/
    for(i = 0; i < n; i++) {
        if(factor_deg(list[i], var) != 1)
            continue;
        k = solve_ex(list[i], var, roots, false);
        if(k != 1)
            goto done;
        each[lines++] = roots[0];
        roots[0] = NULL;
    }
    if(lines > 0) {
        steps_rule(last_line(out), "solve each");
        ok = roots_line(out, var, each, lines);
        for(i = 0; i < lines; i++)
            each[i] = NULL; /*taken, or freed when there was no room*/
        if(!ok)
            goto done;
        ok = false;
    }
    /*then each quadratic with its own steps*/
    for(i = 0; i < n; i++) {
        pcas_ast_t *zero;

        if(factor_deg(list[i], var) != 2)
            continue;
        steps_rule(last_line(out), lines > 0 || i > 0 ? "the next factor" : "solve each");
        if(steps_eq(out, ast_Copy(list[i]), ast_MakeInt(0)) == NULL)
            goto done;
        zero = ast_MakeInt(0);
        if(!steps_solve(list[i], zero, var, METHOD_OPTIMAL, &t) || !steps_append(out, &t)) {
            ast_Cleanup(zero);
            goto done;
        }
        ast_Cleanup(zero);
    }

    /*all the roots togehter, when they fit a line*/
    k = solve_ex(f, var, roots, true);
    if(k >= 1 && k <= STEP_EQS_MAX) {
        steps_rule(last_line(out), "all the roots");
        ok = roots_line(out, var, roots, k);
        for(i = 0; i < k; i++)
            roots[i] = NULL;
        if(!ok)
            goto done;
    }
    ok = !pcas_cancelled;

done:
    for(i = 0; i < SOLVE_MAX; i++)
        ast_Cleanup(roots[i]);
    for(i = 0; i < FACTORS_MAX; i++)
        ast_Cleanup(each[i]);
    ast_Cleanup(f);
    ast_Cleanup(g);
fail:
    if(ok)
        steps_shown(out);
    else
        steps_free(out);
    return ok;
}
