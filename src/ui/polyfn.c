#include "polyfn.h"

#include "app.h"
#include "args.h"
#include "settings.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "engine/ast.h"
#include "engine/cas/cas.h"
#include "render/draw.h"

/*
    fromroots(, inverse(, apart( and divide(, run by the app like geom's. same deal: the line gets
    cut at the top level commas and each piece parsed on its own. roots( isn't
    here, it's just solve( with a different name so app.c sends it to run_solve
*/

#define ARGS_MAX 12

#define SIMP_BASIC (SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_RATIONAL)

/* a letter on its own (or theta), so it can be the variable */
static bool is_var(const pcas_ast_t *e) {
    return e->type == NODE_SYMBOL
           && ((e->op.symbol >= SYM_A && e->op.symbol <= SYM_Z) || e->op.symbol == SYM_THETA);
}

/* var - r. no subtract node, it's a plus the negative */
static pcas_ast_t *factor_of(Symbol var, pcas_ast_t *r) {
    return ast_MakeBinary(OP_ADD, ast_MakeSymbol((char)var),
                          ast_MakeBinary(OP_MULT, ast_MakeInt(-1), r));
}

/* (x - r1)(x - r2)... multiplied out the way expand( does it */
static box_t *from_roots(pcas_ast_t **a, int n) {
    pcas_ast_t *e;
    Symbol var = SYM_X;
    box_t *out;
    int i;

    /* a letter at the end is the variable, unless it's the only thing there */
    if(n > 1 && is_var(a[n - 1]))
        var = a[--n]->op.symbol;

    e = factor_of(var, ast_Copy(a[0]));
    for(i = 1; i < n; i++)
        e = ast_MakeBinary(OP_MULT, e, factor_of(var, ast_Copy(a[i])));

    simplify(e, SIMP_BASIC);
    expand(e, EXP_ALL);
    simplify(e, SIMP_BASIC | SIMP_EVAL | SIMP_LIKE_TERMS);
    /* not value_box, that puts it over one denominator and (2x^2-7x+3)/2 isn't
       what expand( would say. rationalize is the only canon bit the app flips */
    simplify_canonical_form(e, app_rationalize() ? CANONICAL_ALL : CANONICAL_ALL & ~CANONICAL_RATIONALIZE);
    out = pcas_cancelled ? NULL : layout_build(e, &draw_font);
    ast_Cleanup(e);
    return out;
}

/* solve y = f(x) for x, then call y x again. y is just the usual letter, if f
   already has one some other letter stands in. not n, that's the trig n */
static box_t *inverse_of(pcas_ast_t *f, Symbol var, const char **message) {
    pcas_ast_t *roots[SOLVE_MAX] = { NULL }, *e, *t, *x;
    Symbol tmp = SYM_Y;
    box_t *out = NULL;
    int n, i;

    if(var == SYM_Y || has_symbol(f, SYM_Y))
        for(tmp = SYM_A; tmp <= SYM_Z; tmp++)
            if(tmp != var && tmp != SYM_N && !has_symbol(f, tmp))
                break;
    if(tmp > SYM_Z) {
        *message = "no letter left over to use";
        return NULL;
    }

    e = ast_Copy(f);
    if(setting_angle == ANGLE_DEGREES)
        e = degrees_in(e);
    e = ast_MakeBinary(OP_ADD, e, ast_MakeBinary(OP_MULT, ast_MakeInt(-1),
                                                 ast_MakeSymbol((char)tmp)));
    n = solve_ex(e, var, roots, false);
    ast_Cleanup(e);

    if(pcas_cancelled) {
        for(i = 0; i < n; i++)
            ast_Cleanup(roots[i]);
        return NULL;
    }
    /* SOLVE_ALL too, f didn't have x in it so there's nothing to undo */
    if(n <= 0) {
        *message = "no inverse found";
        return NULL;
    }

    t = ast_MakeSymbol((char)tmp);
    x = ast_MakeSymbol((char)var);
    for(i = 0; i < n; i++) {
        substitute(roots[i], t, x);
        simplify(roots[i], SIMP_ALL);
        if(setting_angle == ANGLE_DEGREES)
            degrees_out(roots[i]);
    }
    ast_Cleanup(t);
    ast_Cleanup(x);

    if(!pcas_cancelled)
        out = values_box(roots, n); /* no x = in front, they're functions of x and x = sqrt(x) would be nonsense */
    for(i = 0; i < n; i++)
        ast_Cleanup(roots[i]);
    return out;
}

