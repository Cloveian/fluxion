/*Fluxion: limits, worked out the way you would on paper. not in upstream PineappleCAS.*/

#include "cas.h"
#include "steps.h"
#include "../parser.h"

#define TRIES_MAX 6 /*cancels and l'hopitals before it gives up, (1-cos(x))/x^2 takes 2*/

/*e with var = a and simplified, new. NULL once cancelled or out of memory*/
static pcas_ast_t *value_at(pcas_ast_t *e, pcas_ast_t *var, pcas_ast_t *a) {
    pcas_ast_t *v = ast_Copy(e);

    if(v == NULL)
        return NULL;
    substitute(v, var, a);
    simplify(v, SIMP_ALL);
    if(pcas_cancelled) {
        ast_Cleanup(v);
        return NULL;
    }
    return v;
}

static bool is_int_at(pcas_ast_t *e, pcas_ast_t *var, pcas_ast_t *a, int n) {
    pcas_ast_t *v = value_at(e, var, a);
    bool is = v != NULL && is_ast_int(v, n);

    ast_Cleanup(v);
    return is;
}

/*a letter left in e, pi e i and inf aren't*/
static bool has_letter(pcas_ast_t *e) {
    pcas_ast_t *c;

    if(e->type == NODE_SYMBOL)
        return (e->op.symbol >= SYM_A && e->op.symbol <= SYM_Z) || e->op.symbol == SYM_THETA;
    if(e->type == NODE_OPERATOR)
        for(c = ast_ChildGet(e, 0); c != NULL; c = c->next)
            if(has_letter(c))
                return true;
    return false;
}

/*the ones only real on part of the line. sqrt(-1), ln(-3) and asin(2) aren't numbers*/
static bool narrow(OperatorType op) {
    switch(op) {
    case OP_POW: case OP_ROOT: case OP_LOG: case OP_SIN_INV: case OP_COS_INV:
    case OP_SEC_INV: case OP_CSC_INV: case OP_COSH_INV: case OP_TANH_INV:
        return true;
    default:
        return false;
    }
}

/*v came out a real number. with a letter still in it there's no telling, so yes*/
static bool real(pcas_ast_t *v) {
    pcas_ast_t *b = NULL, *p = NULL;
    double d;

    if(v == NULL || has_imaginary_node(v))
        return false;
    if(has_letter(v))
        return true;
    if(isoptype(v, OP_POW)) {
        b = ast_ChildGet(v, 0);
        p = ast_ChildGet(v, 1);
    } else if(isoptype(v, OP_ROOT)) {
        b = ast_ChildGet(v, 1);
        p = ast_ChildGet(v, 0);
    }
    /*a number to a number's power goes by the base's sign and the power's bottom. as a decimal (-1)^(1/2) took 7 s on the calc*/
    if(b != NULL && b->type == NODE_NUMBER && p->type == NODE_NUMBER) {
        if(mp_rat_compare_zero(b->op.num) >= 0)
            return true;
        /*x^(a/b) for a power, the n of an nth root*/
        return mp_int_is_odd(isoptype(v, OP_POW) ? mp_rat_denom_ref(p->op.num) : mp_rat_numer_ref(p->op.num));
    }
    return approx_double(v, &d);
}

/*just the numbers folded, a full simplify of sqrt(-1) takes the long way to i*/
static bool real_at(pcas_ast_t *e, pcas_ast_t *var, pcas_ast_t *a) {
    pcas_ast_t *v = ast_Copy(e);
    bool is;

    if(v != NULL) {
        substitute(v, var, a);
        simplify(v, SIMP_EVAL);
    }
    is = !pcas_cancelled && real(v);

    ast_Cleanup(v);
    return is;
}

/*a value at var = a on paper: no 0 below, no ln(0) or log base 0 / 1, no 0^0 or 0^-1, no tan(pi/2). on the tree as it is, a
simplify after eats it, 0*e^(1/0) came out 0*/
static bool defined_at(pcas_ast_t *e, pcas_ast_t *var, pcas_ast_t *a) {
    pcas_ast_t *c, *v, *base, *trig;
    bool ok = true;

    if(e == NULL || pcas_cancelled) /*oom, the limit is given up on*/
        return false;
    if(e->type != NODE_OPERATOR)
        return true;
    for(c = ast_ChildGet(e, 0); c != NULL; c = c->next)
        if(!defined_at(c, var, a))
            return false;
    if(!has_symbol(e, var->op.symbol))
        return true;

    switch(optype(e)) {
    case OP_DIV:
        return !is_int_at(ast_ChildGet(e, 1), var, a, 0);
    case OP_LOG:
        base = ast_ChildGet(e, 0);
        if(has_symbol(base, var->op.symbol) && (is_int_at(base, var, a, 0) || is_int_at(base, var, a, 1)))
            return false;
        return !is_int_at(ast_ChildGet(e, 1), var, a, 0) && real_at(e, var, a);
    case OP_POW:
        if(!is_int_at(ast_ChildGet(e, 0), var, a, 0)) { /*a whole power is real anywhere, x^(1/2) isn't*/
            c = ast_ChildGet(e, 1);
            return (c->type == NODE_NUMBER && mp_rat_is_integer(c->op.num)) || real_at(e, var, a);
        }
        v = value_at(ast_ChildGet(e, 1), var, a);
        ok = v != NULL && v->type == NODE_NUMBER && mp_rat_compare_zero(v->op.num) > 0;
        ast_Cleanup(v);
        return ok;
    case OP_TAN: case OP_SEC: case OP_COT: case OP_CSC:
        trig = ast_MakeUnary(optype(e) == OP_TAN || optype(e) == OP_SEC ? OP_COS : OP_SIN, ast_Copy(ast_ChildGet(e, 0)));
        ok = !is_int_at(trig, var, a, 0);
        ast_Cleanup(trig);
        return ok;
    default:
        return !narrow(optype(e)) || real_at(e, var, a);
    }
}

