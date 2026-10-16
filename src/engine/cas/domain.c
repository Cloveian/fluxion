/*
    Fluxion: domain(), not in PineappleCAS. where f is defined over the reals, the
    way the precalc worksheet wants it: the bottom can't be 0, nothing negative
    under an even root, a log wants more than 0, tan breaks every pi/2 + pi n.

    the tree is walked as typed, like asym does it, so x/x still loses x = 0.
    roots and logs only give a bound when what's inside is linear (a x + b),
    anything worse sets partial and the app owns up to it. two bounds on the
    same side keep the tighter one, holes past a bound go, and a hole sitting
    on a closed bound opens it instead (sqrt(x)/x is x > 0)
*/

#include <string.h>

#include "cas.h"

#define SIMP_DOMAIN (SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_RATIONAL | SIMP_EVAL | SIMP_LIKE_TERMS)

/*what sign_of says when there's a letter left (or pi n), can't order those*/
#define UNSURE 2

typedef struct {
    domain_t *d;
    Symbol var;
    bool dead; /*a bottom that's 0 for every x, there's no domain to give*/
} walk_t;

static pcas_ast_t *number(mp_small n) {
    return ast_MakeNumber(num_FromInt(n));
}

static int sign(int c) {
    return c < 0 ? -1 : c > 0;
}

/*-1, 0, 1 or UNSURE. 0 only when it's exactly 0, the rest goes by the decimal,
  which is good enough to tell 2 from sqrt(5)*/
static int sign_of(pcas_ast_t *e) {
    pcas_ast_t *c = e != NULL ? ast_Copy(e) : NULL, *a, *b;
    int s = UNSURE;

    if(c == NULL)
        return UNSURE;
    simplify(c, SIMP_DOMAIN);
    if(is_ast_int(c, 0)) {
        ast_Cleanup(c);
        return 0;
    }
    approx(c);
    simplify(c, SIMP_DOMAIN);
    if(c->type == NODE_NUMBER) {
        s = sign(mp_rat_compare_zero(c->op.num));
    } else if(isoptype(c, OP_DIV)) { /*normalize puts the fraction back as a/b*/
        a = ast_ChildGet(c, 0);
        b = ast_ChildGet(c, 1);
        if(a->type == NODE_NUMBER && b->type == NODE_NUMBER)
            s = sign(mp_rat_compare_zero(a->op.num)) * sign(mp_rat_compare_zero(b->op.num));
    }
    if(s == 0 || pcas_cancelled) /*rounded to 0 but wasn't, or half done*/
        s = UNSURE;
    ast_Cleanup(c);
    return s;
}

/*sign of a - b*/
static int compare(pcas_ast_t *a, pcas_ast_t *b) {
    pcas_ast_t *d = ast_MakeBinary(OP_ADD, ast_Copy(a), ast_MakeBinary(OP_MULT, number(-1), ast_Copy(b)));
    int s = sign_of(d);

    ast_Cleanup(d);
    return s;
}

/*a number exponent or root index as p/q, false if it isn't one (pi, or too big)*/
static bool read_ratio(pcas_ast_t *e, mp_small *p, mp_small *q) {
    pcas_ast_t *c = ast_Copy(e), *a, *b;
    mp_rat r = NULL;
    bool ok = false;

    if(c == NULL)
        return false;
    simplify(c, SIMP_DOMAIN);
    if(c->type == NODE_NUMBER) {
        ok = mp_rat_to_ints(c->op.num, p, q) == MP_OK;
    } else if(isoptype(c, OP_DIV)) {
        a = ast_ChildGet(c, 0);
        b = ast_ChildGet(c, 1);
        if(a->type == NODE_NUMBER && b->type == NODE_NUMBER && mp_rat_compare_zero(b->op.num) != 0
           && (r = num_FromInt(0)) != NULL && mp_rat_div(a->op.num, b->op.num, r) == MP_OK)
            ok = mp_rat_to_ints(r, p, q) == MP_OK;
    }
    if(r != NULL)
        num_Cleanup(r);
    ast_Cleanup(c);
    if(ok && *q < 0) {
        *p = -*p;
        *q = -*q;
    }
    return ok;
}

