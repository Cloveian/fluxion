/*
    Fluxion: a flat polynomial, so (x+1)^8 and (a+b)^3(b+1)^3 get multiplied out
    as rows of coefficients instead of the tree being distributed and collected
    one pair of terms at a time. the generic expand was 4 minutes on the
    calculator for (x+1)^8 even after it learned to collect as it went. this is
    nothing, it's a few hundred rational multiplies.

    only the plain polynomial bits get read, letters (pi, e, theta too) to whole
    powers, numbers, sums, products, powers up to EXPAND_MAX_POWER, and a number
    on the bottom. anything else in a node and poly_expand() leaves that node to
    the old code and carries on into its children
*/

#include <string.h>

#include "cas.h"

#define POLY_VARS 8 /*different letters in one expression*/
#define POLY_MAX_TERMS 512 /*past this it's not something anyone wants to read anyway*/

typedef struct {
    mp_rat c;
    unsigned char x[POLY_VARS]; /*exponent per letter*/
} term_t;

typedef struct {
    term_t *t;
    int n, cap;
} poly_t;

typedef struct {
    Symbol sym[POLY_VARS];
    int n;
} vars_t;

static void poly_free(poly_t *p) {
    int i;

    for(i = 0; i < p->n; i++)
        num_Cleanup(p->t[i].c);
    free(p->t);
    p->t = NULL;
    p->n = p->cap = 0;
}

/*c x^.. into p, onto the term with the same letters if there is one*/
static bool poly_push(poly_t *p, mp_rat c, const unsigned char *x) {
    int i;

    for(i = 0; i < p->n; i++)
        if(memcmp(p->t[i].x, x, POLY_VARS) == 0)
            return mp_rat_add(p->t[i].c, c, p->t[i].c) == MP_OK;
    if(p->n == p->cap) {
        int cap = p->cap > 0 ? p->cap * 2 : 8;
        term_t *t;

        if(cap > POLY_MAX_TERMS)
            return false;
        t = pcas_realloc(p->t, cap * sizeof *t);
        if(t == NULL)
            return false;
        p->t = t;
        p->cap = cap;
    }
    p->t[p->n].c = num_Copy(c);
    if(p->t[p->n].c == NULL)
        return false;
    memcpy(p->t[p->n].x, x, POLY_VARS);
    p->n++;
    return true;
}

static bool poly_mul(const poly_t *a, const poly_t *b, poly_t *out) {
    mp_rat c = num_FromInt(0);
    unsigned char x[POLY_VARS];
    int i, j, k;
    bool ok = c != NULL;

    for(i = 0; ok && i < a->n; i++)
        for(j = 0; ok && j < b->n; j++) {
            for(k = 0; k < POLY_VARS; k++) {
                if(a->t[i].x[k] + b->t[j].x[k] > 255)
                    ok = false;
                x[k] = (unsigned char)(a->t[i].x[k] + b->t[j].x[k]);
            }
            ok = ok && mp_rat_mul(a->t[i].c, b->t[j].c, c) == MP_OK && poly_push(out, c, x);
        }
    num_Cleanup(c);
    return ok && !pcas_cancelled;
}

static bool poly_one(poly_t *p) {
    unsigned char x[POLY_VARS];
    mp_rat one = num_FromInt(1);
    bool ok;

    memset(x, 0, sizeof x);
    ok = one != NULL && poly_push(p, one, x);
    num_Cleanup(one);
    return ok;
}

static bool poly_pow(const poly_t *a, long n, poly_t *out) {
    poly_t acc = { NULL, 0, 0 }, next = { NULL, 0, 0 };
    long k;

    if(!poly_one(&acc))
        return false;
    for(k = 0; k < n; k++) {
        if(!poly_mul(&acc, a, &next)) {
            poly_free(&acc);
            poly_free(&next);
            return false;
        }
        poly_free(&acc);
        acc = next;
        next.t = NULL;
        next.n = next.cap = 0;
    }
    *out = acc;
    return true;
}

