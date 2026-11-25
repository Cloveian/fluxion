/*Fluxion: steps for inequalities, a sign along for a line and a sign chart for the rest. not in upstream PineappleCAS.*/

#include <math.h>
#include <stdlib.h>

#include "steps.h"

#define EDGES_MAX (STEP_EQS_MAX + 2)
#define HUGE_X 1e300 /*no isinf in C89*/
#define X_MAX 1e6 /*a zero past this and the test points don't fit a long*/

static bool is_plain(double x) {
    return x == x && x < HUGE_X && x > -HUGE_X;
}

typedef struct {
    pcas_ast_t *at;
    double x;
    bool hole;
} edge_t;

static char flip(char rel) {
    switch(rel) {
    case '<': return '>';
    case '>': return '<';
    case 'l': return 'g';
    case 'g': return 'l';
    default: return rel;
    }
}

static bool closed(char rel) {
    return rel == 'l' || rel == 'g';
}

static bool wanted(int sign, char rel) {
    return rel == '<' || rel == 'l' ? sign < 0 : sign > 0;
}

/*the lines of an equation, each with the sign. dividing by a negative turns it, and so does writing x on the other side*/
static void sign_along(steps_t *s, char rel, Symbol var) {
    char cur = rel;
    int i, prev = 0;

    for(i = 0; i < s->n; i++) {
        step_t *l = &s->line[i];
        step_eq_t *q = &l->eq[0];
        bool lx = has_symbol(q->left, var), rx = q->right != NULL && has_symbol(q->right, var);
        int dir = lx && !rx ? 1 : !lx && rx ? -1 : 0;

        if(dir != 0 && prev != 0 && dir != prev)
            cur = flip(cur);
        if(dir != 0)
            prev = dir;
        q->rel = cur;
        if((l->move == '/' || l->move == '*') && l->by != NULL && is_negative_for_sure(l->by)) {
            cur = flip(cur);
            l->flip = true;
        }
    }
}

/*the sign of v, -1 0 1, 2 when it isn't a number*/
static int sign_of(pcas_ast_t *v) {
    double x;

    if(!approx_double(v, &x) || !is_plain(x))
        return 2;
    return x > 0 ? 1 : x < 0 ? -1 : 0;
}

/*the real zeros of e as edges, one that's there already just gets hole. false for a family, a letter, or too many*/
static bool add_edges(pcas_ast_t *e, Symbol var, bool hole, edge_t *edges, int *n) {
    pcas_ast_t *roots[SOLVE_MAX] = {NULL};
    bool ok = true;
    int got, i, k;

    if(!has_symbol(e, var))
        return true;
    got = solve(e, var, roots);
    if(got < 0)
        return false;
    for(i = 0; i < got && ok; i++) {
        double x;

        ok = approx_double(roots[i], &x) && fabs(x) <= X_MAX;
        for(k = 0; ok && k < *n && fabs(edges[k].x - x) > 1e-6 * (1 + fabs(x)); k++)
            ;
        if(!ok)
            break;
        if(k < *n) {
            edges[k].hole |= hole;
        } else if(*n == EDGES_MAX) {
            ok = false;
        } else {
            edges[*n].at = ast_Copy(roots[i]);
            edges[*n].x = x;
            edges[*n].hole = hole;
            (*n)++;
        }
    }
    for(i = 0; i < got; i++)
        ast_Cleanup(roots[i]);
    return ok;
}

static int by_x(const void *a, const void *b) {
    double x = ((const edge_t *)a)->x, y = ((const edge_t *)b)->x;

    return x < y ? -1 : x > y;
}

/*a number in the gap before edge i (i = n is the one past the last), a whole one when there is one, else some p/2^m*/
static pcas_ast_t *test_point(const edge_t *edges, int n, int i) {
    double lo, hi;
    long k0, k1, k, den = 2, p;

    if(i == 0) {
        k = (long)ceil(edges[0].x) - 1;
        return ast_MakeInt((mp_small)(k < 0 ? k : 0));
    }
    if(i == n) {
        k = (long)floor(edges[n - 1].x) + 1;
        return ast_MakeInt((mp_small)(k > 0 ? k : 0));
    }
    lo = edges[i - 1].x;
    hi = edges[i].x;
    k0 = (long)floor(lo) + 1;
    k1 = (long)ceil(hi) - 1;
    if(k0 <= k1) {
        k = k0 <= 0 && 0 <= k1 ? 0 : k0 > 0 ? k0 : k1;
        return ast_MakeInt((mp_small)k);
    }
    for(; den < (1L << 22); den *= 2) {
        p = (long)floor(lo * den) + 1;
        if((double)p / den < hi)
            break;
    }
    return ast_MakeBinary(OP_DIV, ast_MakeInt((mp_small)p), ast_MakeInt((mp_small)den));
}