/*lim of f as var -> a, a line for the steps. new*/
static pcas_ast_t *lim(pcas_ast_t *f, pcas_ast_t *var, pcas_ast_t *a, int dir) {
    pcas_ast_t *l = ast_MakeOperator(OP_LIMIT);

    if(l == NULL)
        return NULL;
    ast_ChildAppend(l, ast_Copy(f));
    ast_ChildAppend(l, ast_Copy(var));
    ast_ChildAppend(l, ast_Copy(a));
    if(dir != 0)
        ast_ChildAppend(l, ast_MakeInt(dir));
    return l;
}

/*on the way to e, by rule, when there's steps to keep. takes e*/
static step_t *step(steps_t *s, step_t *l, const char *rule, pcas_ast_t *e) {
    if(s == NULL) {
        ast_Cleanup(e);
        return NULL;
    }
    if(rule != NULL)
        steps_rule(l, rule);
    return steps_expr(s, e);
}

/*plugged in: e with a in it as written, then v. hands v back*/
static pcas_ast_t *plug_in(steps_t *s, step_t *l, pcas_ast_t *e, pcas_ast_t *var, pcas_ast_t *a, pcas_ast_t *v) {
    pcas_ast_t *raw;

    if(s == NULL)
        return v;
    raw = ast_Copy(e);
    if(raw != NULL)
        substitute(raw, var, a);
    l = step(s, l, "plug in", raw);
    step(s, l, NULL, ast_Copy(v));
    return v;
}

/*--- x -> inf ---*/

/*where a piece of f heads as x -> inf. FAR_FIN hands the value back, new*/
#define FAR_FIN  0
#define FAR_UP   1
#define FAR_DOWN (-1)
#define FAR_NONE 2 /*no limit, or none this can find*/

/*sign of a value with no x in it, 2 when it can't tell (letters, i)*/
static int sign_of(pcas_ast_t *v) {
    pcas_ast_t *c, *n;
    int s = 2;

    if(v->type == NODE_NUMBER) /*most of them, no simplify for that*/
        return mp_rat_compare_zero(v->op.num) < 0 ? -1 : mp_rat_compare_zero(v->op.num) > 0;
    if((c = ast_Copy(v)) == NULL)
        return 2;
    simplify(c, SIMP_ALL);
    if(is_ast_int(c, 0))
        s = 0;
    else if((n = approx_number(c)) != NULL) {
        s = mp_rat_compare_zero(n->op.num) < 0 ? -1 : 1;
        ast_Cleanup(n);
    }
    ast_Cleanup(c);
    return s;
}

/*the side e is on once x is big, x = 10^4. which way a piece comes in to 0, -1/x from below*/
static int sign_far(pcas_ast_t *e, pcas_ast_t *var) {
    pcas_ast_t *c = ast_Copy(e), *big = ast_MakeInt(10000);
    int s = 2;

    if(c != NULL && big != NULL && substitute(c, var, big))
        s = sign_of(c);
    ast_Cleanup(c);
    ast_Cleanup(big);
    return s;
}

static int fin(pcas_ast_t **v, pcas_ast_t *e) {
    *v = e;
    return e != NULL ? FAR_FIN : FAR_NONE;
}

/*fin, but asin(2) and (-1)^(1/2) aren't somewhere to head to. takes e*/
static int fin_real(pcas_ast_t **v, pcas_ast_t *e) {
    if(e != NULL && e->type == NODE_OPERATOR && narrow(optype(e)) && !real(e)) {
        ast_Cleanup(e);
        *v = NULL;
        return FAR_NONE;
    }
    return fin(v, e);
}

static int far(pcas_ast_t *e, pcas_ast_t *var, pcas_ast_t **v, int tries);

/*inf/inf or 0/0, slope over slope. simplified first, (-1/x^2)/(-1/x^2) is just 1*/
static int lhopital(pcas_ast_t *top, pcas_ast_t *bottom, pcas_ast_t *var, pcas_ast_t **v, int tries) {
    pcas_ast_t *g;
    int out;

    if(tries <= 0)
        return FAR_NONE;
    g = ast_MakeBinary(OP_DIV, slope_of(top, var), slope_of(bottom, var));
    if(g == NULL)
        return FAR_NONE;
    simplify(g, SIMP_ALL);
    out = far(g, var, v, tries - 1);
    ast_Cleanup(g);
    return out;
}

#define DEG_MAX 10000L /*n and d stay under it, so n*d < 10^8 fits the CE's 32 bit long*/

/*a degree n/d. sqrt(x^2+x) grows like x^1, sqrt(x) like x^(1/2)*/
typedef struct {
    long n, d;
} deg_t;

static deg_t deg_make(long n, long d) {
    deg_t r;
    long g = gcd_small(n, d);

    if(g < 1)
        g = 1;
    r.n = n / g;
    r.d = d / g;
    return r;
}

/*<0, 0, >0 like strcmp. d is always > 0*/
static long deg_cmp(deg_t a, deg_t b) {
    return a.n * b.d - b.n * a.d;
}

static bool deg_ok(deg_t a) {
    return a.n <= DEG_MAX && a.n >= -DEG_MAX && a.d <= DEG_MAX;
}