/*takes x*/
static void add_hole(walk_t *w, pcas_ast_t *x) {
    domain_t *d = w->d;
    int i;

    if(x == NULL)
        return;
    for(i = 0; i < d->nholes; i++) {
        if(compare(x, d->holes[i]) == 0) { /*1/x + 1/x, once is plenty*/
            ast_Cleanup(x);
            return;
        }
    }
    if(d->nholes == SOLVE_MAX) {
        d->partial = true;
        ast_Cleanup(x);
        return;
    }
    d->holes[d->nholes++] = x;
}

/*every real zero of e is out*/
static void zeros(walk_t *w, pcas_ast_t *e) {
    pcas_ast_t *r[SOLVE_MAX];
    int n, i;

    for(i = 0; i < SOLVE_MAX; i++)
        r[i] = NULL;
    n = solve(e, w->var, r);
    if(n == SOLVE_ALL)
        w->dead = true;
    else if(n == SOLVE_UNKNOWN)
        w->d->partial = true;
    for(i = 0; i < n; i++)
        add_hole(w, r[i]);
}

/*takes x. dir 1 for a lower bound, where bigger is tighter, -1 for an upper one*/
static void tighten(domain_t *d, pcas_ast_t **b, bool *b_open, pcas_ast_t *x, bool open, int dir) {
    int c;

    if(*b == NULL) {
        *b = x;
        *b_open = open;
        return;
    }
    c = compare(x, *b);
    if(c == dir) {
        ast_Cleanup(*b);
        *b = x;
        *b_open = open;
        return;
    }
    if(c == 0)
        *b_open = *b_open || open;
    else if(c == UNSURE) /*sqrt(x-a) + sqrt(x-b), depends on a and b*/
        d->partial = true;
    ast_Cleanup(x);
}

/*u >= 0, or u > 0 when open, as a bound on var. only when u is a x + b and the
  sign of a is known, a quadratic in there is the "and maybe more" case*/
static void bound(walk_t *w, pcas_ast_t *u, bool open) {
    domain_t *d = w->d;
    pcas_ast_t *lead = NULL, *r[SOLVE_MAX];
    int deg, s = UNSURE, n, i;

    if(u == NULL || !has_symbol(u, w->var))
        return;
    deg = poly_degree(u, w->var, &lead);
    if(deg == 1)
        s = sign_of(lead);
    ast_Cleanup(lead);
    if(deg == 0) /*x - x + 1, the x went*/
        return;
    if(s != 1 && s != -1) {
        d->partial = true;
        return;
    }

    for(i = 0; i < SOLVE_MAX; i++)
        r[i] = NULL;
    n = solve(u, w->var, r);
    if(n != 1) {
        for(i = 0; i < n; i++)
            ast_Cleanup(r[i]);
        d->partial = true;
        return;
    }
    if(s > 0)
        tighten(d, &d->lower, &d->lower_open, r[0], open, 1);
    else
        tighten(d, &d->upper, &d->upper_open, r[0], open, -1);
}

/*tan and sec are out where cos(u) = 0, cot and csc where sin(u) = 0. solve gives
  those as the whole family*/
static void trig_zeros(walk_t *w, OperatorType op, pcas_ast_t *u) {
    pcas_ast_t *t;

    if(!has_symbol(u, w->var))
        return;
    t = ast_MakeUnary(op, ast_Copy(u));
    if(t != NULL)
        zeros(w, t);
    ast_Cleanup(t);
}

/*-1 <= u <= 1 (strict when open) as two bounds, for asin and friends*/
static void between(walk_t *w, pcas_ast_t *u, bool open) {
    pcas_ast_t *t;

    t = ast_MakeBinary(OP_ADD, ast_Copy(u), number(1));
    bound(w, t, open);
    ast_Cleanup(t);
    t = ast_MakeBinary(OP_ADD, number(1), ast_MakeBinary(OP_MULT, number(-1), ast_Copy(u)));
    bound(w, t, open);
    ast_Cleanup(t);
}