/*the chart as one list: gap 0, edge 0, gap 1, ..., gap n. even p is a gap*/
static bool truth(const bool *gap, const bool *edge, int p) {
    return p % 2 == 0 ? gap[p / 2] : edge[p / 2];
}

/*the peices the true runs make, in line l. false past the room a line has*/
static bool pieces(step_t *l, Symbol var, const edge_t *edges, int n, const bool *gap, const bool *edge) {
    int p, q, k = 0;

    for(p = 0; p <= 2 * n; p++) {
        step_eq_t *e = &l->eq[k];
        pcas_ast_t *lo = NULL, *hi = NULL;
        bool lo_in = false, hi_in = false;

        if(!truth(gap, edge, p))
            continue;
        for(q = p; q + 1 <= 2 * n && truth(gap, edge, q + 1); q++)
            ;
        if(p > 0) {
            lo = edges[p % 2 == 0 ? p / 2 - 1 : p / 2].at;
            lo_in = p % 2 == 1;
        }
        if(q < 2 * n) {
            hi = edges[q / 2].at;
            hi_in = q % 2 == 1;
        }
        if(lo == NULL && hi == NULL)
            return false;
        if(k == STEP_EQS_MAX)
            return false;
        e->left = ast_MakeSymbol(var);
        if(lo != NULL && lo == hi) {
            e->right = ast_Copy(lo);
        } else if(hi == NULL) {
            e->rel = lo_in ? 'g' : '>';
            e->right = ast_Copy(lo);
        } else {
            e->rel = hi_in ? 'l' : '<';
            e->right = ast_Copy(hi);
            if(lo != NULL) {
                e->low = ast_Copy(lo);
                e->low_rel = lo_in ? 'l' : '<';
            }
        }
        k++;
        p = q;
    }
    l->eqs = k;
    return k > 0;
}

/*a line of var = each edge that is (or isn't) a hole*/
static step_t *zeros_line(steps_t *out, Symbol var, const edge_t *edges, int n, bool hole) {
    step_t *l = NULL;
    int i, k = 0;

    for(i = 0; i < n; i++) {
        if(edges[i].hole != hole)
            continue;
        if(l == NULL) {
            l = steps_eq(out, ast_MakeSymbol(var), ast_Copy(edges[i].at));
            if(l == NULL)
                return NULL;
        } else {
            l->eq[k].left = ast_MakeSymbol(var);
            l->eq[k].right = ast_Copy(edges[i].at);
            l->eqs = k + 1;
        }
        k++;
    }
    if(l != NULL)
        l->list = true;
    return l;
}