/*a plain number or n/m, as a fraction*/
static bool exponent(pcas_ast_t *e, long *n, long *d) {
    mp_small a, b;
    long p, q, r, s;

    if(e->type == NODE_NUMBER) {
        if(mp_rat_to_ints(e->op.num, &a, &b) != MP_OK || a > DEG_MAX || a < -DEG_MAX || b > DEG_MAX)
            return false;
        *n = a;
        *d = b;
        return true;
    }
    if(!isoptype(e, OP_DIV) || ast_ChildLength(e) != 2 || !exponent(ast_ChildGet(e, 0), &p, &q)
       || !exponent(ast_ChildGet(e, 1), &r, &s) || r == 0)
        return false;
    *n = r < 0 ? -p * s : p * s;
    *d = r < 0 ? -q * r : q * r;
    return *n <= DEG_MAX && *n >= -DEG_MAX && *d <= DEG_MAX;
}

/*e as x -> inf grows like lead * x^deg, lead new. false when it can't say: sin(x), e^x, ln(x), a sum whose top terms cancel*/
static bool growth(pcas_ast_t *e, pcas_ast_t *var, deg_t *deg, pcas_ast_t **lead) {
    pcas_ast_t *c, *l;
    deg_t cd;
    long n, d;

    *lead = NULL;
    deg->n = 0;
    deg->d = 1;
    if(pcas_cancelled)
        return false;
    if(!has_symbol(e, var->op.symbol))
        return (*lead = ast_Copy(e)) != NULL;
    if(e->type == NODE_SYMBOL) {
        deg->n = 1;
        return (*lead = ast_MakeInt(1)) != NULL;
    }

    switch(optype(e)) {
    case OP_MULT: case OP_DIV:
        *lead = ast_MakeOperator(OP_MULT);
        for(c = ast_ChildGet(e, 0); c != NULL && *lead != NULL; c = c->next) {
            if(!growth(c, var, &cd, &l))
                break;
            if(isoptype(e, OP_DIV) && c != ast_ChildGet(e, 0)) {
                cd.n = -cd.n;
                l = ast_MakeBinary(OP_DIV, ast_MakeInt(1), l);
            }
            *deg = deg_make(deg->n * cd.d + cd.n * deg->d, deg->d * cd.d);
            if(!deg_ok(*deg)) {
                ast_Cleanup(l);
                break;
            }
            ast_ChildAppend(*lead, l);
        }
        if(c == NULL && !pcas_cancelled)
            return true;
        break;
    case OP_POW: /*only a number up there, x^(1/2). e^x has no degree*/
        if(!exponent(ast_ChildGet(e, 1), &n, &d) || !growth(ast_ChildGet(e, 0), var, &cd, &l))
            return false;
        *deg = deg_make(cd.n * n, cd.d * d);
        if((d != 1 && sign_of(l) != 1) || !deg_ok(*deg)) { /*sqrt of a top term that goes negative*/
            ast_Cleanup(l);
            return false;
        }
        return (*lead = ast_MakeBinary(OP_POW, l, ast_Copy(ast_ChildGet(e, 1)))) != NULL;
    case OP_ROOT: /*root(n, b)*/
        if(!exponent(ast_ChildGet(e, 0), &n, &d) || d != 1 || n < 1 || !growth(ast_ChildGet(e, 1), var, &cd, &l))
            return false;
        *deg = deg_make(cd.n, cd.d * n);
        if(sign_of(l) != 1 || !deg_ok(*deg)) {
            ast_Cleanup(l);
            return false;
        }
        return (*lead = ast_MakeBinary(OP_ROOT, ast_Copy(ast_ChildGet(e, 0)), l)) != NULL;
    case OP_ABS:
        if(!growth(ast_ChildGet(e, 0), var, deg, &l))
            return false;
        return (*lead = ast_MakeUnary(OP_ABS, l)) != NULL;
    case OP_ADD: /*the leads with the top degree, a higher one starts them over*/
        for(c = ast_ChildGet(e, 0); c != NULL; c = c->next) {
            if(!growth(c, var, &cd, &l))
                break;
            if(c == ast_ChildGet(e, 0) || deg_cmp(cd, *deg) > 0) {
                ast_Cleanup(*lead);
                *lead = ast_MakeOperator(OP_ADD);
                *deg = cd;
            }
            if(deg_cmp(cd, *deg) < 0 || ast_ChildAppend(*lead, l) != E_SUCCESS)
                ast_Cleanup(l);
        }
        if(c == NULL && !pcas_cancelled && sign_of(*lead) != 0) /*0 is the top terms cancelling, sqrt(x^2+x) - x*/
            return true;
        break;
    default:
        return false;
    }
    ast_Cleanup(*lead);
    *lead = NULL;
    return false;
}

/*a root somewhere in e, the conjugate is for those*/
static bool has_root(pcas_ast_t *e) {
    pcas_ast_t *c;
    long n, d;

    if(isoptype(e, OP_ROOT) || (isoptype(e, OP_POW) && exponent(ast_ChildGet(e, 1), &n, &d) && d != 1))
        return true;
    if(e->type == NODE_OPERATOR)
        for(c = ast_ChildGet(e, 0); c != NULL; c = c->next)
            if(has_root(c))
                return true;
    return false;
}