static void walk(walk_t *w, pcas_ast_t *e) {
    pcas_ast_t *c, *a, *b, *t;
    mp_small p, q;

    if(e->type != NODE_OPERATOR || w->dead || pcas_cancelled)
        return;
    for(c = ast_ChildGet(e, 0); c != NULL; c = c->next)
        walk(w, c);
    if(!has_symbol(e, w->var))
        return;

    a = ast_ChildGet(e, 0);
    b = a != NULL ? a->next : NULL; /*NULL for sin and co*/
    switch(optype(e)) {
    case OP_DIV:
        if(b != NULL && has_symbol(b, w->var))
            zeros(w, b);
        break;
    case OP_POW:
        if(b == NULL || !has_symbol(a, w->var)) /*2^x and e^x are fine everywhere*/
            break;
        if(has_symbol(b, w->var) || !read_ratio(b, &p, &q)) /*x^x, x^pi*/
            w->d->partial = true;
        else if(q % 2 == 0) /*x^(1/2) is sqrt(x), x^(-1/2) is 1/sqrt(x)*/
            bound(w, a, p < 0);
        else if(p < 0)
            zeros(w, a);
        break;
    case OP_ROOT:
        if(b == NULL || !has_symbol(b, w->var))
            break;
        if(has_symbol(a, w->var) || !read_ratio(a, &p, &q) || q != 1 || p <= 0)
            w->d->partial = true;
        else if(p % 2 == 0)
            bound(w, b, false);
        break;
    case OP_LOG:
        if(has_symbol(a, w->var)) /*log base x wants x > 0 and x != 1 on top, todo*/
            w->d->partial = true;
        else
            bound(w, b, true);
        break;
    case OP_TAN: case OP_SEC:
        trig_zeros(w, OP_COS, a);
        break;
    case OP_COT: case OP_CSC:
        trig_zeros(w, OP_SIN, a);
        break;
    case OP_SIN_INV: case OP_COS_INV:
        between(w, a, false);
        break;
    case OP_TANH_INV:
        between(w, a, true);
        break;
    case OP_COSH_INV: /*u >= 1*/
        t = ast_MakeBinary(OP_ADD, ast_Copy(a), number(-1));
        bound(w, t, false);
        ast_Cleanup(t);
        break;
    default:
        break;
    }
}

void domain_free(domain_t *d) {
    int i;

    for(i = 0; i < d->nholes; i++)
        ast_Cleanup(d->holes[i]);
    ast_Cleanup(d->lower);
    ast_Cleanup(d->upper);
    memset(d, 0, sizeof *d);
}

bool domain(pcas_ast_t *f, Symbol var, domain_t *out) {
    walk_t w;
    int i, j, c;

    memset(out, 0, sizeof *out);
    if(f == NULL || pcas_cancelled)
        return false;
    w.d = out;
    w.var = var;
    w.dead = false;
    walk(&w, f);
    if(w.dead || pcas_cancelled) {
        domain_free(out);
        return false;
    }

    /*holes the bounds already threw out. the n ones can't be compared, they stay*/
    for(i = 0; i < out->nholes; ) {
        pcas_ast_t *h = out->holes[i];
        bool drop = false;

        if(!has_symbol(h, SYM_N)) {
            c = out->lower != NULL ? compare(h, out->lower) : 1;
            if(c == 0)
                out->lower_open = true;
            drop = c == 0 || c == -1;
            c = out->upper != NULL ? compare(h, out->upper) : -1;
            if(c == 0)
                out->upper_open = true;
            drop = drop || c == 0 || c == 1;
        }
        if(drop) {
            ast_Cleanup(h);
            for(j = i + 1; j < out->nholes; j++)
                out->holes[j - 1] = out->holes[j];
            out->holes[--out->nholes] = NULL;
        } else {
            i++;
        }
    }

    /*sqrt(x-5) + sqrt(2-x), the bounds cross and there's nothing left. a hole
      on a one point domain lands here too, the loop above opened both sides.
      the holes don't mean anything then*/
    if(out->lower != NULL && out->upper != NULL) {
        c = compare(out->lower, out->upper);
        out->empty = c == 1 || (c == 0 && (out->lower_open || out->upper_open));
    }
    if(out->empty) {
        for(i = 0; i < out->nholes; i++) {
            ast_Cleanup(out->holes[i]);
            out->holes[i] = NULL;
        }
        out->nholes = 0;
    }

    /*smallest first. each solve() came sorted, but there's one per bottom*/
    for(i = 1; i < out->nholes; i++) {
        pcas_ast_t *h = out->holes[i];

        for(j = i; j > 0 && compare(out->holes[j - 1], h) == 1; j--)
            out->holes[j] = out->holes[j - 1];
        out->holes[j] = h;
    }

    if(pcas_cancelled) {
        domain_free(out);
        return false;
    }
    return true;
}
