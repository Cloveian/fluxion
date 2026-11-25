/*Fluxion: int (floor), ceil, round, sign, mod, min, max, and conj, re, im, arg of numbers. not in upstream PineappleCAS.*/

#include <math.h>

#include "cas.h"

#define MARGIN 1e-4 /*how far from a whole number or a .5 a float answer has to be before it's trusted, approx.c is a float*/

/*the rational e is, whether it's one number or the a/b normalize leaves. false for anything else. out is set, not allocated*/
static bool rational_of(pcas_ast_t *e, mp_rat out) {
    pcas_ast_t *c = ast_Copy(e);
    bool ok;

    if(c == NULL)
        return false;
    fold_fraction(c);
    ok = c->type == NODE_NUMBER && mp_rat_copy(c->op.num, out) == MP_OK;
    ast_Cleanup(c);
    return ok;
}

/*floor of r in q. the quotient goes down by 1 when it came out short, a remainder on the under 0 side*/
static bool floor_rat(mp_rat r, mp_int q) {
    mp_int rem = mp_int_alloc();
    bool ok;

    mp_int_init(rem);
    ok = mp_int_div(mp_rat_numer_ref(r), mp_rat_denom_ref(r), q, rem) == MP_OK;
    if(ok && mp_int_compare_zero(rem) != 0 && mp_rat_compare_zero(r) < 0)
        ok = mp_int_sub_value(q, 1, q) == MP_OK;
    mp_int_free(rem);
    return ok;
}

/*the whole number q as a node*/
static pcas_ast_t *whole(mp_int q) {
    mp_rat n = num_FromInt(0);

    if(n == NULL || mp_int_copy(q, mp_rat_numer_ref(n)) != MP_OK) {
        num_Cleanup(n);
        return NULL;
    }
    return ast_MakeNumber(n);
}

/*int, ceil, round and sign of one number. sqrt(2) and pi go by their decimals, when those aren't near a edge*/
static bool eval_round(pcas_ast_t *e, unsigned short flags) {
    OperatorType op = optype(e);
    mp_rat r = num_FromInt(0), half = num_FromInt(0);
    mp_int q = mp_int_alloc();
    pcas_ast_t *result = NULL;
    double v, f;
    bool ok = false, neg;

    if(!(flags & EVAL_INT) || r == NULL || half == NULL)
        goto done;
    mp_int_init(q);
    if(rational_of(ast_ChildGet(e, 0), r)) {
        if(op == OP_SIGN) {
            mp_int_set_value(q, mp_rat_compare_zero(r));
            ok = true;
        } else {
            neg = op == OP_CEIL || (op == OP_ROUND && mp_rat_compare_zero(r) < 0); /*round goes half away from 0, like the TI's*/
            if(neg)
                mp_rat_neg(r, r);
            if(op == OP_ROUND) {
                mp_rat_set_value(half, 1, 2);
                mp_rat_add(r, half, r);
            }
            ok = floor_rat(r, q);
            if(ok && neg)
                ok = mp_int_neg(q, q) == MP_OK;
        }
        result = ok ? whole(q) : NULL;
    } else if(approx_double(ast_ChildGet(e, 0), &v) && v > -1e9 && v < 1e9) {
        if(op == OP_SIGN) {
            if(v > MARGIN || v < -MARGIN)
                result = ast_MakeInt(v > 0 ? 1 : -1);
        } else {
            f = op == OP_ROUND ? (v < 0 ? -v : v) + 0.5 : v; /*round is floor(|v| + 1/2) with v's sign*/
            if(f - floor(f) > MARGIN && ceil(f) - f > MARGIN)
                result = ast_MakeInt((mp_small)(op == OP_CEIL ? ceil(f) : op == OP_ROUND && v < 0 ? -floor(f) : floor(f)));
        }
    }
    if(result != NULL)
        replace_node(e, result);
    ok = result != NULL;
done:
    num_Cleanup(r);
    num_Cleanup(half);
    mp_int_free(q);
    return ok;
}