static int far_ratio(pcas_ast_t *top, pcas_ast_t *bottom, pcas_ast_t *var, pcas_ast_t **v, int tries) {
    pcas_ast_t *t = NULL, *b = NULL, *lt = NULL, *lb = NULL;
    deg_t dt, db;
    int ft = far(top, var, &t, tries), fb = far(bottom, var, &b, tries), out = FAR_NONE, s;
    long c;

    *v = NULL;
    if(ft == FAR_NONE || fb == FAR_NONE) {
        out = FAR_NONE;
    } else if(ft == FAR_FIN && fb == FAR_FIN) {
        s = sign_of(b);
        if(s == 1 || s == -1) {
            out = fin(v, ast_MakeBinary(OP_DIV, t, b));
            t = b = NULL;
        } else if(s == 0 && sign_of(t) == 0) {
            out = lhopital(top, bottom, var, v, tries);
        }
    } else if(ft == FAR_FIN) {
        out = fin(v, ast_MakeInt(0));
    } else if(fb == FAR_FIN) {
        s = sign_of(b);
        if(s == 1 || s == -1)
            out = ft * s;
    } else {
        /*by top terms when both have a degree (polynomials, roots of them), anything else l'hopital*/
        if(growth(top, var, &dt, &lt) && growth(bottom, var, &db, &lb)) {
            c = deg_cmp(dt, db);
            if(c < 0) {
                out = fin(v, ast_MakeInt(0));
            } else if(c == 0) {
                out = fin(v, ast_MakeBinary(OP_DIV, lt, lb));
                lt = lb = NULL;
            } else {
                s = sign_of(lt) * sign_of(lb);
                if(s == 1 || s == -1)
                    out = s;
            }
        } else {
            out = lhopital(top, bottom, var, v, tries);
        }
    }
    ast_Cleanup(t);
    ast_Cleanup(b);
    ast_Cleanup(lt);
    ast_Cleanup(lb);
    return out;
}

/*p + q as (p^2 - q^2)/(p - q), sqrt(x^2+x) - x is x/(sqrt(x^2+x) + x)*/
static int far_conjugate(pcas_ast_t *e, pcas_ast_t *var, pcas_ast_t **v, int tries) {
    pcas_ast_t *p = ast_ChildGet(e, 0), *q = ast_ChildGet(e, 1), *top, *bottom;
    int out = FAR_NONE;

    top = ast_MakeBinary(OP_ADD, ast_MakeBinary(OP_POW, ast_Copy(p), ast_MakeInt(2)),
        ast_MakeBinary(OP_MULT, ast_MakeInt(-1), ast_MakeBinary(OP_POW, ast_Copy(q), ast_MakeInt(2))));
    bottom = ast_MakeBinary(OP_ADD, ast_Copy(p), ast_MakeBinary(OP_MULT, ast_MakeInt(-1), ast_Copy(q)));
    if(top != NULL && bottom != NULL) {
        simplify(top, SIMP_LIGHT); /*sqrt(..)^2 back to what's inside*/
        expand(top, EXP_ALL);
        simplify(top, SIMP_LIGHT | SIMP_LIKE_TERMS); /*x^2+x-x^2 is x*/
        simplify(bottom, SIMP_LIGHT);
        out = far_ratio(top, bottom, var, v, tries - 1);
    }
    ast_Cleanup(top);
    ast_Cleanup(bottom);
    return out;
}

/*inf - inf. by the top terms if they don't cancel, else the conjugate, else as one fraction, else up over down*/
static int far_gap(pcas_ast_t *e, pcas_ast_t *var, pcas_ast_t **v, int tries) {
    pcas_ast_t *lead = NULL, *g, *up, *down, *c, *r = NULL;
    deg_t deg;
    int out = FAR_NONE, s, f;

    if(growth(e, var, &deg, &lead)) {
        s = sign_of(lead);
        ast_Cleanup(lead);
        return s == 1 || s == -1 ? s : FAR_NONE;
    }
    if(tries <= 0)
        return FAR_NONE;
    if(ast_ChildLength(e) == 2 && has_root(e) && (out = far_conjugate(e, var, v, tries)) != FAR_NONE)
        return out;
    if((g = ast_Copy(e)) == NULL)
        return FAR_NONE;
    comdenom(g, true);
    if(isoptype(g, OP_DIV))
        out = far_ratio(ast_ChildGet(g, 0), ast_ChildGet(g, 1), var, v, tries - 1);
    ast_Cleanup(g);
    if(out != FAR_NONE)
        return out;

    /*ln(x) - x: up(1 - down/up), it's the 1 - down/up that says which wins*/
    up = ast_MakeOperator(OP_ADD);
    down = ast_MakeOperator(OP_ADD);
    for(c = ast_ChildGet(e, 0); c != NULL && up != NULL && down != NULL; c = c->next) {
        f = far(c, var, &r, 0);
        ast_Cleanup(r);
        if(f == FAR_UP)
            ast_ChildAppend(up, ast_Copy(c));
        else if(f == FAR_DOWN)
            ast_ChildAppend(down, ast_MakeBinary(OP_MULT, ast_MakeInt(-1), ast_Copy(c)));
    }
    g = ast_MakeBinary(OP_ADD, ast_MakeInt(1), ast_MakeBinary(OP_MULT, ast_MakeInt(-1), ast_MakeBinary(OP_DIV, down, up)));
    if(g != NULL) {
        simplify(g, SIMP_ALL);
        f = far(g, var, &r, tries - 1);
        s = f == FAR_FIN ? sign_of(r) : f;
        if(s == 1 || s == -1) /*up times something that stays the same side of 0*/
            out = s;
        ast_Cleanup(r);
        ast_Cleanup(g);
    }
    return out;
}

