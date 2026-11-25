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

/*a polynomial line in the order you'd write it, x^3 first. the canonical form with the powers combined puts -x in front of x^3*/
static void show_poly(step_t *l) {
    simplify(l->eq[0].left, SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_RATIONAL | SIMP_EVAL | SIMP_LIKE_TERMS);
    simplify_canonical_form(l->eq[0].left, CANONICAL_ALL ^ CANONICAL_COMBINE_POWERS);
    simplify_canonical_form(l->eq[0].right, CANONICAL_ALL ^ CANONICAL_COMBINE_POWERS);
    l->as_is = true;
}

static step_t *last_line(steps_t *s) {
    return &s->line[s->n - 1];
}

/*r times num/den, in place*/
static void rat_scale(mp_rat r, mp_small num, mp_small den) {
    mp_rat f = num_FromInt(0);

    if(f == NULL)
        return;
    mp_rat_set_value(f, num, den);
    mp_rat_mul(r, f, r);
    num_Cleanup(f);
}

/*the coefficients of a cubic as numbers, c[0] to c[3], from its *dih*rivatives at 0. false if a coefficient isn't a number*/
static bool cubic_numbers(pcas_ast_t *f, Symbol var, mp_rat *c) {
    pcas_ast_t *d = ast_Copy(f), *x = ast_MakeSymbol(var), *z = ast_MakeInt(0), *v, *next;
    mp_rat fact = num_FromInt(1);
    int k;
    bool ok = d != NULL && x != NULL;

    for(k = 0; ok && k <= 3; k++) {
        v = ast_Copy(d);
        substitute(v, x, z);
        simplify(v, SIMP_ALL);
        fold_fraction(v);
        ok = v->type == NODE_NUMBER && mp_rat_div(v->op.num, fact, c[k]) == MP_OK;
        ast_Cleanup(v);
        next = ok ? slope_of(d, x) : NULL;
        ast_Cleanup(d);
        d = next;
        rat_scale(fact, k + 1, 1);
    }
    ast_Cleanup(d);
    ast_Cleanup(x);
    ast_Cleanup(z);
    num_Cleanup(fact);
    return ok;
}

/*c3 var^3 + c2 var^2 + c1 var + c0, zero terms left out*/
static pcas_ast_t *cubic_tree(mp_rat *c, Symbol var) {
    pcas_ast_t *sum = ast_MakeOperator(OP_ADD), *t;
    int k;

    for(k = 3; k >= 0; k--) {
        if(mp_rat_compare_zero(c[k]) == 0)
            continue;
        t = k == 0 ? steps_number(c[k]) : ast_MakeSymbol(var);
        if(k > 1)
            t = ast_MakeBinary(OP_POW, t, ast_MakeInt(k));
        if(k > 0 && mp_rat_compare_value(c[k], 1, 1) != 0)
            t = ast_MakeBinary(OP_MULT, steps_number(c[k]), t);
        ast_ChildAppend(sum, t);
    }
    if(ast_ChildLength(sum) == 1) {
        t = ast_ChildRemoveIndex(sum, 0);
        ast_Cleanup(sum);
        return t;
    }
    return sum;
}

/*t = cbrt(-q/2 + sqrt(disc)) + cbrt(-q/2 - sqrt(disc))*/
static pcas_ast_t *cube_root_tail(mp_rat q, mp_rat disc) {
    mp_rat h = num_FromInt(0);
    pcas_ast_t *neg_half, *sd = sqrt_of(steps_number(disc)), *tail;

    mp_rat_copy(q, h);
    rat_scale(h, -1, 2);
    neg_half = steps_number(h);
    num_Cleanup(h);
    tail = ast_MakeBinary(OP_ADD,
                          ast_MakeBinary(OP_ROOT, ast_MakeInt(3), ast_MakeBinary(OP_ADD, ast_Copy(neg_half), ast_Copy(sd))),
                          ast_MakeBinary(OP_ROOT, ast_MakeInt(3), ast_MakeBinary(OP_ADD, ast_Copy(neg_half), ast_MakeBinary(OP_MULT, ast_MakeInt(-1), ast_Copy(sd)))));
    ast_Cleanup(neg_half);
    ast_Cleanup(sd);
    return tail;
}