/*f rel 0 factored, where its top and bottom are 0, a test point in every gap, the pieces that come out*/
static bool chart(pcas_ast_t *left, pcas_ast_t *right, char rel, Symbol var, steps_t *out) {
    edge_t edges[EDGES_MAX];
    bool gap[EDGES_MAX + 1], edge[EDGES_MAX];
    pcas_ast_t *f = NULL, *g = NULL, *top, *bottom = NULL;
    step_t *l;
    int n = 0, i;
    bool ok = false;

    l = steps_eq(out, ast_Copy(left), ast_Copy(right));
    if(l == NULL)
        return false;
    simplify(l->eq[0].left, SIMP_NORMALIZE | SIMP_EVAL);
    simplify(l->eq[0].right, SIMP_NORMALIZE | SIMP_EVAL);
    l->eq[0].rel = rel;

    f = ast_MakeBinary(OP_ADD, ast_Copy(left), ast_MakeBinary(OP_MULT, ast_MakeInt(-1), ast_Copy(right)));
    if(f == NULL)
        return false;
    simplify(f, SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_RATIONAL | SIMP_EVAL | SIMP_LIKE_TERMS);
    expand(f, EXP_ALL);
    simplify(f, SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_RATIONAL | SIMP_EVAL | SIMP_LIKE_TERMS);
    if(!has_symbol(f, var))
        goto done;
    if(!is_ast_int(right, 0)) { /*everything to one side*/
        l->move = '_';
        l->by = ast_Copy(right);
        l = steps_eq(out, ast_Copy(f), ast_MakeInt(0));
        if(l == NULL)
            goto done;
        l->eq[0].rel = rel;
    }

    g = ast_Copy(f);
    simplify(g, SIMP_ALL);
    comdenom(g, true);
    if(!isoptype(g, OP_DIV))
        factor(g, FAC_POLYNOMIAL);
    if(!steps_same(g, l->eq[0].left)) { /*what the last line shows*/
        steps_rule(l, isoptype(g, OP_DIV) ? "one bottom, factored" : "factor");
        l = steps_eq(out, ast_Copy(g), ast_MakeInt(0));
        if(l == NULL)
            goto done;
        l->eq[0].rel = rel;
    }
    top = g;
    if(isoptype(g, OP_DIV)) {
        top = ast_ChildGet(g, 0);
        bottom = ast_ChildGet(g, 1);
    }

    if(!add_edges(top, var, false, edges, &n))
        goto done;
    if(n > 0) { /*the zeros of the top, then the ones of the bottom*/
        if(n > STEP_EQS_MAX)
            goto done;
        qsort(edges, (size_t)n, sizeof edges[0], by_x);
        steps_rule(l, "where the top is 0");
        l = zeros_line(out, var, edges, n, false);
        if(l == NULL)
            goto done;
    }
    if(bottom != NULL && has_symbol(bottom, var)) {
        int holes = 0;

        if(!add_edges(bottom, var, true, edges, &n))
            goto done;
        qsort(edges, (size_t)n, sizeof edges[0], by_x);
        for(i = 0; i < n; i++)
            holes += edges[i].hole;
        if(holes > STEP_EQS_MAX)
            goto done;
        if(holes > 0) {
            steps_rule(l, "where the bottom is 0, left out");
            l = zeros_line(out, var, edges, n, true);
            if(l == NULL)
                goto done;
        }
    }
    if(n == 0)
        goto done;

    for(i = 0; i <= n; i++) { /*a test point in each gap*/
        pcas_ast_t *t = test_point(edges, n, i), *v = ast_Copy(g), *at = ast_MakeSymbol(var), *shown;
        int s;

        l->move = 't';
        l->by = t;
        shown = ast_Copy(g);
        simplify_canonical_form(shown, CANONICAL_ALL); /*x first, so (-3)_2*/
        substitute(shown, at, t);
        substitute(v, at, t);
        ast_Cleanup(at);
        simplify(v, SIMP_ALL);
        s = sign_of(v);
        l = steps_eq(out, shown, v);
        if(l == NULL || s == 2)
            goto done;
        l->as_is = true;
        gap[i] = wanted(s, rel);
    }
    for(i = 0; i < n; i++)
        edge[i] = closed(rel) && !edges[i].hole;

    steps_rule(l, "keep the gaps that fit");
    l = steps_expr(out, ast_MakeSymbol(var));
    if(l == NULL)
        goto done;
    ast_Cleanup(l->eq[0].left);
    l->eq[0].left = NULL;
    ok = pieces(l, var, edges, n, gap, edge) && !pcas_cancelled;

done:
    ast_Cleanup(f);
    ast_Cleanup(g);
    for(i = 0; i < n; i++)
        ast_Cleanup(edges[i].at);
    return ok;
}

bool steps_ineq(pcas_ast_t *left, pcas_ast_t *right, char rel, Symbol var, steps_t *out) {
    bool ok;

    out->n = 0;
    if(steps_linear(left, right, var, out)) {
        sign_along(out, rel, var);
        steps_shown(out);
        return true;
    }
    ok = chart(left, right, rel, var, out);
    if(ok)
        steps_shown(out);
    else
        steps_free(out);
    return ok;
}