static int far_sum(pcas_ast_t *e, pcas_ast_t *var, pcas_ast_t **v, int tries) {
    pcas_ast_t *c, *cv, *sum = ast_MakeOperator(OP_ADD);
    bool up = false, down = false;
    int f;

    if(sum == NULL)
        return FAR_NONE;
    for(c = ast_ChildGet(e, 0); c != NULL; c = c->next) {
        f = far(c, var, &cv, tries);
        if(f == FAR_NONE) {
            ast_Cleanup(sum);
            return FAR_NONE;
        }
        if(f == FAR_FIN)
            ast_ChildAppend(sum, cv);
        up |= f == FAR_UP;
        down |= f == FAR_DOWN;
    }
    if(up || down) {
        ast_Cleanup(sum);
        return up && down ? far_gap(e, var, v, tries) : up ? FAR_UP : FAR_DOWN;
    }
    return fin(v, sum);
}

/*0 * inf: rest over 1/zero (x e^-x), or zero over 1/rest (x sin(1/x))*/
static int far_zero_inf(pcas_ast_t *e, pcas_ast_t *zero, pcas_ast_t *var, pcas_ast_t **v, int tries) {
    pcas_ast_t *rest = ast_Copy(e), *inv;
    unsigned i;
    int out = FAR_NONE, k;

    if(rest == NULL || tries <= 0) {
        ast_Cleanup(rest);
        return FAR_NONE;
    }
    for(i = 0; ast_ChildGet(e, i) != zero; i++)
        ;
    ast_Cleanup(ast_ChildRemoveIndex(rest, i));
    for(k = 0; k < 2 && out == FAR_NONE && !pcas_cancelled; k++) {
        inv = ast_MakeBinary(OP_DIV, ast_MakeInt(1), ast_Copy(k == 0 ? zero : rest));
        if(inv == NULL)
            break;
        simplify(inv, SIMP_ALL);
        out = far_ratio(k == 0 ? rest : zero, inv, var, v, tries - 1);
        ast_Cleanup(inv);
    }
    ast_Cleanup(rest);
    return out;
}

static int far_product(pcas_ast_t *e, pcas_ast_t *var, pcas_ast_t **v, int tries) {
    pcas_ast_t *c, *cv, *prod = ast_MakeOperator(OP_MULT), *zero = NULL;
    bool inf = false, unsure = false;
    int sign = 1, f, s;

    if(prod == NULL)
        return FAR_NONE;
    for(c = ast_ChildGet(e, 0); c != NULL; c = c->next) {
        f = far(c, var, &cv, tries);
        if(f == FAR_NONE) {
            ast_Cleanup(prod);
            return FAR_NONE;
        }
        if(f != FAR_FIN) {
            inf = true;
            sign *= f;
            continue;
        }
        s = sign_of(cv);
        if(s == 0 && zero == NULL)
            zero = c;
        unsure |= s == 2;
        sign *= s == 2 ? 1 : s;
        ast_ChildAppend(prod, cv);
    }
    if(!inf)
        return fin(v, prod);
    ast_Cleanup(prod);
    if(zero != NULL)
        return far_zero_inf(e, zero, var, v, tries);
    return unsure ? FAR_NONE : sign;
}

static int far_pow(pcas_ast_t *base, pcas_ast_t *ex, pcas_ast_t *var, pcas_ast_t **v, int tries) {
    pcas_ast_t *b = NULL, *x = NULL, *g;
    int fb, fx, out = FAR_NONE, s;
    mp_small n, den;

    if(has_symbol(base, var->op.symbol) && has_symbol(ex, var->op.symbol)) {
        /*x^(1/x), (1+1/x)^x as e^(ex ln(base))*/
        g = ast_MakeBinary(OP_MULT, ast_Copy(ex), ast_MakeBinary(OP_LOG, ast_MakeSymbol(SYM_EULER), ast_Copy(base)));
        if(g == NULL)
            return FAR_NONE;
        simplify(g, SIMP_ALL);
        fx = far(g, var, &x, tries);
        ast_Cleanup(g);
        if(fx == FAR_FIN)
            return fin(v, ast_MakeBinary(OP_POW, ast_MakeSymbol(SYM_EULER), x));
        return fx == FAR_DOWN ? fin(v, ast_MakeInt(0)) : fx;
    }

    fb = far(base, var, &b, tries);
    fx = far(ex, var, &x, tries);
    if(fb == FAR_NONE || fx == FAR_NONE) {
    } else if(fx == FAR_FIN) { /*x only in the base*/
        s = sign_of(x);
        if(fb == FAR_FIN) {
            if(sign_of(b) != 0 || s == 1) { /*no 0^-1*/
                out = fin_real(v, ast_MakeBinary(OP_POW, b, x));
                b = x = NULL;
            }
        } else if(s == 0) {
            out = fin(v, ast_MakeInt(1));
        } else if(s == -1) {
            out = fin(v, ast_MakeInt(0));
        } else if(s == 1 && fb == FAR_UP) {
            out = FAR_UP;
        } else if(s == 1 && x->type == NODE_NUMBER && mp_rat_to_ints(x->op.num, &n, &den) == MP_OK && den == 1) {
            out = n % 2 == 0 ? FAR_UP : FAR_DOWN; /*(-inf)^3*/
        }
    } else if(sign_of(b) == 1) { /*x only in the power, 2^x*/
        g = ast_MakeBinary(OP_ADD, ast_Copy(b), ast_MakeInt(-1));
        s = g != NULL ? sign_of(g) : 2;
        ast_Cleanup(g);
        if(s == 0)
            out = fin(v, ast_MakeInt(1));
        else if(s == 1 || s == -1)
            out = s * fx == 1 ? FAR_UP : fin(v, ast_MakeInt(0));
    }
    ast_Cleanup(b);
    ast_Cleanup(x);
    return out;
}