static int var_index(const vars_t *v, Symbol s) {
    int i;

    for(i = 0; i < v->n; i++)
        if(v->sym[i] == s)
            return i;
    return -1;
}

static bool whole_power(const pcas_ast_t *p, long *n) {
    mp_small num, den;

    if(p->type != NODE_NUMBER || mp_rat_to_ints(p->op.num, &num, &den) != MP_OK || den != 1)
        return false;
    *n = num;
    return num >= 0 && num <= EXPAND_MAX_POWER;
}

/*the letters in e, and whether e is something this file can read at all*/
static bool collect(pcas_ast_t *e, vars_t *v) {
    pcas_ast_t *k;
    long n;

    switch(e->type) {
    case NODE_NUMBER:
        return true;
    case NODE_SYMBOL:
        if(e->op.symbol == SYM_IMAG) /*i squared is -1, not a letter*/
            return false;
        if(var_index(v, e->op.symbol) >= 0)
            return true;
        if(v->n == POLY_VARS)
            return false;
        v->sym[v->n++] = e->op.symbol;
        return true;
    case NODE_OPERATOR:
        switch(optype(e)) {
        case OP_ADD: case OP_MULT:
            for(k = opbase(e); k != NULL; k = k->next)
                if(!collect(k, v))
                    return false;
            return true;
        case OP_POW:
            return whole_power(ast_ChildGet(e, 1), &n) && collect(ast_ChildGet(e, 0), v);
        case OP_DIV:
            k = ast_ChildGet(e, 1);
            return k->type == NODE_NUMBER && mp_rat_compare_zero(k->op.num) != 0 && collect(ast_ChildGet(e, 0), v);
        default:
            return false;
        }
    default:
        return false;
    }
}

/*e as a polynomial, into out (empty going in). collect() said yes already*/
static bool read_poly(pcas_ast_t *e, const vars_t *v, poly_t *out) {
    unsigned char x[POLY_VARS];
    pcas_ast_t *k;
    poly_t tmp = { NULL, 0, 0 }, acc = { NULL, 0, 0 };
    mp_rat c;
    long n;
    int i;
    bool ok = true;

    memset(x, 0, sizeof x);
    switch(e->type) {
    case NODE_NUMBER:
        return poly_push(out, e->op.num, x);
    case NODE_SYMBOL:
        x[var_index(v, e->op.symbol)] = 1;
        c = num_FromInt(1);
        ok = c != NULL && poly_push(out, c, x);
        num_Cleanup(c);
        return ok;
    default:
        break;
    }

    switch(optype(e)) {
    case OP_ADD:
        for(k = opbase(e); ok && k != NULL; k = k->next) {
            ok = read_poly(k, v, &tmp);
            for(i = 0; ok && i < tmp.n; i++)
                ok = poly_push(out, tmp.t[i].c, tmp.t[i].x);
            poly_free(&tmp);
        }
        return ok;
    case OP_MULT:
        ok = poly_one(&acc);
        for(k = opbase(e); ok && k != NULL; k = k->next) {
            poly_t prod = { NULL, 0, 0 };

            ok = read_poly(k, v, &tmp) && poly_mul(&acc, &tmp, &prod);
            poly_free(&tmp);
            poly_free(&acc);
            acc = prod;
        }
        for(i = 0; ok && i < acc.n; i++)
            ok = poly_push(out, acc.t[i].c, acc.t[i].x);
        poly_free(&acc);
        return ok;
    case OP_POW:
        whole_power(ast_ChildGet(e, 1), &n);
        ok = read_poly(ast_ChildGet(e, 0), v, &tmp) && poly_pow(&tmp, n, &acc);
        poly_free(&tmp);
        for(i = 0; ok && i < acc.n; i++)
            ok = poly_push(out, acc.t[i].c, acc.t[i].x);
        poly_free(&acc);
        return ok;
    case OP_DIV:
        c = num_FromInt(1);
        ok = c != NULL && mp_rat_div(c, ast_ChildGet(e, 1)->op.num, c) == MP_OK && read_poly(ast_ChildGet(e, 0), v, &tmp);
        for(i = 0; ok && i < tmp.n; i++)
            ok = mp_rat_mul(tmp.t[i].c, c, tmp.t[i].c) == MP_OK && poly_push(out, tmp.t[i].c, tmp.t[i].x);
        poly_free(&tmp);
        num_Cleanup(c);
        return ok;
    default:
        return false;
    }
}