/* a / b as a sum of simpler fractions: the whole part from the long *dih*vision, the rest over the roots of b. NULL (and message) when it can't */
static box_t *apart_of(pcas_ast_t *f, Symbol var, const char **message) {
    pcas_ast_t *e = ast_Copy(f), *top, *bottom, *q = NULL, *r = NULL, *sum = NULL, *whole = NULL, *t;
    box_t *out = NULL;
    bool exact;

    if(setting_angle == ANGLE_DEGREES)
        e = degrees_in(e);
    simplify(e, SIMP_ALL);
    comdenom(e, true);
    if(!isoptype(e, OP_DIV) || !has_symbol(ast_ChildGet(e, 1), var)) {
        ast_Cleanup(e);
        *message = "not a fraction with x on the bottom";
        return NULL;
    }
    top = ast_ChildGet(e, 0);
    bottom = ast_ChildGet(e, 1);
    if(poly_degree(top, var, NULL) >= poly_degree(bottom, var, NULL)) {
        q = poly_quotient(top, bottom, var, &exact);
        r = q != NULL ? poly_remainder(top, bottom, var) : NULL;
        if(q == NULL || r == NULL) {
            *message = "the top and bottom have to be polynomials";
            goto done;
        }
        top = r;
        whole = q;
    }
    if(is_ast_int(top, 0)) { /*it divides evenly, there's no fraction left*/
        sum = ast_Copy(whole);
    } else if((sum = partial_sum(top, bottom, var)) == NULL) {
        *message = "the bottom needs rational roots, all different";
        goto done;
    } else if(whole != NULL) {
        sum = ast_MakeBinary(OP_ADD, ast_Copy(whole), sum);
    }
    if(isoptype(sum, OP_ADD)) /*a term at a time, all of it at once puts the fractions back over one bottom*/
        for(t = ast_ChildGet(sum, 0); t != NULL; t = t->next)
            simplify(t, SIMP_ALL);
    else
        simplify(sum, SIMP_ALL);
    simplify(sum, SIMP_NORMALIZE | SIMP_COMMUTATIVE);
    simplify_canonical_form(sum, CANONICAL_ALL);
    if(setting_angle == ANGLE_DEGREES)
        degrees_out(sum);
    out = pcas_cancelled ? NULL : layout_build(sum, &draw_font);
done:
    ast_Cleanup(e);
    ast_Cleanup(q);
    ast_Cleanup(r);
    ast_Cleanup(sum);
    return out;
}

/* divide(p, q, x): the quotient and what's left, p = quotient q + remainder */
static box_t *divide_of(pcas_ast_t *p, pcas_ast_t *q, Symbol var, const char **message) {
    pcas_ast_t *top = ast_Copy(p), *bottom = ast_Copy(q), *quo, *rem;
    box_t *col = NULL;
    bool exact;

    simplify(top, SIMP_ALL);
    simplify(bottom, SIMP_ALL);
    quo = poly_quotient(top, bottom, var, &exact);
    rem = quo != NULL ? poly_remainder(top, bottom, var) : NULL;
    if(quo == NULL || rem == NULL) {
        *message = "both have to be polynomials in x with numbers for coefficients";
    } else {
        simplify(quo, SIMP_ALL);
        simplify(rem, SIMP_ALL);
        simplify_canonical_form(quo, CANONICAL_ALL);
        simplify_canonical_form(rem, CANONICAL_ALL);
        col = layout_box(BOX_COL, 0);
        layout_add(col, labelled("quotient", layout_build(quo, &draw_font)));
        layout_add(col, labelled("remainder", layout_build(rem, &draw_font)));
        layout_finish(col, &draw_font);
    }
    ast_Cleanup(top);
    ast_Cleanup(bottom);
    ast_Cleanup(quo);
    ast_Cleanup(rem);
    return col;
}

enum { K_FROMROOTS, K_INVERSE, K_APART, K_DIVIDE, K_COUNT };
static const char *const names[] = { "fromroots", "inverse", "apart", "divide" };
static const char *const usages[] = { "fromroots(root, root, ..., x)", "inverse(f, x)", "apart(f, x)", "divide(p, q, x)" };

bool polyfn_run(const char *cmd, char *src, box_t *in) {
    pcas_ast_t *a[ARGS_MAX] = { NULL };
    pcas_error_t err = E_SUCCESS;
    const char *message = NULL, *usage;
    box_t *out = NULL;
    clock_t start;
    int kind, n, i;

    if(cmd == NULL)
        return false;
    for(kind = 0; kind < K_COUNT; kind++)
        if(strcmp(cmd, names[kind]) == 0)
            break;
    if(kind == K_COUNT)
        return false;
    usage = usages[kind];

    n = args_read(src, a, ARGS_MAX, &err);
    if(n < 1 || ((kind == K_INVERSE || kind == K_APART) && (n > 2 || (n == 2 && !is_var(a[1]))))
       || (kind == K_DIVIDE && (n < 2 || n > 3 || (n == 3 && !is_var(a[2]))))) {
        for(i = 0; i < n; i++)
            ast_Cleanup(a[i]);
        push(in, NULL, n < 0 ? error_text[err] : usage, 0);
        return true;
    }

    start = clock();
    if(kind == K_INVERSE)
        out = inverse_of(a[0], n == 2 ? a[1]->op.symbol : SYM_X, &message);
    else if(kind == K_APART)
        out = apart_of(a[0], n == 2 ? a[1]->op.symbol : SYM_X, &message);
    else if(kind == K_DIVIDE)
        out = divide_of(a[0], a[1], n == 3 ? a[2]->op.symbol : SYM_X, &message);
    else
        out = from_roots(a, n);

    if(pcas_cancelled && out == NULL)
        message = pcas_oom ? "out of memory" : "cancelled";
    push(in, out, message, (unsigned long)((clock() - start) * 1000 / CLOCKS_PER_SEC));

    for(i = 0; i < n; i++)
        ast_Cleanup(a[i]);
    return true;
}