/*mod(a, b) = a - b floor(a/b) for numbers, min and max of two numbers or of the same one twice*/
static bool eval_pair(pcas_ast_t *e) {
    pcas_ast_t *a = ast_ChildGet(e, 0), *b = ast_ChildGet(e, 1), *pick = NULL;
    mp_rat x = num_FromInt(0), y = num_FromInt(0), t = num_FromInt(0);
    mp_int q = mp_int_alloc();
    double vx, vy;
    bool both = false, ok = false;

    if(x == NULL || y == NULL || t == NULL)
        goto done;
    mp_int_init(q);
    if(optype(e) != OP_MOD && ast_Compare(a, b)) { /*max(x, x) is x*/
        replace_node(e, ast_Copy(a));
        ok = true;
        goto done;
    }
    both = rational_of(a, x) && rational_of(b, y);
    if(optype(e) == OP_MOD) {
        if(!both || mp_rat_compare_zero(y) == 0)
            goto done;
        mp_rat_div(x, y, t);
        if(!floor_rat(t, q))
            goto done;
        mp_rat_set_value(t, 0, 1);
        mp_int_copy(q, mp_rat_numer_ref(t));
        mp_rat_mul(y, t, t);
        mp_rat_sub(x, t, x);
        replace_node(e, ast_MakeNumber(x));
        x = NULL;
        ok = true;
        goto done;
    }
    if(both) {
        pick = (mp_rat_compare(x, y) < 0) == (optype(e) == OP_MIN) ? a : b;
    } else if(approx_double(a, &vx) && approx_double(b, &vy) && (vx - vy > MARGIN || vy - vx > MARGIN)) {
        pick = (vx < vy) == (optype(e) == OP_MIN) ? a : b;
    }
    if(pick != NULL) {
        replace_node(e, ast_Copy(pick));
        ok = true;
    }
done:
    num_Cleanup(x);
    num_Cleanup(y);
    num_Cleanup(t);
    mp_int_free(q);
    return ok;
}

/*conj(z): i is -i, with the letters real*/
static pcas_ast_t *conjugate(pcas_ast_t *z) {
    pcas_ast_t *c = ast_Copy(z), *i = ast_MakeSymbol(SYM_IMAG), *minus_i = ast_MakeBinary(OP_MULT, ast_MakeInt(-1), ast_MakeSymbol(SYM_IMAG));

    if(c != NULL && i != NULL && minus_i != NULL)
        substitute(c, i, minus_i);
    ast_Cleanup(i);
    ast_Cleanup(minus_i);
    return c;
}

/*(z + conj z)/2 for the real part, (z - conj z)/(2i) for the imagnary one*/
static pcas_ast_t *part(pcas_ast_t *z, pcas_ast_t *zbar, bool imaginary) {
    if(!imaginary)
        return ast_MakeBinary(OP_DIV, ast_MakeBinary(OP_ADD, ast_Copy(z), ast_Copy(zbar)), ast_MakeInt(2));
    return ast_MakeBinary(OP_DIV, ast_MakeBinary(OP_ADD, ast_Copy(z), ast_MakeBinary(OP_MULT, ast_MakeInt(-1), ast_Copy(zbar))),
                          ast_MakeBinary(OP_MULT, ast_MakeInt(2), ast_MakeSymbol(SYM_IMAG)));
}

/*re(z) = (z + conj z)/2, im(z) = (z - conj z)/(2i), arg of a number a + bi by its parts. the rest of simplify tidies the trees*/
static bool eval_complex(pcas_ast_t *e) {
    pcas_ast_t *z = ast_ChildGet(e, 0), *zbar = conjugate(z), *out = NULL, *re, *im;
    OperatorType op = optype(e);
    double a, b;

    if(zbar == NULL)
        return false;
    if(op == OP_CONJ) {
        out = zbar;
        zbar = NULL;
    } else if(op == OP_RE) {
        out = part(z, zbar, false);
    } else if(op == OP_IM) {
        out = part(z, zbar, true);
    } else { /*arg: needs the two parts as numbers*/
        re = part(z, zbar, false);
        im = part(z, zbar, true);
        simplify(re, SIMP_ALL);
        simplify(im, SIMP_ALL);
        if(!pcas_cancelled && approx_double(re, &a) && approx_double(im, &b)) {
            if(a == 0 && b == 0) {
                out = NULL;
            } else if(b == 0) {
                out = a > 0 ? ast_MakeInt(0) : ast_MakeSymbol(SYM_PI);
            } else if(a == 0) {
                out = ast_MakeBinary(OP_DIV, ast_MakeSymbol(SYM_PI), ast_MakeInt(b > 0 ? 2 : -2));
            } else {
                out = ast_MakeUnary(OP_TAN_INV, ast_MakeBinary(OP_DIV, ast_Copy(im), ast_Copy(re)));
                if(a < 0)
                    out = ast_MakeBinary(OP_ADD, out, ast_MakeBinary(OP_MULT, ast_MakeInt(b > 0 ? 1 : -1), ast_MakeSymbol(SYM_PI)));
            }
        }
        ast_Cleanup(re);
        ast_Cleanup(im);
    }
    ast_Cleanup(zbar);
    if(out == NULL)
        return false;
    replace_node(e, out);
    return true;
}

bool eval_function(pcas_ast_t *e, unsigned short flags) {
    switch(optype(e)) {
    case OP_INT: case OP_CEIL: case OP_ROUND: case OP_SIGN:
        return eval_round(e, flags);
    case OP_MOD: case OP_MIN: case OP_MAX:
        return eval_pair(e);
    case OP_CONJ: case OP_RE: case OP_IM: case OP_ARG:
        return eval_complex(e);
    default:
        return false;
    }
}