static int degree(const term_t *t) {
    int i, d = 0;

    for(i = 0; i < POLY_VARS; i++)
        d += t->x[i];
    return d;
}

/*back to a tree, highest degree first (simplify sorts it its own way after anyway)*/
static pcas_ast_t *build(poly_t *p, const vars_t *v) {
    pcas_ast_t *sum = ast_MakeOperator(OP_ADD);
    int i, j;

    if(sum == NULL)
        return NULL;
    for(i = 1; i < p->n; i++) {
        term_t t = p->t[i];

        for(j = i; j > 0 && degree(&p->t[j - 1]) < degree(&t); j--)
            p->t[j] = p->t[j - 1];
        p->t[j] = t;
    }

    for(i = 0; i < p->n; i++) {
        pcas_ast_t *m = ast_MakeOperator(OP_MULT);
        term_t *t = &p->t[i];

        if(mp_rat_compare_zero(t->c) == 0 || m == NULL) {
            ast_Cleanup(m);
            continue;
        }
        if(degree(t) == 0 || mp_rat_compare_value(t->c, 1, 1) != 0)
            ast_ChildAppend(m, ast_MakeNumber(num_Copy(t->c)));
        for(j = 0; j < v->n; j++) {
            if(t->x[j] == 0)
                continue;
            ast_ChildAppend(m, t->x[j] == 1 ? ast_MakeSymbol(v->sym[j])
                                            : ast_MakeBinary(OP_POW, ast_MakeSymbol(v->sym[j]), ast_MakeNumber(num_FromInt(t->x[j]))));
        }
        if(ast_ChildLength(m) == 1) {
            pcas_ast_t *only = ast_ChildRemoveIndex(m, 0);

            ast_Cleanup(m);
            m = only;
        }
        ast_ChildAppend(sum, m);
    }
    if(ast_ChildLength(sum) == 0) {
        ast_Cleanup(sum);
        return ast_MakeNumber(num_FromInt(0));
    }
    if(ast_ChildLength(sum) == 1) {
        pcas_ast_t *only = ast_ChildRemoveIndex(sum, 0);

        ast_Cleanup(sum);
        return only;
    }
    return sum;
}

/*is there anything to multiply out here. a sum to a power, a product with a
  sum in it, or one of those over a number*/
static bool wants_it(pcas_ast_t *e) {
    pcas_ast_t *k;
    long n;

    if(e->type != NODE_OPERATOR)
        return false;
    switch(optype(e)) {
    case OP_POW:
        return isoptype(ast_ChildGet(e, 0), OP_ADD) && whole_power(ast_ChildGet(e, 1), &n) && n >= 2;
    case OP_MULT:
        for(k = opbase(e); k != NULL; k = k->next)
            if(isoptype(k, OP_ADD) || wants_it(k))
                return true;
        return false;
    case OP_DIV:
        return ast_ChildGet(e, 1)->type == NODE_NUMBER && wants_it(ast_ChildGet(e, 0));
    default:
        return false;
    }
}

bool poly_expand(pcas_ast_t *e) {
    bool changed = false;
    unsigned i;

    if(e == NULL || e->type != NODE_OPERATOR || pcas_cancelled)
        return false;

    if(wants_it(e)) {
        vars_t v;
        poly_t p = { NULL, 0, 0 };

        v.n = 0;
        if(collect(e, &v) && read_poly(e, &v, &p)) {
            pcas_ast_t *tree = build(&p, &v);

            if(tree != NULL) {
                replace_node(e, tree);
                changed = true;
            }
        }
        poly_free(&p);
        if(changed)
            return true;
    }

    for(i = 0; i < ast_ChildLength(e); i++)
        changed |= poly_expand(ast_ChildGet(e, i));
    return changed;
}