/*t = 2 sqrt(c) cos(acos(a sqrt(b))/3 - 2 pi k/3) with c = -p/3, b = -3/p and a = 3q/(2p) as numbers*/
static pcas_ast_t *cos_tail(mp_rat p, mp_rat q) {
    mp_rat rc = num_FromInt(0), rb = num_FromInt(0), ra = num_FromInt(0);
    pcas_ast_t *tail;

    mp_rat_copy(p, rc);
    rat_scale(rc, -1, 3);
    mp_rat_set_value(rb, -3, 1);
    mp_rat_div(rb, p, rb);
    mp_rat_copy(q, ra);
    rat_scale(ra, 3, 2);
    mp_rat_div(ra, p, ra);
    tail = ast_MakeBinary(OP_MULT, ast_MakeBinary(OP_MULT, ast_MakeInt(2), sqrt_of(steps_number(rc))),
                          ast_MakeUnary(OP_COS, ast_MakeBinary(OP_ADD,
                              ast_MakeBinary(OP_DIV, ast_MakeUnary(OP_COS_INV, ast_MakeBinary(OP_MULT, steps_number(ra), sqrt_of(steps_number(rb)))), ast_MakeInt(3)),
                              ast_MakeBinary(OP_MULT, ast_MakeInt(-1), ast_MakeBinary(OP_DIV, ast_MakeBinary(OP_MULT, ast_MakeInt(2), ast_MakeBinary(OP_MULT, ast_MakeSymbol(SYM_PI), ast_MakeSymbol(SYM_K))), ast_MakeInt(3))))));
    num_Cleanup(rc);
    num_Cleanup(rb);
    num_Cleanup(ra);
    return tail;
}

