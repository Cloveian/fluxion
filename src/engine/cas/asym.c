/*
    Fluxion: asym(), not in PineappleCAS (the nspire hasn't got one either, this is
    the "where does the graph break" question off a precalc worksheet).

    f = top/bottom in one variable. each zero of the bottom is a hole if the top is
    zero there at least as many times over (multiplicity, counted with derivatives),
    otherwise a vertical asymptote. the horizontal one comes from the degrees, top
    lower gives y = 0 and the same degree gives lead/lead. top one higher gives a
    slant one instead, y = the quotient of top/bottom. higher than that has neither

    the top and bottom are read straight off the tree as typed, since simplify
    would cancel the (x-1) out of (x^2-1)/(x-1) and the hole with it. anything
    that isn't one fraction gets put over a common denominator first.
*/

#include <string.h>

#include "cas.h"

#define SIMP_ASYM (SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_RATIONAL | SIMP_EVAL | SIMP_LIKE_TERMS)

/*p with var = x, is it 0. no identity passes, so e^ln(5) - 5 comes out as a number
  that is 0 and nothing else says so, that's what the second look is for*/
static bool zero_at(pcas_ast_t *p, Symbol var, pcas_ast_t *x) {
    pcas_ast_t *e = ast_Copy(p), *v = ast_MakeSymbol(var);
    double d;
    bool zero;

    if(e == NULL || v == NULL) {
        ast_Cleanup(e);
        ast_Cleanup(v);
        return false;
    }
    substitute(e, v, x);
    simplify(e, SIMP_ASYM);
    zero = is_ast_int(e, 0) || (approx_double(e, &d) && d > -1e-5 && d < 1e-5); /*float *dih*gits*/
    ast_Cleanup(e);
    ast_Cleanup(v);
    return zero;
}

/*how many times x is a root of p, by differentiating while it stays zero
  there. capped, nobody has an (x-1)^9 on their worksheet*/
static int mult_at(pcas_ast_t *p, Symbol var, pcas_ast_t *x) {
    pcas_ast_t *e = ast_Copy(p), *v = ast_MakeSymbol(var);
    int m = 0;

    if(e == NULL || v == NULL) {
        ast_Cleanup(e);
        ast_Cleanup(v);
        return 0;
    }
    while(m < 8 && !pcas_cancelled && zero_at(e, var, x)) {
        derivative(e, v, v);
        simplify(e, SIMP_ASYM);
        m++;
    }
    ast_Cleanup(e);
    ast_Cleanup(v);
    return m;
}

/*the y f heads to as var goes to inf or -inf. NULL when that isn't a number*/
static pcas_ast_t *toward_inf(pcas_ast_t *f, Symbol var, bool negative) {
    pcas_ast_t *e = ast_MakeOperator(OP_LIMIT), *inf = ast_MakeSymbol(SYM_INF);
    double v;

    ast_ChildAppend(e, ast_Copy(f));
    ast_ChildAppend(e, ast_MakeSymbol(var));
    ast_ChildAppend(e, negative ? ast_MakeBinary(OP_MULT, ast_MakeInt(-1), inf) : inf);
    simplify(e, SIMP_ALL);
    if(pcas_cancelled || !approx_double(e, &v)) {
        ast_Cleanup(e);
        return NULL;
    }
    return e;
}

int asym_ends(const asym_t *a, pcas_ast_t **out) {
    int n = 0;

    if(a->horiz != NULL)
        out[n++] = a->horiz;
    if(a->left != NULL)
        out[n++] = a->left;
    if(a->right != NULL)
        out[n++] = a->right;
    return n;
}

/*f at x + h as a number, false when it isn't one*/
static bool value_near(pcas_ast_t *f, Symbol var, pcas_ast_t *x, int h, double *v) {
    pcas_ast_t *e = ast_Copy(f), *s = ast_MakeSymbol(var);
    pcas_ast_t *at = ast_MakeBinary(OP_ADD, ast_Copy(x), ast_MakeBinary(OP_DIV, ast_MakeInt(h), ast_MakeInt(1000)));
    bool ok = false;

    if(e != NULL && s != NULL && at != NULL) {
        substitute(e, s, at);
        ok = !pcas_cancelled && approx_double(e, v);
    }
    ast_Cleanup(e);
    ast_Cleanup(s);
    ast_Cleanup(at);
    return ok;
}

/*domain's families of holes (tan's pi/2 + pi n): vertical where f is far off just beside the first one (n = 0), holes where it isn't*/
static void family_holes(pcas_ast_t *f, Symbol var, asym_t *out) {
    domain_t d;
    pcas_ast_t *n0, *first, *nv;
    double lo, hi;
    int i;

    if(!domain(f, var, &d)) {
        domain_free(&d);
        return;
    }
    for(i = 0; i < d.nholes && !pcas_cancelled; i++) {
        if(!has_symbol(d.holes[i], SYM_N))
            continue;
        first = ast_Copy(d.holes[i]);
        n0 = ast_MakeInt(0);
        nv = ast_MakeSymbol(SYM_N);
        if(first != NULL && n0 != NULL && nv != NULL)
            substitute(first, nv, n0);
        ast_Cleanup(n0);
        ast_Cleanup(nv);
        if(first != NULL && value_near(f, var, first, 1, &lo) && value_near(f, var, first, -1, &hi)) {
            if(lo > 100 || lo < -100 || hi > 100 || hi < -100) {
                if(out->nvert < SOLVE_MAX)
                    out->vert[out->nvert++] = ast_Copy(d.holes[i]);
            } else if(out->nholes < SOLVE_MAX) {
                out->holes[out->nholes++] = ast_Copy(d.holes[i]);
            }
        }
        ast_Cleanup(first);
    }
    domain_free(&d);
}

