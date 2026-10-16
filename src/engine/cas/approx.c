/*Fluxion: the decimal form. not in upstream PineappleCAS.*/

/*the code is C89, but sinl, isfinite, ... are C99 library: glibc hides them under -std=c89*/
#define _ISOC99_SOURCE

#include "cas.h"

#include <math.h>
#include <string.h>

/*float (the CE's double). long double maths is +13 KB compressed and there just isn't
room. float on the PC too so the tests get the same ~7 digits the CE does*/
typedef float real;

static real int_value(mp_int z) {
    real v = 0;
    mp_size i;

    for(i = MP_USED(z); i > 0; i--)
        v = ldexpf(v, (int)MP_DIGIT_BIT) + (real)MP_DIGITS(z)[i - 1];
    return MP_SIGN(z) == MP_NEG ? -v : v;
}

static bool is_whole(real x) {
    return x == floorf(x);
}

/*the value of e if it has no variables and is real, in *x. floats the whole way, good enough*/
static bool value(pcas_ast_t *e, real *x) {
    pcas_ast_t *child;
    real a = 0, b = 0;

    if(e->type == NODE_NUMBER) {
        *x = int_value(mp_rat_numer_ref(e->op.num)) / int_value(mp_rat_denom_ref(e->op.num));
        return true;
    }
    if(e->type == NODE_SYMBOL) {
        if(e->op.symbol == SYM_PI)    *x = 3.14159265358979323846F;
        else if(e->op.symbol == SYM_EULER) *x = 2.71828182845904523536F;
        /*else if(e->op.symbol == SYM_PHI) *x = 1.6180339887F;  // no phi symbol yet, add if i ever need it*/
        else return false;
        return true;
    }

    if(optype(e) == OP_ADD || optype(e) == OP_MULT) {
        a = optype(e) == OP_ADD ? 0 : 1;
        for(child = ast_ChildGet(e, 0); child != NULL; child = child->next) {
            if(!value(child, &b))
                return false;
            a = optype(e) == OP_ADD ? a + b : a * b;
        }
        *x = a;
        return isfinite(a);
    }

    if(optype(e) == OP_DERIV || !value(ast_ChildGet(e, 0), &a))
        return false;
    if(ast_ChildLength(e) > 1 && !value(ast_ChildGet(e, 1), &b))
        return false;

    switch(optype(e)) {
    case OP_DIV:
        if(b == 0) return false;
        *x = a / b;
        break;
    case OP_POW:
        if(a < 0 && !is_whole(b)) return false;
        *x = powf(a, b);
        break;
    case OP_ROOT:
        /*a = index, b = radicand. odd roots of negatives are real*/
        if(b < 0 && !(is_whole(a) && fmodf(a, 2) != 0)) return false;
        *x = b < 0 ? -powf(-b, 1 / a) : powf(b, 1 / a);
        break;
    case OP_LOG:
        if(a <= 0 || a == 1 || b <= 0) return false;
        *x = logf(b) / logf(a);
        break;
    case OP_FACTORIAL:
        if(a < 0 || !is_whole(a)) return false;
        *x = tgammaf(a + 1);
        break;
    case OP_INT:      *x = floorf(a); break;
    case OP_ABS:      *x = fabsf(a); break;
    case OP_SIN:      *x = sinf(a); break;
    case OP_COS:      *x = cosf(a); break;
    case OP_TAN:      *x = tanf(a); break;
    case OP_SIN_INV:  if(a < -1 || a > 1) return false; *x = asinf(a); break;
    case OP_COS_INV:  if(a < -1 || a > 1) return false; *x = acosf(a); break;
    case OP_TAN_INV:  *x = atanf(a); break;
    case OP_COT:      *x = 1 / tanf(a); break;
    case OP_SEC:      *x = 1 / cosf(a); break;
    case OP_CSC:      *x = 1 / sinf(a); break;
    case OP_SINH:     *x = sinhf(a); break;
    case OP_COSH:     *x = coshf(a); break;
    case OP_TANH:     *x = tanhf(a); break;
    case OP_SINH_INV: *x = asinhf(a); break;
    case OP_COSH_INV: if(a < 1) return false; *x = acoshf(a); break;
    case OP_TANH_INV: if(a <= -1 || a >= 1) return false; *x = atanhf(a); break;
    default:
        return false;
    }
    return isfinite(*x);
}

/*x exactly, as a fraction over a power of 2*/
static mp_rat rat_from(real x) {
    mp_rat r = num_FromInt(0);
    int k;

    if(x == 0)
        return r;

    /*x = f * 2^k, 1/2 <= |f| < 1, and f's 24 bits fit in a long*/
    /*Poyo!!*/
    mp_int_set_value(mp_rat_numer_ref(r), (long)ldexpf(frexpf(x, &k), 24));

    k -= 24;
    if(k >= 0)
        mp_int_mul_pow2(mp_rat_numer_ref(r), k, mp_rat_numer_ref(r));
    else
        mp_int_mul_pow2(mp_rat_denom_ref(r), -k, mp_rat_denom_ref(r));
    mp_rat_reduce(r);
    return r;
}

