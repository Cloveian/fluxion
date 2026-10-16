/*
    Fluxion: asym(), not in PineappleCAS (the nspire hasn't got one either, this is
    the "where does the graph break" question off a precalc worksheet).

    f = top/bottom in one variable. each zero of the bottom is a hole if the top is
    zero there at least as many times over (multiplicity, counted with derivatives),
    otherwise a vertical asymptote. the horizontal one comes from the degrees, top
    lower gives y = 0 and the same degree gives lead/lead. top higher has none, it'd
    be a slant one instead and those are a todo

    the top and bottom are read straight off the tree as typed, since simplify
    would cancel the (x-1) out of (x^2-1)/(x-1) and the hole with it. anything
    that isn't one fraction gets put over a common denominator first.
*/

#include <string.h>

#include "cas.h"

#define SIMP_ASYM (SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_RATIONAL | SIMP_EVAL | SIMP_LIKE_TERMS)

/*p with var = x, is it 0. no identity passes, so sqrt(2)^2 - 2 might slip through
  as not zero (hole missed, shows as vertical). rare enough for now*/
static bool zero_at(pcas_ast_t *p, Symbol var, pcas_ast_t *x) {
    pcas_ast_t *e = ast_Copy(p), *v = ast_MakeSymbol(var);
    bool zero;

    if(e == NULL || v == NULL) {
        ast_Cleanup(e);
        ast_Cleanup(v);
        return false;
    }
    substitute(e, v, x);
    simplify(e, SIMP_ASYM);
    zero = is_ast_int(e, 0);
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

void asym_free(asym_t *a) {
    int i;

    for(i = 0; i < a->nholes; i++)
        ast_Cleanup(a->holes[i]);
    for(i = 0; i < a->nvert; i++)
        ast_Cleanup(a->vert[i]);
    ast_Cleanup(a->horiz);
    memset(a, 0, sizeof *a);
}

bool asym(pcas_ast_t *f, Symbol var, asym_t *out) {
    pcas_ast_t *top, *bot, *lead_t = NULL, *lead_b = NULL;
    pcas_ast_t *zeros[SOLVE_MAX];
    int n, i, dt, db;

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
            bot = ast_MakeNumber(num_FromInt(1));
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
        if(mult_at(top, var, zeros[i]) >= mult_at(bot, var, zeros[i]))
            out->holes[out->nholes++] = zeros[i];
        else
            out->vert[out->nvert++] = zeros[i];
        zeros[i] = NULL;
    }

    dt = poly_degree(top, var, &lead_t);
    db = poly_degree(bot, var, &lead_b);
    if(dt >= 0 && db >= 0 && dt < db) {
        out->horiz = ast_MakeNumber(num_FromInt(0));
    } else if(dt >= 0 && dt == db) {
        out->horiz = ast_MakeBinary(OP_DIV, lead_t, lead_b);
        lead_t = lead_b = NULL;
        simplify(out->horiz, SIMP_ASYM);
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