void asym_free(asym_t *a) {
    int i;

    for(i = 0; i < a->nholes; i++)
        ast_Cleanup(a->holes[i]);
    for(i = 0; i < a->nvert; i++)
        ast_Cleanup(a->vert[i]);
    ast_Cleanup(a->left);
    ast_Cleanup(a->right);
    ast_Cleanup(a->horiz);
    ast_Cleanup(a->slant);
    memset(a, 0, sizeof *a);
}

bool asym(pcas_ast_t *f, Symbol var, asym_t *out) {
    pcas_ast_t *top, *bot, *lead_t = NULL, *lead_b = NULL;
    pcas_ast_t *zeros[SOLVE_MAX];
    int n, i, dt, db;
    bool exact, families = false;

    memset(out, 0, sizeof *out);
    for(i = 0; i < SOLVE_MAX; i++)
        zeros[i] = NULL;
    if(f == NULL || pcas_cancelled)
        return false;

    if(isoptype(f, OP_DIV)) {
        top = ast_Copy(ast_ChildGet(f, 0));
        bot = ast_Copy(ast_ChildGet(f, 1));
    } else {
        pcas_ast_t *e = ast_Copy(f);

        if(e == NULL)
            return false;
        simplify(e, SIMP_ASYM);
        comdenom(e, false);
        simplify(e, SIMP_ASYM);
        if(isoptype(e, OP_DIV)) {
            top = ast_Copy(ast_ChildGet(e, 0));
            bot = ast_Copy(ast_ChildGet(e, 1));
            ast_Cleanup(e);
        } else { /*a polynomial or so, nothing under it*/
            top = e;
            bot = ast_MakeInt(1);
        }
    }
    if(top == NULL || bot == NULL || pcas_cancelled) {
        ast_Cleanup(top);
        ast_Cleanup(bot);
        return false;
    }

    /*zeros of the bottom, sorted already. SOLVE_ALL means the bottom is just 0*/
    n = solve(bot, var, zeros);
    if(n < 0 || pcas_cancelled) {
        ast_Cleanup(top);
        ast_Cleanup(bot);
        return false;
    }
    for(i = 0; i < n; i++) {
        if(has_symbol(zeros[i], SYM_N)) { /*a whole family, family_holes judges those*/
            ast_Cleanup(zeros[i]);
            families = true;
        } else if(mult_at(top, var, zeros[i]) >= mult_at(bot, var, zeros[i])) {
            out->holes[out->nholes++] = zeros[i];
        } else {
            out->vert[out->nvert++] = zeros[i];
        }
        zeros[i] = NULL;
    }
    if(families || has_operator(f, OP_TAN) || has_operator(f, OP_SEC) || has_operator(f, OP_CSC) || has_operator(f, OP_COT))
        family_holes(f, var, out);

    dt = poly_degree(top, var, &lead_t);
    db = poly_degree(bot, var, &lead_b);
    if(dt < 0 || db < 0) { /*not a polynomial fraction, the ends have to come from limits*/
        out->left = toward_inf(f, var, true);
        out->right = toward_inf(f, var, false);
        if(out->left != NULL && out->right != NULL && ast_Compare(out->left, out->right)) {
            out->horiz = out->left;
            out->left = NULL;
            ast_Cleanup(out->right);
            out->right = NULL;
        }
    } else if(dt < db) {
        out->horiz = ast_MakeInt(0);
    } else if(dt == db) {
        out->horiz = ast_MakeBinary(OP_DIV, lead_t, lead_b);
        lead_t = lead_b = NULL;
        simplify(out->horiz, SIMP_ASYM);
    } else if(dt == db + 1) {
        /*the remainder over the bottom goes to 0 out there, the quotient is what's left.
          NULL from poly_quotient (a letter that isn't var) just means no line shown.
          no remainder means f is that line with holes in it, nothing to approach*/
        out->slant = poly_quotient(top, bot, var, &exact);
        if(out->slant != NULL && exact) {
            ast_Cleanup(out->slant);
            out->slant = NULL;
        }
        if(out->slant != NULL)
            simplify(out->slant, SIMP_ASYM);
    }
    ast_Cleanup(lead_t);
    ast_Cleanup(lead_b);
    ast_Cleanup(top);
    ast_Cleanup(bot);

    if(pcas_cancelled) { /*half of this isn't an answer*/
        asym_free(out);
        return false;
    }
    return true;
}