/*e as var -> inf. tries is what's left for l'hopital and the rewrites*/
static int far(pcas_ast_t *e, pcas_ast_t *var, pcas_ast_t **v, int tries) {
    pcas_ast_t *c, *cv, *g;
    int f, out;

    *v = NULL;
    if(e == NULL || pcas_cancelled)
        return FAR_NONE;
    if(!has_symbol(e, var->op.symbol))
        return fin(v, ast_Copy(e));
    if(e->type == NODE_SYMBOL)
        return FAR_UP;

    switch(optype(e)) {
    case OP_ADD:  return far_sum(e, var, v, tries);
    case OP_MULT: return far_product(e, var, v, tries);
    case OP_DIV:  return far_ratio(ast_ChildGet(e, 0), ast_ChildGet(e, 1), var, v, tries);
    case OP_POW:  return far_pow(ast_ChildGet(e, 0), ast_ChildGet(e, 1), var, v, tries);
    case OP_ROOT: /*sqrt(x) as x^(1/2)*/
        g = ast_MakeBinary(OP_DIV, ast_MakeInt(1), ast_Copy(ast_ChildGet(e, 0)));
        out = g != NULL ? far_pow(ast_ChildGet(e, 1), g, var, v, tries) : FAR_NONE;
        ast_Cleanup(g);
        return out;
    case OP_LOG:
        if(has_symbol(ast_ChildGet(e, 0), var->op.symbol))
            return FAR_NONE;
        f = far(ast_ChildGet(e, 1), var, &cv, tries);
        out = f == FAR_FIN ? sign_of(cv) : 2;
        if(out == 1)
            return fin(v, ast_MakeBinary(OP_LOG, ast_Copy(ast_ChildGet(e, 0)), cv));
        ast_Cleanup(cv);
        if((f != FAR_UP && out != 0) || (out == 0 && sign_far(ast_ChildGet(e, 1), var) != 1)) /*to 0 from above, from below isn't real*/
            return FAR_NONE;
        g = ast_MakeBinary(OP_ADD, ast_Copy(ast_ChildGet(e, 0)), ast_MakeInt(-1)); /*a base under 1 heads the other way*/
        f = f == FAR_UP ? 1 : -1;
        out = g != NULL ? sign_of(g) : 2;
        ast_Cleanup(g);
        return out == 1 || out == -1 ? out * f : FAR_NONE;
    case OP_TAN_INV: case OP_COT_INV: case OP_SEC_INV: case OP_CSC_INV:
        f = far(ast_ChildGet(e, 0), var, &cv, tries);
        if(f == FAR_FIN)
            return fin_real(v, ast_MakeUnary(optype(e), cv));
        if(f == FAR_NONE)
            return FAR_NONE;
        if(optype(e) == OP_TAN_INV)
            return fin(v, ast_MakeBinary(OP_MULT, ast_MakeInt(f), ast_MakeBinary(OP_DIV, ast_MakeSymbol(SYM_PI), ast_MakeInt(2))));
        if(optype(e) == OP_SEC_INV)
            return fin(v, ast_MakeBinary(OP_DIV, ast_MakeSymbol(SYM_PI), ast_MakeInt(2)));
        return fin(v, optype(e) == OP_COT_INV && f == FAR_DOWN ? ast_MakeSymbol(SYM_PI) : ast_MakeInt(0)); /*cot^-1 is 0 to pi like the nspire*/
    case OP_ABS:
        f = far(ast_ChildGet(e, 0), var, &cv, tries);
        if(f == FAR_FIN)
            return fin(v, ast_MakeUnary(OP_ABS, cv));
        return f == FAR_NONE ? FAR_NONE : FAR_UP;
    default: /*sin, cos, ... of something that settles. sin(x) doesn't*/
        g = ast_MakeOperator(optype(e));
        for(c = ast_ChildGet(e, 0); c != NULL && g != NULL; c = c->next) {
            if(far(c, var, &cv, tries) != FAR_FIN) {
                ast_Cleanup(g);
                return FAR_NONE;
            }
            ast_ChildAppend(g, cv);
        }
        if(g != NULL && (optype(e) == OP_TAN || optype(e) == OP_SEC || optype(e) == OP_COT || optype(e) == OP_CSC)) { /*tan(pi/2) isn't one*/
            c = ast_MakeUnary(optype(e) == OP_TAN || optype(e) == OP_SEC ? OP_COS : OP_SIN, ast_Copy(ast_ChildGet(g, 0)));
            f = c != NULL ? sign_of(c) : 0;
            ast_Cleanup(c);
            if(f == 0) {
                ast_Cleanup(g);
                return FAR_NONE;
            }
        }
        return fin_real(v, g);
    }
}

/*lim at inf or -inf, new. an infinate one only for root*/
static pcas_ast_t *limit_far(pcas_ast_t *f, pcas_ast_t *var, int side, bool root) {
    pcas_ast_t *g = ast_Copy(f), *v = NULL;
    int out;

    if(g == NULL)
        return NULL;
    if(side < 0) { /*-inf is inf with x turned around*/
        pcas_ast_t *neg = ast_MakeBinary(OP_MULT, ast_MakeInt(-1), ast_Copy(var));

        if(neg != NULL)
            substitute(g, var, neg);
        ast_Cleanup(neg);
    }
    simplify(g, SIMP_LIGHT | SIMP_LIKE_TERMS); /*the full one spreads (3x^2+x)/(2x^2-5) into two fractions first, for nothing*/
    out = far(g, var, &v, TRIES_MAX);
    ast_Cleanup(g);
    if(pcas_cancelled) {
        ast_Cleanup(v);
        return NULL;
    }
    if(out == FAR_FIN) {
        simplify(v, SIMP_ALL);
        return v;
    }
    if(out == FAR_NONE || !root)
        return NULL;
    v = ast_MakeSymbol(SYM_INF);
    return out == FAR_UP ? v : ast_MakeBinary(OP_MULT, ast_MakeInt(-1), v);
}