bool approx(pcas_ast_t *e) {
    pcas_ast_t *child;
    bool changed = false;
    real x;

    if(e->type == NODE_NUMBER)
        return false;

    /*sums, products and fractions stay exact (simplify's eval does them), only the
    irrational parts are rounded: 7e^15+1 has the error of e^15 alone*/
    if(!isoptype(e, OP_ADD) && !isoptype(e, OP_MULT) && !isoptype(e, OP_DIV) && value(e, &x)) {
        replace_node(e, ast_MakeNumber(rat_from(x)));
        return true;
    }

    if(e->type == NODE_OPERATOR) {
        for(child = ast_ChildGet(e, 0); child != NULL; child = child->next)
            changed |= approx(child);
    }
    return changed;
}

/*round(n * 10^s / d) into out, n and d positive*/
static void scaled_round(mp_int n, mp_int d, int s, mp_int out) {
    mp_int top = mp_int_alloc(), bot = mp_int_alloc(), p = mp_int_alloc();

    mp_int_expt_value(10, s < 0 ? -s : s, p);
    if(s >= 0) {
        mp_int_mul(n, p, top);
        mp_int_copy(d, bot);
    } else {
        mp_int_copy(n, top);
        mp_int_mul(d, p, bot);
    }

    /*floor((2top + bot) / 2bot)*/
    mp_int_mul_pow2(top, 1, top);
    mp_int_add(top, bot, top);
    mp_int_mul_pow2(bot, 1, bot);
    mp_int_div(top, bot, out, NULL);

    mp_int_free(top);
    mp_int_free(bot);
    mp_int_free(p);
}

/*n <=> d * 10^k, n and d positive*/
static int compare_pow10(mp_int n, mp_int d, int k) {
    mp_int a = mp_int_alloc(), b = mp_int_alloc(), p = mp_int_alloc();
    int c;

    mp_int_expt_value(10, k < 0 ? -k : k, p);
    if(k >= 0) {
        mp_int_copy(n, a);
        mp_int_mul(d, p, b);
    } else {
        mp_int_mul(n, p, a);
        mp_int_copy(d, b);
    }
    c = mp_int_compare(a, b);

    mp_int_free(a);
    mp_int_free(b);
    mp_int_free(p);
    return c;
}

void approx_format(mp_rat r, int digits, char *buf) {
    mp_int n = mp_int_alloc(), d = mp_int_alloc(), m = mp_int_alloc(), hi = mp_int_alloc();
    char s[24];
    int exp, len, i;

    /*out of memory: the run's being thrown away, and a garbage r can send exp off
    to -20000 and sit there computing 10^20000 (the sweep found that one)*/
    if(pcas_oom) {
        *buf = '\0';
        goto done;
    }

    if(mp_rat_compare_zero(r) == 0) {
        strcpy(buf, "0");
        goto done;
    }
    if(mp_rat_compare_zero(r) < 0)
        *buf++ = '-';

    mp_int_abs(mp_rat_numer_ref(r), n);
    mp_int_copy(mp_rat_denom_ref(r), d);

    /*10^exp <= n/d < 10^(exp+1), exactly: the digit counts are a guess (a bound,
    can be one or two off). not a float log: past 10^38 that was inf, and m got
    every digit*/
    /*OwO*/
    exp = (int)mp_int_string_len(n, 10) - (int)mp_int_string_len(d, 10);
    while(!pcas_oom && compare_pow10(n, d, exp) < 0)
        exp--;
    while(!pcas_oom && compare_pow10(n, d, exp + 1) >= 0)
        exp++;

    /*digits significant digits, m in [10^(digits-1), 10^digits). rounding can
    carry over: 9.999996 is 10.0000*/
    scaled_round(n, d, digits - 1 - exp, m);
    mp_int_expt_value(10, digits, hi);
    if(mp_int_compare(m, hi) >= 0) {
        exp++;
        mp_int_div_value(m, 10, m, NULL);
    }

    mp_int_to_string(m, 10, s, sizeof(s) - 1);     /*imath writes the NUL at s[limit]*/
    len = (int)strlen(s);
    while(len > 1 && s[len - 1] == '0')
        len--;

    /*plain from 0.001 up to what fits in the digits, like the TI-84; else 1.23457E7*/
    if(exp >= -3 && exp < digits) {
        if(exp < 0) {
            *buf++ = '0';
            *buf++ = '.';
            for(i = -1; i > exp; i--)
                *buf++ = '0';
            memcpy(buf, s, len);
            buf += len;
        } else {
            for(i = 0; i <= exp; i++)
                *buf++ = i < len ? s[i] : '0';
            if(len > exp + 1) {
                *buf++ = '.';
                memcpy(buf, s + exp + 1, len - exp - 1);
                buf += len - exp - 1;
            }
        }
        *buf = '\0';
    } else {
        *buf++ = s[0];
        if(len > 1) {
            *buf++ = '.';
            memcpy(buf, s + 1, len - 1);
            buf += len - 1;
        }
        *buf++ = 'E';
        if(exp < 0) {
            *buf++ = '-';
            exp = -exp;
        }
        /*s is free again: the exponent's digits backwards*/
        for(len = 0; exp > 0 || len == 0; exp /= 10)
            s[len++] = (char)('0' + exp % 10);
        while(len > 0)
            *buf++ = s[--len];
        *buf = '\0';
    }

done:
    mp_int_free(n);
    mp_int_free(d);
    mp_int_free(m);
    mp_int_free(hi);
}
