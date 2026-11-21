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

/*d/dx of e, new and simplified. NULL the same way*/
static pcas_ast_t *slope(pcas_ast_t *e, pcas_ast_t *var) {
    pcas_ast_t *d = ast_Copy(e);

    if(d == NULL)
        return NULL;
    derivative(d, var, var);
    simplify(d, SIMP_ALL);
    if(pcas_cancelled) {
        ast_Cleanup(d);
        return NULL;
    }
    return d;
}

static bool is_int_at(pcas_ast_t *e, pcas_ast_t *var, pcas_ast_t *a, int n) {
    pcas_ast_t *v = value_at(e, var, a);
    bool is = v != NULL && is_ast_int(v, n);

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
        return !is_int_at(ast_ChildGet(e, 1), var, a, 0);
    case OP_POW:
        if(!is_int_at(ast_ChildGet(e, 0), var, a, 0))
            return true;
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
        return true;
    }
}

/*lim of f as var -> a, a line for the steps. new*/
static pcas_ast_t *lim(pcas_ast_t *f, pcas_ast_t *var, pcas_ast_t *a) {
    pcas_ast_t *l = ast_MakeOperator(OP_LIMIT);

    if(l == NULL)
        return NULL;
    ast_ChildAppend(l, ast_Copy(f));
    ast_ChildAppend(l, ast_Copy(var));
    ast_ChildAppend(l, ast_Copy(a));
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
    pcas_ast_t *c = ast_Copy(v), *n;
    int s = 2;

    if(c == NULL)
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

static int fin(pcas_ast_t **v, pcas_ast_t *e) {
    *v = e;
    return e != NULL ? FAR_FIN : FAR_NONE;
}

static int far(pcas_ast_t *e, pcas_ast_t *var, pcas_ast_t **v, int tries);

/*inf/inf or 0/0, slope over slope. simplified first, (-1/x^2)/(-1/x^2) is just 1*/
static int lhopital(pcas_ast_t *top, pcas_ast_t *bottom, pcas_ast_t *var, pcas_ast_t **v, int tries) {
    pcas_ast_t *g;
    int out;

    if(tries <= 0)
        return FAR_NONE;
    g = ast_MakeBinary(OP_DIV, slope(top, var), slope(bottom, var));
    if(g == NULL)
        return FAR_NONE;
    simplify(g, SIMP_ALL);
    out = far(g, var, v, tries - 1);
    ast_Cleanup(g);
    return out;
}

static int far_ratio(pcas_ast_t *top, pcas_ast_t *bottom, pcas_ast_t *var, pcas_ast_t **v, int tries) {
    pcas_ast_t *t = NULL, *b = NULL, *lt = NULL, *lb = NULL;
    int ft = far(top, var, &t, tries), fb = far(bottom, var, &b, tries), out = FAR_NONE, s, dt, db;

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
        out = fin(v, ast_MakeNumber(num_FromInt(0)));
    } else if(fb == FAR_FIN) {
        s = sign_of(b);
        if(s == 1 || s == -1)
            out = ft * s;
    } else {
        /*polynomials by their top terms, anything else l'hopital*/
        dt = poly_degree(top, var->op.symbol, &lt);
        db = poly_degree(bottom, var->op.symbol, &lb);
        if(dt > 0 && db > 0 && dt < db) {
            out = fin(v, ast_MakeNumber(num_FromInt(0)));
        } else if(dt > 0 && dt == db) {
            out = fin(v, ast_MakeBinary(OP_DIV, lt, lb));
            lt = lb = NULL;
        } else if(dt > 0 && db > 0) {
            s = sign_of(lt) * sign_of(lb);
            if(s == 1 || s == -1)
                out = s;
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

/*inf - inf. a polynomial goes by its top term, else as one fraction, else up over down*/
static int far_gap(pcas_ast_t *e, pcas_ast_t *var, pcas_ast_t **v, int tries) {
    pcas_ast_t *lead = NULL, *g, *up, *down, *c, *r = NULL;
    int deg = poly_degree(e, var->op.symbol, &lead), out = FAR_NONE, s, f;

    if(deg > 0) {
        s = sign_of(lead);
        ast_Cleanup(lead);
        return s == 1 || s == -1 ? s : FAR_NONE;
    }
    ast_Cleanup(lead);
    if(tries <= 0 || (g = ast_Copy(e)) == NULL)
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
            ast_ChildAppend(down, ast_MakeBinary(OP_MULT, ast_MakeNumber(num_FromInt(-1)), ast_Copy(c)));
    }
    g = ast_MakeBinary(OP_ADD, ast_MakeNumber(num_FromInt(1)), ast_MakeBinary(OP_MULT, ast_MakeNumber(num_FromInt(-1)), ast_MakeBinary(OP_DIV, down, up)));
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
        inv = ast_MakeBinary(OP_DIV, ast_MakeNumber(num_FromInt(1)), ast_Copy(k == 0 ? zero : rest));
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
        return fx == FAR_DOWN ? fin(v, ast_MakeNumber(num_FromInt(0))) : fx;
    }

    fb = far(base, var, &b, tries);
    fx = far(ex, var, &x, tries);
    if(fb == FAR_NONE || fx == FAR_NONE) {
    } else if(fx == FAR_FIN) { /*x only in the base*/
        s = sign_of(x);
        if(fb == FAR_FIN) {
            if(sign_of(b) != 0 || s == 1) { /*no 0^-1*/
                out = fin(v, ast_MakeBinary(OP_POW, b, x));
                b = x = NULL;
            }
        } else if(s == 0) {
            out = fin(v, ast_MakeNumber(num_FromInt(1)));
        } else if(s == -1) {
            out = fin(v, ast_MakeNumber(num_FromInt(0)));
        } else if(s == 1 && fb == FAR_UP) {
            out = FAR_UP;
        } else if(s == 1 && x->type == NODE_NUMBER && mp_rat_to_ints(x->op.num, &n, &den) == MP_OK && den == 1) {
            out = n % 2 == 0 ? FAR_UP : FAR_DOWN; /*(-inf)^3*/
        }
    } else if(sign_of(b) == 1) { /*x only in the power, 2^x*/
        g = ast_MakeBinary(OP_ADD, ast_Copy(b), ast_MakeNumber(num_FromInt(-1)));
        s = g != NULL ? sign_of(g) : 2;
        ast_Cleanup(g);
        if(s == 0)
            out = fin(v, ast_MakeNumber(num_FromInt(1)));
        else if(s == 1 || s == -1)
            out = s * fx == 1 ? FAR_UP : fin(v, ast_MakeNumber(num_FromInt(0)));
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
        g = ast_MakeBinary(OP_DIV, ast_MakeNumber(num_FromInt(1)), ast_Copy(ast_ChildGet(e, 0)));
        out = g != NULL ? far_pow(ast_ChildGet(e, 1), g, var, v, tries) : FAR_NONE;
        ast_Cleanup(g);
        return out;
    case OP_LOG:
        if(has_symbol(ast_ChildGet(e, 0), var->op.symbol))
            return FAR_NONE;
        f = far(ast_ChildGet(e, 1), var, &cv, tries);
        if(f == FAR_FIN && sign_of(cv) == 1)
            return fin(v, ast_MakeBinary(OP_LOG, ast_Copy(ast_ChildGet(e, 0)), cv));
        ast_Cleanup(cv);
        if(f != FAR_UP)
            return FAR_NONE;
        g = ast_MakeBinary(OP_ADD, ast_Copy(ast_ChildGet(e, 0)), ast_MakeNumber(num_FromInt(-1))); /*a base under 1 heads down*/
        out = g != NULL ? sign_of(g) : 2;
        ast_Cleanup(g);
        return out == 1 || out == -1 ? out : FAR_NONE;
    case OP_TAN_INV:
        f = far(ast_ChildGet(e, 0), var, &cv, tries);
        if(f == FAR_FIN)
            return fin(v, ast_MakeUnary(OP_TAN_INV, cv));
        if(f == FAR_NONE)
            return FAR_NONE;
        return fin(v, ast_MakeBinary(OP_MULT, ast_MakeNumber(num_FromInt(f)), ast_MakeBinary(OP_DIV, ast_MakeSymbol(SYM_PI), ast_MakeNumber(num_FromInt(2)))));
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
        return fin(v, g);
    }
}

/*lim at inf or -inf, new. an infinate one only for root*/
static pcas_ast_t *limit_far(pcas_ast_t *f, pcas_ast_t *var, int side, bool root) {
    pcas_ast_t *g = ast_Copy(f), *v = NULL;
    int out;

    if(g == NULL)
        return NULL;
    if(side < 0) { /*-inf is inf with x turned around*/
        pcas_ast_t *neg = ast_MakeBinary(OP_MULT, ast_MakeNumber(num_FromInt(-1)), ast_Copy(var));

        if(neg != NULL)
            substitute(g, var, neg);
        ast_Cleanup(neg);
    }
    simplify(g, SIMP_ALL);
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
    return out == FAR_UP ? v : ast_MakeBinary(OP_MULT, ast_MakeNumber(num_FromInt(-1)), v);
}

/*new, NULL when there isn't one or it gives up. s NULL for no steps, root when the limit is the whole input*/
static pcas_ast_t *limit_of(pcas_ast_t *f, pcas_ast_t *var, pcas_ast_t *a, steps_t *s, bool root) {
    pcas_ast_t *g, *top, *bottom, *t, *b, *out = NULL;
    step_t *l = NULL;
    bool cancelled;
    int tries;

    if(s != NULL && (t = lim(f, var, a)) != NULL) {
        simplify(ast_ChildGet(t, 0), SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_RATIONAL | SIMP_EVAL); /*x^2_1 parses as x^2+-1*1, it showed a lone minus*/
        l = steps_expr(s, t);
    }
    if(inf_sign(a) != 0) {
        out = limit_far(f, var, inf_sign(a), root);
        if(out != NULL)
            step(s, l, "plug in inf", ast_Copy(out));
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
        return NULL;
    }
    if(!ast_Compare(g, f))
        l = step(s, l, "one fraction", lim(g, var, a));
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
            l = step(s, l, "0/0, factor and cancel", lim(g, var, a));
            out = defined_at(g, var, a) ? plug_in(s, l, g, var, a, value_at(g, var, a)) : NULL;
            ast_Cleanup(g);
            break;
        }
        if(cancelled) {
            ast_Cleanup(top);
            ast_Cleanup(bottom);
            top = ast_Copy(ast_ChildGet(g, 0));
            bottom = ast_Copy(ast_ChildGet(g, 1));
            l = step(s, l, "0/0, factor and cancel", lim(g, var, a));
        } else {
            t = slope(top, var);
            b = slope(bottom, var);
            ast_Cleanup(top);
            ast_Cleanup(bottom);
            top = t;
            bottom = b;
            ast_Cleanup(g);
            g = ast_MakeBinary(OP_DIV, ast_Copy(top), ast_Copy(bottom));
            l = step(s, l, "0/0, l'Hopital", lim(g, var, a));
        }
        ast_Cleanup(g);
    }
    ast_Cleanup(top);
    ast_Cleanup(bottom);
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
    if(!isoptype(e, OP_LIMIT) || ast_ChildGet(e, 1)->type != NODE_SYMBOL)
        return changed;

    out = limit_of(ast_ChildGet(e, 0), ast_ChildGet(e, 1), ast_ChildGet(e, 2), NULL, root);
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
    if(!isoptype(l, OP_LIMIT) || ast_ChildGet(l, 1)->type != NODE_SYMBOL)
        return false;
    v = limit_of(ast_ChildGet(l, 0), ast_ChildGet(l, 1), ast_ChildGet(l, 2), out, true);
    if(v == NULL || pcas_cancelled) {
        ast_Cleanup(v);
        steps_free(out);
        return false;
    }
    ast_Cleanup(v);
    steps_shown(out);
    return true;
}
