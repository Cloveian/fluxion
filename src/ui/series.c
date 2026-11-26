#include "series.h"

#include "app.h"
#include "args.h"
#include "calc1.h"
#include "settings.h"

#include <string.h>
#include <time.h>

#include "engine/ast.h"
#include "engine/cas/cas.h"
#include "engine/cas/derivative.h"
#include "render/draw.h"

/* sum(f, k, a, b) and product(f, k, a, b) */

#define TERMS_MAX 100 /* each one is a substitute and a simplify */
#define DEGREE_MAX 3 /* highest power with a sum formula */

/* e as a whole number once simplified, false when it isn't one */
static bool whole_value(pcas_ast_t *e, mp_small *n) {
    pcas_ast_t *c = ast_Copy(e);
    bool ok;

    if(c == NULL) /* oom */
        return false;
    simplify(c, SIMP_ALL);
    ok = whole_number(c, n);
    ast_Cleanup(c);
    return ok;
}

/* lo + (lo+1) + ... + hi, or the product, each term worked out. NULL when cancelled */
static pcas_ast_t *run_terms(pcas_ast_t *f, pcas_ast_t *k, mp_small lo, mp_small hi, bool product) {
    pcas_ast_t *acc = ast_MakeInt(product ? 1 : 0), *v, *t;
    mp_small i;

    for(i = lo; i <= hi && !pcas_cancelled; i++) {
        v = ast_MakeInt(i);
        t = value_at(f, k->op.symbol, v);
        ast_Cleanup(v);
        if(t == NULL) { /* oom */
            ast_Cleanup(acc);
            return NULL;
        }
        acc = ast_MakeBinary(product ? OP_MULT : OP_ADD, acc, t);
        simplify(acc, SIMP_ALL);
    }
    if(pcas_cancelled) {
        ast_Cleanup(acc);
        return NULL;
    }
    return acc;
}

/* 1^j + 2^j + ... + n^j for j up to DEGREE_MAX */
static pcas_ast_t *power_sum(int j, pcas_ast_t *n) {
    pcas_ast_t *half = ast_MakeBinary(OP_DIV, ast_MakeBinary(OP_MULT, ast_Copy(n), ast_MakeBinary(OP_ADD, ast_Copy(n), ast_MakeInt(1))), ast_MakeInt(2));

    switch(j) {
    case 0:
        ast_Cleanup(half);
        return ast_Copy(n);
    case 1:
        return half;
    case 2:
        ast_Cleanup(half);
        return ast_MakeBinary(OP_DIV, ast_MakeBinary(OP_MULT, ast_Copy(n), ast_MakeBinary(OP_MULT, ast_MakeBinary(OP_ADD, ast_Copy(n), ast_MakeInt(1)),
                              ast_MakeBinary(OP_ADD, ast_MakeBinary(OP_MULT, ast_MakeInt(2), ast_Copy(n)), ast_MakeInt(1)))), ast_MakeInt(6));
    default:
        return ast_MakeBinary(OP_POW, half, ast_MakeInt(2));
    }
}

/* f as c0 + c1 k + c2 k^2 + c3 k^3 in c[], from the derivatives at 0. returns the degree, -1 when it's not a polynomial that small */
static int coefficients(pcas_ast_t *f, pcas_ast_t *k, pcas_ast_t **c) {
    pcas_ast_t *d = ast_Copy(f), *z = ast_MakeInt(0), *next;
    int j, fact = 1;

    for(j = 0; j <= DEGREE_MAX && d != NULL; j++) {
        if(!has_symbol(d, k->op.symbol)) {
            c[j] = ast_MakeBinary(OP_DIV, d, ast_MakeInt(fact));
            simplify(c[j], SIMP_ALL);
            ast_Cleanup(z);
            return j;
        }
        c[j] = ast_Copy(d);
        if(c[j] == NULL) /* oom */
            break;
        substitute(c[j], k, z);
        c[j] = ast_MakeBinary(OP_DIV, c[j], ast_MakeInt(fact));
        simplify(c[j], SIMP_ALL);
        next = slope_of(d, k);
        ast_Cleanup(d);
        d = next;
        fact *= j + 1;
    }
    ast_Cleanup(d);
    ast_Cleanup(z);
    for(j = 0; j <= DEGREE_MAX; j++) {
        ast_Cleanup(c[j]);
        c[j] = NULL;
    }
    return -1;
}