/*each term of p over x^n, tidied: 3x^2/x^2 is 3, x/x^2 is 1/x. new*/
static pcas_ast_t *spread(pcas_ast_t *p, pcas_ast_t *var, int n) {
    pcas_ast_t *e = ast_Copy(p), *out, *c, *t;

    if(e == NULL)
        return NULL;
    out = ast_MakeOperator(OP_ADD);
    expand(e, EXP_ALL);
    simplify(e, SIMP_LIGHT | SIMP_LIKE_TERMS);
    for(c = isoptype(e, OP_ADD) ? ast_ChildGet(e, 0) : e; c != NULL && out != NULL; c = c == e ? NULL : c->next) {
        t = ast_MakeBinary(OP_DIV, ast_Copy(c), ast_MakeBinary(OP_POW, ast_Copy(var), ast_MakeInt(n)));
        simplify(t, SIMP_LIGHT);
        ast_ChildAppend(out, t);
    }
    ast_Cleanup(e);
    return out;
}

/*the paper way for a polynomial over a polynomial at inf: top and bottom over the bottom's top power. the new line, or l when it isn't one of those*/
static step_t *over_power(steps_t *s, step_t *l, pcas_ast_t *f, pcas_ast_t *var, pcas_ast_t *a) {
    pcas_ast_t *g;
    int n;

    if(s == NULL || !isoptype(f, OP_DIV) || (n = poly_degree(ast_ChildGet(f, 1), var->op.symbol, NULL)) < 1
       || poly_degree(ast_ChildGet(f, 0), var->op.symbol, NULL) < 0)
        return l;
    g = ast_MakeBinary(OP_DIV, spread(ast_ChildGet(f, 0), var, n), spread(ast_ChildGet(f, 1), var, n));
    l = step(s, l, "divide by the top power", lim(g, var, a, 0));
    ast_Cleanup(g);
    return l;
}

/*from one side, dir 1 the right and -1 the left: f(a + dir/x) as x -> inf. e^(1/x) at 0 is inf from the right, 0 from the left*/
static pcas_ast_t *one_side(pcas_ast_t *f, pcas_ast_t *var, pcas_ast_t *a, int dir, bool root) {
    pcas_ast_t *g = ast_Copy(f), *to, *out = NULL;

    to = ast_MakeBinary(OP_ADD, ast_Copy(a), ast_MakeBinary(OP_DIV, ast_MakeInt(dir), ast_Copy(var)));
    if(g != NULL && to != NULL && substitute(g, var, to)) {
        simplify(g, SIMP_ALL); /*1/(1/x) is x, limit_far only does the light one*/
        out = limit_far(g, var, 1, root);
    }
    ast_Cleanup(g);
    ast_Cleanup(to);
    return out;
}

/*when the usual way finds nothing: the side asked for, or both sides when they agree (1/x^2 at 0). l the line to put the rule on*/
static pcas_ast_t *sides(pcas_ast_t *f, pcas_ast_t *var, pcas_ast_t *a, int dir, bool root, steps_t *s, step_t *l) {
    pcas_ast_t *r, *left = NULL;

    if(pcas_cancelled || (r = one_side(f, var, a, dir != 0 ? dir : 1, root)) == NULL)
        return NULL;
    if(dir == 0 && ((left = one_side(f, var, a, -1, root)) == NULL || !ast_Compare(r, left))) {
        ast_Cleanup(r);
        ast_Cleanup(left);
        return NULL;
    }
    ast_Cleanup(left);
    step(s, l, dir > 0 ? "from the right" : dir < 0 ? "from the left" : "both sides", ast_Copy(r));
    return r;
}