/*cubic with no rational root, by hand: square term out, p and q, the discrimant, then the formula. false if it has a rational root or a coefficient isn't a number*/
static bool cardano(steps_t *out, step_t *first, pcas_ast_t *right, pcas_ast_t *f, Symbol var) {
    mp_rat c[4], bb = num_FromInt(0), cc = num_FromInt(0), dd = num_FromInt(0), p = num_FromInt(0), q = num_FromInt(0), disc = num_FromInt(0), t1 = num_FromInt(0), t2 = num_FromInt(0);
    pcas_ast_t *roots[SOLVE_MAX] = {NULL}, *e, *tt, *tail;
    Symbol t = var == SYM_T ? SYM_U : SYM_T;
    step_t *l = first;
    int k, n;
    bool ok = false;

    for(k = 0; k < 4; k++)
        c[k] = num_FromInt(0);
    if(!cubic_numbers(f, var, c) || mp_rat_compare_zero(c[3]) == 0)
        goto done;
    if(!is_ast_int(right, 0)) { /*everything on one side*/
        l->move = '_';
        l->by = ast_Copy(right);
        l = steps_eq(out, cubic_tree(c, var), ast_MakeInt(0));
        if(l == NULL)
            goto done;
    }
    mp_rat_div(c[2], c[3], bb);
    mp_rat_div(c[1], c[3], cc);
    mp_rat_div(c[0], c[3], dd);
    if(mp_rat_compare_value(c[3], 1, 1) != 0) { /*x^3 first*/
        mp_rat one[4];

        one[0] = dd;
        one[1] = cc;
        one[2] = bb;
        one[3] = num_FromInt(1);
        steps_rule(l, "divide by the first number");
        l = steps_eq(out, cubic_tree(one, var), ast_MakeInt(0));
        num_Cleanup(one[3]);
        if(l == NULL)
            goto done;
    }
    /*p = c - b^2/3, q = 2b^3/27 - bc/3 + d*/
    mp_rat_mul(bb, bb, t1);
    rat_scale(t1, 1, 3);
    mp_rat_sub(cc, t1, p);
    mp_rat_mul(bb, bb, t1);
    mp_rat_mul(t1, bb, t1);
    rat_scale(t1, 2, 1);
    rat_scale(t1, 1, 27);
    mp_rat_mul(bb, cc, t2);
    rat_scale(t2, 1, 3);
    mp_rat_sub(t1, t2, t1);
    mp_rat_add(t1, dd, q);
    if(mp_rat_compare_zero(bb) != 0) { /*x = t - b/3 takes the x^2 out*/
        mp_rat red[4];

        red[0] = q;
        red[1] = p;
        red[2] = num_FromInt(0);
        red[3] = num_FromInt(1);
        steps_rule(l, "x = t - b/3 takes the square term out");
        l = steps_eq(out, cubic_tree(red, t), ast_MakeInt(0));
        num_Cleanup(red[2]);
        num_Cleanup(red[3]);
        if(l == NULL)
            goto done;
    }
    /*disc = (q/2)^2 + (p/3)^3*/
    mp_rat_copy(q, t1);
    rat_scale(t1, 1, 2);
    mp_rat_mul(t1, t1, t1);
    mp_rat_copy(p, t2);
    rat_scale(t2, 1, 3);
    mp_rat_mul(t2, t2, disc);
    mp_rat_mul(disc, t2, disc);
    mp_rat_add(disc, t1, disc);
    if(mp_rat_compare_zero(disc) == 0)
        goto done;
    steps_rule(l, "the discriminant, (q/2)^2 + (p/3)^3");
    e = ast_MakeBinary(OP_ADD, ast_MakeBinary(OP_POW, ast_MakeBinary(OP_DIV, steps_number(q), ast_MakeInt(2)), ast_MakeInt(2)),
                       ast_MakeBinary(OP_POW, ast_MakeBinary(OP_DIV, steps_number(p), ast_MakeInt(3)), ast_MakeInt(3)));
    l = steps_expr(out, e);
    if(l == NULL)
        goto done;
    l->as_is = true;
    l = steps_expr(out, steps_number(disc));
    if(l == NULL)
        goto done;
    /*over 0 one real root and two complex ones, under it three real*/
    tt = ast_MakeSymbol(t);
    if(mp_rat_compare_zero(disc) > 0) {
        steps_rule(l, "over 0: one real root and two complex");
        tail = cube_root_tail(q, disc);
    } else {
        steps_rule(l, "under 0: three real roots, k = 0, 1, 2");
        tail = cos_tail(p, q);
    }
    l = steps_eq(out, tt, tail);
    if(l == NULL)
        goto done;
    l->as_is = true;
    /*back to x, all the roots as the solver writes them*/
    steps_rule(l, "x = t - b/3");
    n = solve_ex(f, var, roots, true);
    if(n < 0) /*the complex pair of most cubics isn't found, the real root is*/
        n = solve_ex(f, var, roots, false);
    if(n < 1 || n > STEP_EQS_MAX)
        goto done;
    ok = roots_line(out, var, roots, n);
    for(k = 0; k < n; k++)
        roots[k] = NULL; /*taken or freed*/
done:
    for(k = 0; k < 4; k++)
        num_Cleanup(c[k]);
    num_Cleanup(bb);
    num_Cleanup(cc);
    num_Cleanup(dd);
    num_Cleanup(p);
    num_Cleanup(q);
    num_Cleanup(disc);
    num_Cleanup(t1);
    num_Cleanup(t2);
    for(k = 0; k < SOLVE_MAX; k++)
        ast_Cleanup(roots[k]);
    return ok;
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
    show_poly(l);

    f = ast_MakeBinary(OP_ADD, ast_Copy(left), ast_MakeBinary(OP_MULT, ast_MakeInt(-1), ast_Copy(right)));
    if(f == NULL)
        goto fail;
    simplify(f, LIGHT);
    expand(f, EXP_ALL);
    simplify(f, LIGHT);
    g = ast_Copy(f);
    factor(g, FAC_POLYNOMIAL);
    if(g == NULL || !factors_of(g, var, list, &n)) {
        ok = g != NULL && poly_degree(f, var, NULL) == 3 && cardano(out, l, right, f, var);
        goto done;
    }

    if(!is_ast_int(right, 0)) { /*everything on one side*/
        l->move = '_';
        l->by = ast_Copy(right);
        l = steps_eq(out, ast_Copy(f), ast_MakeInt(0));
        if(l == NULL)
            goto done;
        show_poly(l);
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