/* formula for the sum with k from a to b, b having a letter in it. NULL when f isn't a polynomial or r^k */
static pcas_ast_t *sum_formula(pcas_ast_t *f, pcas_ast_t *k, pcas_ast_t *a, pcas_ast_t *b) {
    pcas_ast_t *c[DEGREE_MAX + 1] = { NULL }, *total = NULL, *below, *r, *part, *base;
    int deg, j;

    /* r^k: (r^(b+1) - r^a)/(r-1) */
    if(isoptype(f, OP_POW) && !has_symbol(ast_ChildGet(f, 0), k->op.symbol) && ast_Compare(ast_ChildGet(f, 1), k)) {
        base = ast_ChildGet(f, 0);
        return ast_MakeBinary(OP_DIV,
            ast_MakeBinary(OP_ADD, ast_MakeBinary(OP_POW, ast_Copy(base), ast_MakeBinary(OP_ADD, ast_Copy(b), ast_MakeInt(1))),
                           ast_MakeBinary(OP_MULT, ast_MakeInt(-1), ast_MakeBinary(OP_POW, ast_Copy(base), ast_Copy(a)))),
            ast_MakeBinary(OP_ADD, ast_Copy(base), ast_MakeInt(-1)));
    }
    deg = coefficients(f, k, c);
    if(deg < 0)
        return NULL;
    below = ast_MakeBinary(OP_ADD, ast_Copy(a), ast_MakeInt(-1));
    total = ast_MakeInt(0);
    for(j = 0; j <= deg; j++) {
        r = power_sum(j, b);
        part = power_sum(j, below);
        part = ast_MakeBinary(OP_MULT, ast_Copy(c[j]), ast_MakeBinary(OP_ADD, r, ast_MakeBinary(OP_MULT, ast_MakeInt(-1), part)));
        total = ast_MakeBinary(OP_ADD, total, part);
    }
    for(j = 0; j <= DEGREE_MAX; j++)
        ast_Cleanup(c[j]);
    ast_Cleanup(below);
    return total;
}

/* an index i would be the imaginary unit, so it's swapped for the first K..Z letter the command doesn't use */
static void index_i_as_letter(pcas_ast_t *a[4]) {
    pcas_ast_t *k;
    Symbol s;
    bool used;
    int j;

    if(a[1]->type != NODE_SYMBOL || a[1]->op.symbol != SYM_IMAG)
        return;
    for(s = SYM_K; s <= SYM_Z; s++) {
        used = false;
        for(j = 0; j < 4; j++)
            used |= has_symbol(a[j], s);
        if(!used)
            break;
    }
    if(s > SYM_Z || (k = ast_MakeSymbol(s)) == NULL)
        return;
    substitute(a[0], a[1], k);
    ast_Cleanup(a[1]);
    a[1] = k;
}

bool series_run(const char *cmd, char *src, box_t *in) {
    pcas_ast_t *a[4] = { NULL }, *e = NULL;
    pcas_error_t err = E_SUCCESS;
    const char *message = NULL;
    bool product;
    box_t *out = NULL;
    clock_t start;
    mp_small lo, hi;
    int n, i;

    if(cmd == NULL || (strcmp(cmd, "sum") != 0 && strcmp(cmd, "product") != 0))
        return false;
    product = cmd[0] == 'p';
    n = args_read(src, a, 4, &err);
    if(n == 4)
        index_i_as_letter(a);
    if(n != 4 || a[1]->type != NODE_SYMBOL || a[1]->op.symbol < SYM_A || a[1]->op.symbol > SYM_Z) {
        push(in, NULL, n < 0 ? error_text[err] : product ? "product(f, k, from, to)" : "sum(f, k, from, to)", 0);
        for(i = 0; i < n; i++)
            ast_Cleanup(a[i]);
        return true;
    }

    start = clock();
    if(whole_value(a[2], &lo) && whole_value(a[3], &hi)) {
        if(hi < lo)
            e = ast_MakeInt(product ? 1 : 0);
        else if(hi - lo >= TERMS_MAX)
            message = "up to 100 terms, a letter for the last one gives a formula";
        else
            e = run_terms(a[0], a[1], lo, hi, product);
    } else if(product) {
        if(ast_Compare(a[0], a[1]) && is_ast_int(a[2], 1))
            e = ast_MakeUnary(OP_FACTORIAL, ast_Copy(a[3]));
        else
            message = "product needs numbers at both ends, or product(k,k,1,n)";
    } else {
        e = sum_formula(a[0], a[1], a[2], a[3]);
        if(e == NULL)
            message = "the term has to be a polynomial in k up to k^3, or r^k";
        else {
            expand(e, EXP_ALL);
            simplify(e, SIMP_ALL);
        }
    }
    if(e != NULL && !pcas_cancelled) {
        simplify_canonical_form(e, CANONICAL_ALL);
        out = layout_build(e, &draw_font);
    }
    if(pcas_cancelled && out == NULL)
        message = pcas_oom ? "out of memory" : "cancelled";
    push(in, out, message, (unsigned long)((clock() - start) * 1000 / CLOCKS_PER_SEC));
    ast_Cleanup(e);
    for(i = 0; i < n; i++)
        ast_Cleanup(a[i]);
    return true;
}
