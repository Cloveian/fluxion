/*Fluxion: is a statement like 23x = 22x or x + 1 > x true, for every value of its letters. not in upstream PineappleCAS.*/

#include <math.h>
#include "cas.h"

#define SAMPLES 24 /*points tried, a letter a number from the table*/
#define NUMERIC_MIN 8 /*points that have to be defined and agree before a numeric yes*/
#define TOL 1e-4 /*approx.c is a float, 24 bits*/

/*the numbers the letters get, num/den*/
static const int pick[12][2] = { {1, 1}, {2, 1}, {-1, 1}, {3, 1}, {1, 2}, {-2, 1}, {5, 1}, {0, 1}, {3, 2}, {-1, 2}, {7, 1}, {10, 1} };

static bool lacks_letters(pcas_ast_t *e) {
    Symbol c;

    for(c = SYM_A; c <= SYM_Z; c++)
        if(has_symbol(e, c))
            return false;
    return true;
}

/*rel holds for these two numbers, a bit of flaot slack, a tie isn't < or >*/
static bool holds(char rel, double l, double r) {
    double tol = TOL * (1 + fabs(l) + fabs(r));

    switch(rel) {
    case '<': return l < r - tol;
    case '>': return l > r + tol;
    case 'l': return l <= r + tol;
    case 'g': return l >= r - tol;
    default: return fabs(l - r) <= tol;
    }
}

/*what a number with no letters says about rel against 0, TRUTH_UNKNOWN when it can't tell (a surd that's a hair off 0)*/
static int decide(char rel, pcas_ast_t *d) {
    double v;

    if(is_ast_int(d, 0))
        return rel == '=' || rel == 'l' || rel == 'g' ? TRUTH_TRUE : TRUTH_FALSE;
    if(!approx_double(d, &v) || fabs(v) < 1e-6)
        return TRUTH_UNKNOWN;
    switch(rel) {
    case '<': case 'l': return v < 0 ? TRUTH_TRUE : TRUTH_FALSE;
    case '>': case 'g': return v > 0 ? TRUTH_TRUE : TRUTH_FALSE;
    default: return TRUTH_FALSE; /*not 0*/
    }
}

/*e with every letter in it a number from the table, k picks which: NULL when oom*/
static pcas_ast_t *at_point(pcas_ast_t *e, const Symbol *vars, int n, int k, int *num, int *den) {
    pcas_ast_t *c = ast_Copy(e), *x, *v;
    int j, idx;

    for(j = 0; j < n && c != NULL; j++) {
        idx = (k * (j + 1) + 7 * j) % (int)(sizeof pick / sizeof pick[0]);
        num[j] = pick[idx][0];
        den[j] = pick[idx][1];
        x = ast_MakeSymbol(vars[j]);
        v = den[j] == 1 ? ast_MakeInt(num[j]) : ast_MakeBinary(OP_DIV, ast_MakeInt(num[j]), ast_MakeInt(den[j]));
        if(x == NULL || v == NULL) {
            ast_Cleanup(x);
            ast_Cleanup(v);
            ast_Cleanup(c);
            return NULL;
        }
        substitute(c, x, v);
        ast_Cleanup(x);
        ast_Cleanup(v);
    }
    return c;
}

/*the route that shows d = 0 when simplify alone didn't, NULL for none*/
static const char *prove_zero(pcas_ast_t *left, pcas_ast_t *right, pcas_ast_t *d) {
    const char *route = NULL;
    pcas_ast_t *w = ast_Copy(d), *l, *r;

    expand(w, EXP_ALL);
    simplify(w, SIMP_ALL);
    if(is_ast_int(w, 0))
        route = "expand";
    ast_Cleanup(w);
    if(route != NULL)
        return route;

    /*each side on its own, they can end up the same without the *dih*ference showing it*/
    l = ast_Copy(left);
    r = ast_Copy(right);
    simplify(l, SIMP_ALL);
    simplify(r, SIMP_ALL);
    simplify_canonical_form(l, CANONICAL_ALL);
    simplify_canonical_form(r, CANONICAL_ALL);
    if(!pcas_cancelled && ast_Compare(l, r))
        route = "same-sides";
    ast_Cleanup(l);
    ast_Cleanup(r);
    if(route != NULL)
        return route;

    /*over one bottom, the top may cancel to 0*/
    w = ast_Copy(d);
    comdenom(w, false);
    simplify(w, SIMP_ALL);
    if(!pcas_cancelled && is_ast_int(w, 0))
        route = "one-bottom";
    ast_Cleanup(w);
    return route;
}

/*a counterexample says false, no matter what the algebra got to. all points that hold say true, but only numericaly*/
static int sample(pcas_ast_t *left, pcas_ast_t *right, char rel, truth_t *t) {
    pcas_ast_t *l, *r;
    Symbol c;
    int agree = 0, k, j, num[3], den[3];
    double lv, rv;

    for(c = SYM_A; c <= SYM_Z && t->nvars < 3; c++)
        if(has_symbol(left, c) || has_symbol(right, c))
            t->var[t->nvars++] = c;
    for(k = 0; k < SAMPLES && !pcas_cancelled; k++) {
        l = at_point(left, t->var, t->nvars, k, num, den);
        r = at_point(right, t->var, t->nvars, k, num, den);
        if(l != NULL && r != NULL && approx_double(l, &lv) && approx_double(r, &rv)) {
            if(!holds(rel, lv, rv)) {
                for(j = 0; j < t->nvars; j++) {
                    t->num[j] = num[j];
                    t->den[j] = den[j];
                }
                ast_Cleanup(l);
                ast_Cleanup(r);
                t->route = "counterexample";
                return TRUTH_FALSE;
            }
            agree++;
        }
        ast_Cleanup(l);
        ast_Cleanup(r);
    }
    if(!pcas_cancelled && agree >= NUMERIC_MIN) {
        t->points = agree;
        t->route = "numerically";
        return TRUTH_TRUE;
    }
    return TRUTH_UNKNOWN;
}

int truth_of(pcas_ast_t *left, pcas_ast_t *right, char rel, truth_t *t) {
    pcas_ast_t *d = ast_MakeBinary(OP_ADD, ast_Copy(left), ast_MakeBinary(OP_MULT, ast_MakeInt(-1), ast_Copy(right))), *w;
    const char *route = NULL;
    int result;

    t->route = NULL;
    t->points = 0;
    t->nvars = 0;
    if(d == NULL)
        return TRUTH_UNKNOWN;
    w = ast_Copy(d);
    simplify(w, SIMP_ALL);
    if(pcas_cancelled) {
        ast_Cleanup(w);
        ast_Cleanup(d);
        return TRUTH_UNKNOWN;
    }
    if(lacks_letters(w)) { /*2 + 3 = 5, x - x < 1: one number*/
        result = decide(rel, w);
        t->route = lacks_letters(d) ? "numbers" : "simplify";
        ast_Cleanup(w);
        ast_Cleanup(d);
        return result;
    }
    if(rel == '=') /*proof, the cheap ones first*/
        route = is_ast_int(w, 0) ? "simplify" : prove_zero(left, right, d);
    ast_Cleanup(w);
    ast_Cleanup(d);
    if(pcas_cancelled)
        return TRUTH_UNKNOWN;
    if(route != NULL) {
        t->route = route;
        return TRUTH_TRUE;
    }
    return sample(left, right, rel, t);
}