/*new, NULL when there isn't one or it gives up. s NULL for no steps, root when the limit is the whole input, dir 1 / -1 from one side*/
static pcas_ast_t *limit_of(pcas_ast_t *f, pcas_ast_t *var, pcas_ast_t *a, steps_t *s, bool root, int dir) {
    pcas_ast_t *g, *top, *bottom, *t, *b, *out = NULL;
    step_t *l = NULL;
    bool cancelled;
    int tries;

    if(s != NULL && (t = lim(f, var, a, dir)) != NULL) {
        simplify(ast_ChildGet(t, 0), SIMP_LIGHT); /*x^2_1 parses as x^2+-1*1, it showed a lone minus*/
        l = steps_expr(s, t);
    }
    if(inf_sign(a) != 0) {
        out = limit_far(f, var, inf_sign(a), root);
        if(out != NULL) {
            step_t *before = l;

            l = over_power(s, l, f, var, a);
            step(s, l, l != before ? "1/x -> 0" : "plug in inf", ast_Copy(out));
        }
        return out;
    }
    if(has_symbol(a, SYM_INF)) /*inf+1, 2inf*/
        return NULL;
    g = ast_Copy(f);
    if(defined_at(f, var, a)) {
        ast_Cleanup(g);
        return plug_in(s, l, f, var, a, value_at(f, var, a));
    }
    if(g == NULL || pcas_cancelled) {
        ast_Cleanup(g);
        return NULL;
    }
    comdenom(g, true); /*one fraction, top and bottom factored*/
    if(!isoptype(g, OP_DIV)) { /*not a fraction and not defined there, e^(1/x) at 0*/
        ast_Cleanup(g);
        return sides(f, var, a, dir, root, s, l);
    }
    if(!ast_Compare(g, f))
        l = step(s, l, "one fraction", lim(g, var, a, dir));
    top = ast_Copy(ast_ChildGet(g, 0));
    bottom = ast_Copy(ast_ChildGet(g, 1));
    ast_Cleanup(g);

    for(tries = 0; tries < TRIES_MAX && !pcas_cancelled; tries++) {
        if(!defined_at(top, var, a) || !defined_at(bottom, var, a)) /*x e^(1/x) over 1, no limit to find this way*/
            break;
        t = value_at(top, var, a);
        b = value_at(bottom, var, a);
        if(t == NULL || b == NULL) {
            ast_Cleanup(t);
            ast_Cleanup(b);
            break;
        }

        if(!is_ast_int(b, 0)) {
            out = ast_MakeBinary(OP_DIV, t, b);
            simplify(out, SIMP_ALL);
            g = ast_MakeBinary(OP_DIV, ast_Copy(top), ast_Copy(bottom));
            plug_in(s, l, g, var, a, out);
            ast_Cleanup(g);
            break;
        }
        if(!is_ast_int(t, 0)) { /*something over 0, no limit*/
            ast_Cleanup(t);
            ast_Cleanup(b);
            break;
        }
        ast_Cleanup(t);
        ast_Cleanup(b);

        /*0/0: what top and bottom share comes off first, then l'hopital*/
        g = ast_MakeBinary(OP_DIV, ast_Copy(top), ast_Copy(bottom));
        if(g == NULL)
            break;
        cancelled = cancel_fractions(g);
        if(!isoptype(g, OP_DIV)) { /*cancelled all the way down*/
            l = step(s, l, "0/0, factor and cancel", lim(g, var, a, dir));
            out = defined_at(g, var, a) ? plug_in(s, l, g, var, a, value_at(g, var, a)) : NULL;
            ast_Cleanup(g);
            break;
        }
        if(cancelled) {
            ast_Cleanup(top);
            ast_Cleanup(bottom);
            top = ast_Copy(ast_ChildGet(g, 0));
            bottom = ast_Copy(ast_ChildGet(g, 1));
            l = step(s, l, "0/0, factor and cancel", lim(g, var, a, dir));
        } else {
            t = slope_of(top, var);
            b = slope_of(bottom, var);
            ast_Cleanup(top);
            ast_Cleanup(bottom);
            top = t;
            bottom = b;
            ast_Cleanup(g);
            g = ast_MakeBinary(OP_DIV, ast_Copy(top), ast_Copy(bottom));
            l = step(s, l, "0/0, l'Hopital", lim(g, var, a, dir));
        }
        ast_Cleanup(g);
    }
    ast_Cleanup(top);
    ast_Cleanup(bottom);
    return out != NULL ? out : sides(f, var, a, dir, root, s, l);
}

int limit_side(pcas_ast_t *e) {
    pcas_ast_t *d = ast_ChildGet(e, 3);

    if(d == NULL)
        return 0;
    if(isoptype(d, OP_MULT) && ast_ChildLength(d) == 2 && is_ast_int(ast_ChildGet(d, 0), -1) && is_ast_int(ast_ChildGet(d, 1), 1)) /*-1 as typed, the steps get it unsimplifyed*/
        return -1;
    return is_ast_int(d, 1) ? 1 : is_ast_int(d, -1) ? -1 : 2;
}

/*an i out of f and a with none in them is a root of a negative. simplify pulls those out front as i sqrt(..) before far or real() see them*/
static pcas_ast_t *real_limit(pcas_ast_t *f, pcas_ast_t *var, pcas_ast_t *a, steps_t *s, bool root, int dir) {
    pcas_ast_t *out = limit_of(f, var, a, s, root, dir);

    if(out != NULL && has_imaginary_node(out) && !has_imaginary_node(f) && !has_imaginary_node(a)) {
        ast_Cleanup(out);
        return NULL;
    }
    return out;
}

/*root: e is the whole thing simplify got. an inf anywhere else turns into inf - inf = 0*/
static bool limits(pcas_ast_t *e, bool root) {
    pcas_ast_t *child, *out;
    bool changed = false;

    if(pcas_cancelled || e->type != NODE_OPERATOR)
        return false;
    for(child = ast_ChildGet(e, 0); child != NULL; child = child->next)
        changed |= limits(child, false);
    if(!isoptype(e, OP_LIMIT) || ast_ChildGet(e, 1)->type != NODE_SYMBOL || limit_side(e) == 2)
        return changed;

    out = real_limit(ast_ChildGet(e, 0), ast_ChildGet(e, 1), ast_ChildGet(e, 2), NULL, root, limit_side(e));
    if(out == NULL)
        return changed;
    replace_node(e, out);
    return true;
}

bool eval_limit_nodes(pcas_ast_t *e) {
    return limits(e, true);
}

bool steps_limit(pcas_ast_t *l, steps_t *out) {
    pcas_ast_t *v;

    out->n = 0;
    if(!isoptype(l, OP_LIMIT) || ast_ChildGet(l, 1)->type != NODE_SYMBOL || limit_side(l) == 2)
        return false;
    v = real_limit(ast_ChildGet(l, 0), ast_ChildGet(l, 1), ast_ChildGet(l, 2), out, true, limit_side(l));
    if(v == NULL || pcas_cancelled) {
        ast_Cleanup(v);
        steps_free(out);
        return false;
    }
    ast_Cleanup(v);
    steps_shown(out);
    return true;
}
