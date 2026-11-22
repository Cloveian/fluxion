/*Fluxion: integrals, the way you'd do them on paper. not in upstream PineappleCAS.*/

#include "cas.h"
#include "steps.h"

#include <math.h>

#define SUBS_MAX 4 /*u-substitutions in one integral*/
#define CANDS_MAX 8 /*pieces tried as u, each one a d/dx and a full simplify*/
#define ROUNDS_MAX 12 /*lines of rules before it gives up*/

/*each u and the x thing it stands for, put back at the end*/
typedef struct {
    pcas_ast_t *u[SUBS_MAX], *g[SUBS_MAX];
    pcas_ast_t *all; /*the whole integral, U+2x cos(x^2) has a U outside f*/
    int n;
} subs_t;

/*u is a*x + b: a, new. NULL for anything else (or a = 0)*/
static pcas_ast_t *linear(pcas_ast_t *u, pcas_ast_t *x) {
    pcas_ast_t *a = slope_of(u, x);

    if(a != NULL && (has_symbol(a, x->op.symbol) || is_ast_int(a, 0))) {
        ast_Cleanup(a);
        return NULL;
    }
    return a;
}

/*F/a, both taken. a = 1 is just F*/
static pcas_ast_t *over(pcas_ast_t *F, pcas_ast_t *a) {
    if(a != NULL && is_ast_int(a, 1)) {
        ast_Cleanup(a);
        return F;
    }
    return ast_MakeBinary(OP_DIV, F, a);
}

/*e is the number p/q, as one number or a/b of two (simplify leaves -1/2 like that)*/
static bool is_value(pcas_ast_t *e, long p, long q) {
    mp_rat r;
    bool is;

    if(e->type == NODE_NUMBER)
        return mp_rat_compare_value(e->op.num, p, q) == 0;
    if(!isoptype(e, OP_DIV) || ast_ChildGet(e, 0)->type != NODE_NUMBER || ast_ChildGet(e, 1)->type != NODE_NUMBER
       || is_ast_int(ast_ChildGet(e, 1), 0) || (r = num_Copy(ast_ChildGet(e, 0)->op.num)) == NULL)
        return false;
    is = mp_rat_div(r, ast_ChildGet(e, 1)->op.num, r) == MP_OK && mp_rat_compare_value(r, p, q) == 0;
    num_Cleanup(r);
    return is;
}

/*ln|u|, u taken*/
static pcas_ast_t *ln_abs(pcas_ast_t *u) {
    return ast_MakeBinary(OP_LOG, ast_MakeSymbol(SYM_EULER), ast_MakeUnary(OP_ABS, u));
}

/*c + k u^2 (sign 1) or c - k u^2 (sign -1), c and k plain numbers over 0: u, with c and k new. else NULL*/
static pcas_ast_t *square_plus(pcas_ast_t *e, pcas_ast_t *x, int sign, pcas_ast_t **c, pcas_ast_t **k) {
    pcas_ast_t *a, *b, *sq;
    double cv, kv;

    if(!isoptype(e, OP_ADD) || ast_ChildLength(e) != 2)
        return NULL;
    a = ast_ChildGet(e, 0);
    b = ast_ChildGet(e, 1);
    sq = has_symbol(a, x->op.symbol) ? a : b;
    a = sq == a ? b : a; /*the constant*/
    if(has_symbol(a, x->op.symbol) || !approx_double(a, &cv) || cv <= 0)
        return NULL;
    if(isoptype(sq, OP_MULT) && ast_ChildLength(sq) == 2 && !has_symbol(ast_ChildGet(sq, 0), x->op.symbol)) { /*9x^2*/
        *k = ast_Copy(ast_ChildGet(sq, 0));
        sq = ast_ChildGet(sq, 1);
    } else {
        *k = ast_MakeInt(1);
    }
    if(sign < 0)
        *k = ast_MakeBinary(OP_MULT, ast_MakeInt(-1), *k);
    if(*k == NULL || !approx_double(*k, &kv) || kv <= 0 || !isoptype(sq, OP_POW) || !is_value(ast_ChildGet(sq, 1), 2, 1)) {
        ast_Cleanup(*k);
        return NULL;
    }
    *c = ast_Copy(a);
    return ast_ChildGet(sq, 0);
}

/*b^n off the table, F new*/
static const char *table_pow(pcas_ast_t *f, pcas_ast_t *x, pcas_ast_t **F) {
    pcas_ast_t *b = ast_ChildGet(f, 0), *n = ast_ChildGet(f, 1), *u, *a, *c, *k, *G;
    Symbol s = x->op.symbol;
    OperatorType op;
    int sign;

    if(!has_symbol(n, s) && (a = linear(b, x)) != NULL) {
        if(is_value(n, -1, 1)) {
            *F = over(ln_abs(ast_Copy(b)), a);
            return "log rule";
        }
        *F = over(ast_MakeBinary(OP_DIV, ast_MakeBinary(OP_POW, ast_Copy(b), ast_MakeBinary(OP_ADD, ast_Copy(n), ast_MakeInt(1))),
            ast_MakeBinary(OP_ADD, ast_Copy(n), ast_MakeInt(1))), a);
        return "power rule";
    }
    if(!has_symbol(b, s) && (a = linear(n, x)) != NULL) {
        if(b->type == NODE_SYMBOL && b->op.symbol == SYM_EULER)
            *F = over(ast_Copy(f), a);
        else
            *F = over(ast_MakeBinary(OP_DIV, ast_Copy(f), ast_MakeBinary(OP_LOG, ast_MakeSymbol(SYM_EULER), ast_Copy(b))), a);
        return "exp rule";
    }

    /*sec^2 as 1/cos^2 and the rest*/
    if(b->type == NODE_OPERATOR && ast_ChildLength(b) == 1 && (a = linear(u = ast_ChildGet(b, 0), x)) != NULL) {
        op = optype(b);
        if((op == OP_COS && is_value(n, -2, 1)) || (op == OP_SEC && is_value(n, 2, 1))) {
            *F = over(ast_MakeUnary(OP_TAN, ast_Copy(u)), a);
            return "trig rule";
        }
        if((op == OP_SIN && is_value(n, -2, 1)) || (op == OP_CSC && is_value(n, 2, 1))) {
            *F = over(ast_MakeBinary(OP_MULT, ast_MakeInt(-1), ast_MakeUnary(OP_COT, ast_Copy(u))), a);
            return "trig rule";
        }
        ast_Cleanup(a);
    }
    /*1/(c + k u^2) is atan(sqrt(k/c) u)/sqrt(ck), (c - k u^2)^(-1/2) is asin(sqrt(k/c) u)/sqrt(k). each over a*/
    sign = is_value(n, -1, 1) ? 1 : is_value(n, -1, 2) ? -1 : 0;
    if(sign != 0 && (u = square_plus(b, x, sign, &c, &k)) != NULL) {
        if((a = linear(u, x)) != NULL) {
            pcas_ast_t *r = sqrt_of(ast_MakeBinary(OP_DIV, ast_Copy(k), ast_Copy(c)));
            pcas_ast_t *bottom = sqrt_of(sign > 0 ? ast_MakeBinary(OP_MULT, ast_Copy(c), ast_Copy(k)) : ast_Copy(k));

            G = ast_MakeUnary(sign > 0 ? OP_TAN_INV : OP_SIN_INV, ast_MakeBinary(OP_MULT, r, ast_Copy(u)));
            *F = over(ast_MakeBinary(OP_DIV, G, bottom), a);
        }
        ast_Cleanup(c);
        ast_Cleanup(k);
        if(a != NULL)
            return "inverse trig rule";
    }
    return NULL;
}

/*sin(u), ln(u), ... with u = ax+b*/
static const char *table_fn(pcas_ast_t *f, pcas_ast_t *x, pcas_ast_t **F) {
    pcas_ast_t *u = ast_ChildGet(f, ast_ChildLength(f) - 1), *a, *G = NULL;
    const char *name = "trig rule";

    if((a = linear(u, x)) == NULL)
        return NULL;
    switch(optype(f)) {
    case OP_SIN:  G = ast_MakeBinary(OP_MULT, ast_MakeInt(-1), ast_MakeUnary(OP_COS, ast_Copy(u))); break;
    case OP_COS:  G = ast_MakeUnary(OP_SIN, ast_Copy(u)); break;
    case OP_TAN:  G = ast_MakeBinary(OP_MULT, ast_MakeInt(-1), ln_abs(ast_MakeUnary(OP_COS, ast_Copy(u)))); break;
    case OP_COT:  G = ln_abs(ast_MakeUnary(OP_SIN, ast_Copy(u))); break;
    case OP_SINH: G = ast_MakeUnary(OP_COSH, ast_Copy(u)); name = "hyperbolic rule"; break;
    case OP_COSH: G = ast_MakeUnary(OP_SINH, ast_Copy(u)); name = "hyperbolic rule"; break;
    case OP_LOG: /*u ln u - u, over ln(b) for log base b*/
        if(has_symbol(ast_ChildGet(f, 0), x->op.symbol))
            break;
        G = ast_MakeBinary(OP_ADD, ast_MakeBinary(OP_MULT, ast_Copy(u), ast_MakeBinary(OP_LOG, ast_MakeSymbol(SYM_EULER), ast_Copy(u))),
            ast_MakeBinary(OP_MULT, ast_MakeInt(-1), ast_Copy(u)));
        if(!(ast_ChildGet(f, 0)->type == NODE_SYMBOL && ast_ChildGet(f, 0)->op.symbol == SYM_EULER))
            G = ast_MakeBinary(OP_DIV, G, ast_MakeBinary(OP_LOG, ast_MakeSymbol(SYM_EULER), ast_Copy(ast_ChildGet(f, 0))));
        name = "log rule";
        break;
    default:
        break;
    }
    if(G == NULL) {
        ast_Cleanup(a);
        return NULL;
    }
    *F = over(G, a);
    return name;
}

/*sec(u)tan(u) and csc(u)cot(u)*/
static const char *table_pair(pcas_ast_t *f, pcas_ast_t *x, pcas_ast_t **F) {
    pcas_ast_t *p = ast_ChildGet(f, 0), *q, *a;
    OperatorType op;

    if(ast_ChildLength(f) != 2 || p->type != NODE_OPERATOR)
        return NULL;
    q = p->next;
    if(optype(p) == OP_TAN || optype(p) == OP_COT) { /*either order*/
        q = p;
        p = p->next;
    }
    if(p->type != NODE_OPERATOR || q->type != NODE_OPERATOR || !ast_Compare(ast_ChildGet(p, 0), ast_ChildGet(q, 0)))
        return NULL;
    op = optype(p);
    if(!((op == OP_SEC && optype(q) == OP_TAN) || (op == OP_CSC && optype(q) == OP_COT)) || (a = linear(ast_ChildGet(p, 0), x)) == NULL)
        return NULL;
    *F = ast_MakeUnary(op, ast_Copy(ast_ChildGet(p, 0)));
    if(op == OP_CSC)
        *F = ast_MakeBinary(OP_MULT, ast_MakeInt(-1), *F);
    *F = over(*F, a);
    return "trig rule";
}

/*straight off the table, f(ax+b) for any of them. the rule's name, NULL when it isn't on there*/
static const char *table(pcas_ast_t *f, pcas_ast_t *x, pcas_ast_t **F) {
    pcas_ast_t *g;
    const char *name;

    *F = NULL;
    if(ast_Compare(f, x)) {
        *F = ast_MakeBinary(OP_DIV, ast_MakeBinary(OP_POW, ast_Copy(x), ast_MakeInt(2)), ast_MakeInt(2));
        return "power rule";
    }
    if(f->type != NODE_OPERATOR)
        return NULL;
    switch(optype(f)) {
    case OP_POW:
        return table_pow(f, x, F);
    case OP_MULT:
        return table_pair(f, x, F);
    case OP_DIV: /*1/g and sqrt(g) as powers*/
    case OP_ROOT:
        if(isoptype(f, OP_DIV) && !is_ast_int(ast_ChildGet(f, 0), 1))
            return NULL;
        g = ast_ChildGet(f, 1);
        if(isoptype(f, OP_ROOT))
            g = ast_MakeBinary(OP_POW, ast_Copy(g), ast_MakeBinary(OP_DIV, ast_MakeInt(1), ast_Copy(ast_ChildGet(f, 0))));
        else if(isoptype(g, OP_POW)) /*1/b^k is b^-k*/
            g = ast_MakeBinary(OP_POW, ast_Copy(ast_ChildGet(g, 0)), ast_MakeBinary(OP_MULT, ast_MakeInt(-1), ast_Copy(ast_ChildGet(g, 1))));
        else
            g = ast_MakeBinary(OP_POW, ast_Copy(g), ast_MakeInt(-1));
        simplify(ast_ChildGet(g, 1), SIMP_NORMALIZE | SIMP_RATIONAL | SIMP_EVAL);
        name = !pcas_cancelled && isoptype(g, OP_POW) ? table_pow(g, x, F) : NULL; /*an oom leaves g a child short*/
        ast_Cleanup(g);
        return name;
    default:
        return table_fn(f, x, F);
    }
}

/*∫f dx, f taken*/
static pcas_ast_t *integral(pcas_ast_t *f, pcas_ast_t *x) {
    pcas_ast_t *i = ast_MakeOperator(OP_INTEGRAL);

    if(i == NULL) {
        ast_Cleanup(f);
        return NULL;
    }
    ast_ChildAppend(i, f);
    ast_ChildAppend(i, ast_Copy(x));
    return i;
}

/*the factors of a product with no x, pulled out front. NULL when there's none, or nothing but*/
static pcas_ast_t *constant_part(pcas_ast_t *f, pcas_ast_t *x, pcas_ast_t **rest) {
    pcas_ast_t *c = ast_MakeOperator(OP_MULT), *r = ast_MakeOperator(OP_MULT), *k;

    for(k = ast_ChildGet(f, 0); k != NULL && c != NULL && r != NULL; k = k->next)
        ast_ChildAppend(has_symbol(k, x->op.symbol) ? r : c, ast_Copy(k));
    if(c == NULL || r == NULL || ast_ChildLength(c) == 0 || ast_ChildLength(r) == 0) {
        ast_Cleanup(c);
        ast_Cleanup(r);
        return NULL;
    }
    *rest = r;
    return c;
}

/*the pieces of f with x in them that u could be, outermsot first. not f itself or a bare x*/
static int candidates(pcas_ast_t *f, pcas_ast_t *e, pcas_ast_t *x, pcas_ast_t **out, int n) {
    pcas_ast_t *c;

    if(e->type != NODE_OPERATOR || n >= CANDS_MAX || !has_symbol(e, x->op.symbol))
        return n;
    if(e != f && !isoptype(e, OP_MULT))
        out[n++] = e;
    for(c = ast_ChildGet(e, 0); c != NULL; c = c->next)
        n = candidates(f, c, x, out, n);
    return n;
}

/*a letter for u that isn't in the integral or used already. U first like on paper*/
static pcas_ast_t *fresh(subs_t *subs) {
    static const char pick[] = "UWVTSR";
    int i, k;

    for(i = 0; pick[i] != '\0'; i++) {
        for(k = 0; k < subs->n && subs->u[k]->op.symbol != (Symbol)pick[i]; k++)
            ;
        if(k == subs->n && !has_symbol(subs->all, (Symbol)pick[i]))
            return ast_MakeSymbol(pick[i]);
    }
    return NULL;
}

/*u = some piece g where f/g' is all g, 2x cos(x^2) is cos(u) with u = x^2. ∫f dx becomes ∫(f/g') du*/
static const char *usub(pcas_ast_t *e, subs_t *subs) {
    pcas_ast_t *f = ast_ChildGet(e, 0), *x = ast_ChildGet(e, 1), *cand[CANDS_MAX], *g, *q, *u;
    int n, i;

    if(subs->n >= SUBS_MAX || (u = fresh(subs)) == NULL)
        return NULL;
    n = candidates(f, f, x, cand, 0);
    for(i = 0; i < n && !pcas_cancelled; i++) {
        q = ast_MakeBinary(OP_DIV, ast_Copy(f), slope_of(cand[i], x));
        g = ast_Copy(cand[i]);
        simplify(q, SIMP_ALL);
        simplify(g, SIMP_ALL); /*how it looks in q now, x^2+1 or 1+x^2*/
        if(q != NULL && g != NULL) {
            substitute(q, cand[i], u);
            substitute(q, g, u);
        }
        ast_Cleanup(g);
        if(q != NULL && !pcas_cancelled && !has_symbol(q, x->op.symbol)) {
            subs->u[subs->n] = u; /*not a copy, an oom one is NULL and fresh() reads it*/
            subs->g[subs->n++] = ast_Copy(cand[i]);
            replace_node(e, integral(q, u));
            return "u-substitution";
        }
        ast_Cleanup(q);
    }
    ast_Cleanup(u);
    return NULL;
}

/*one rule on the integral node e, in place. its name, NULL when nothing fits*/
static const char *one_rule(pcas_ast_t *e, subs_t *subs) {
    pcas_ast_t *f = ast_ChildGet(e, 0), *x = ast_ChildGet(e, 1), *n, *c, *rest;
    const char *name;

    if(!has_symbol(f, x->op.symbol)) { /*3 is 3x*/
        replace_node(e, ast_MakeBinary(OP_MULT, ast_Copy(f), ast_Copy(x)));
        return "constant rule";
    }
    if(isoptype(f, OP_ADD)) {
        n = ast_MakeOperator(OP_ADD);
        for(c = ast_ChildGet(f, 0); c != NULL && n != NULL; c = c->next)
            ast_ChildAppend(n, integral(ast_Copy(c), x));
        replace_node(e, n);
        return "sum rule";
    }
    if(isoptype(f, OP_MULT) && (c = constant_part(f, x, &rest)) != NULL) {
        replace_node(e, ast_MakeBinary(OP_MULT, c, integral(rest, x)));
        return "constant multiple";
    }
    if(isoptype(f, OP_DIV) && !has_symbol(ast_ChildGet(f, 1), x->op.symbol)) { /*x^2/2*/
        replace_node(e, ast_MakeBinary(OP_DIV, integral(ast_Copy(ast_ChildGet(f, 0)), x), ast_Copy(ast_ChildGet(f, 1))));
        return "constant multiple";
    }
    if(isoptype(f, OP_DIV) && !has_symbol(ast_ChildGet(f, 0), x->op.symbol) && !is_ast_int(ast_ChildGet(f, 0), 1)) { /*3/x*/
        replace_node(e, ast_MakeBinary(OP_MULT, ast_Copy(ast_ChildGet(f, 0)),
            integral(ast_MakeBinary(OP_DIV, ast_MakeInt(1), ast_Copy(ast_ChildGet(f, 1))), x)));
        return "constant multiple";
    }
    if((name = table(f, x, &n)) != NULL) {
        replace_node(e, n);
        return name;
    }
    return usub(e, subs);
}

/*F for the integral node I, new. NULL when some piece has no rule. the lines go in s when it isn't NULL*/
static pcas_ast_t *integral_of(pcas_ast_t *I, steps_t *s) {
    pcas_ast_t *cur, *found[STEPS_FOUND_MAX], *x = ast_ChildGet(I, 1), *t;
    const char *name;
    step_t *l = NULL;
    subs_t subs;
    int n = 0, i, had, rounds = 0;

    subs.n = 0;
    if((cur = integral(ast_Copy(ast_ChildGet(I, 0)), x)) == NULL)
        return NULL;
    subs.all = cur;
    simplify(ast_ChildGet(cur, 0), SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_EVAL); /*sqrt(x) is x^(1/2) for the rules*/
    if(s != NULL && (t = ast_Copy(I)) != NULL) {
        simplify(ast_ChildGet(t, 0), SIMP_LIGHT);
        l = steps_expr(s, t);
    }

    while(!pcas_cancelled && (n = steps_find(cur, OP_INTEGRAL, found, 0)) > 0) {
        if(++rounds > ROUNDS_MAX)
            break;
        had = subs.n;
        for(i = 0; i < n && (name = one_rule(found[i], &subs)) != NULL; i++)
            steps_rule(l, name);
        if(i < n)
            break;
        simplify(cur, SIMP_LIGHT | SIMP_LIKE_TERMS);
        for(i = had; i < subs.n && s != NULL; i++) /*u = x^2 on its own line*/
            steps_eq(s, ast_Copy(subs.u[i]), ast_Copy(subs.g[i]));
        if(s != NULL)
            l = steps_expr(s, ast_Copy(cur));
    }
    if(pcas_cancelled || steps_find(cur, OP_INTEGRAL, found, 0) > 0) /*a piece with no rule*/
        n = -1;
    for(i = subs.n - 1; i >= 0 && n >= 0; i--) /*the u's go back the other way round, a later u can have earlier one in it*/
        substitute(cur, subs.u[i], subs.g[i]);
    for(i = 0; i < subs.n; i++) {
        ast_Cleanup(subs.u[i]);
        ast_Cleanup(subs.g[i]);
    }
    if(n < 0) {
        ast_Cleanup(cur);
        return NULL;
    }
    if(subs.n > 0) {
        steps_rule(l, "back to x");
        if(s != NULL)
            steps_expr(s, ast_Copy(cur));
    }
    return cur;
}

/*e with x = pt, as a float*/
static bool float_at(pcas_ast_t *e, pcas_ast_t *x, pcas_ast_t *pt, double *v) {
    pcas_ast_t *c = ast_Copy(e);
    bool ok = c != NULL && substitute(c, x, pt) && approx_float(c, v);

    ast_Cleanup(c);
    return ok;
}

/*for what domain can't pin down: tan sec cot csc of ax+b by where ax+b goes, a root of a quadratic or less by its lowest point (an end or the vertex)*/
static bool fine_between(pcas_ast_t *e, pcas_ast_t *x, pcas_ast_t *pa, pcas_ast_t *pb) {
    pcas_ast_t *c, *u, *k;
    double ua, ub, lo, hi, off, n, q[3], A, B, C, m, v;
    int i;

    if(e->type != NODE_OPERATOR || !has_symbol(e, x->op.symbol))
        return true;
    for(c = ast_ChildGet(e, 0); c != NULL; c = c->next)
        if(!fine_between(c, x, pa, pb))
            return false;

    switch(optype(e)) {
    case OP_TAN: case OP_SEC: case OP_COT: case OP_CSC:
        u = ast_ChildGet(e, 0);
        if((k = linear(u, x)) == NULL)
            return false;
        ast_Cleanup(k);
        if(!float_at(u, x, pa, &ua) || !float_at(u, x, pb, &ub))
            return false;
        lo = ua < ub ? ua : ub;
        hi = ua < ub ? ub : ua;
        off = optype(e) == OP_TAN || optype(e) == OP_SEC ? 1.5707963267949 : 0;
        n = ceil((lo - off) / 3.14159265358979 - 1e-9);
        return off + n * 3.14159265358979 > hi + 1e-9;
    case OP_POW: case OP_ROOT:
        u = isoptype(e, OP_POW) ? ast_ChildGet(e, 0) : ast_ChildGet(e, 1);
        c = isoptype(e, OP_POW) ? ast_ChildGet(e, 1) : NULL;
        if(c != NULL && c->type == NODE_NUMBER && mp_rat_is_integer(c->op.num))
            return true;    /*a whole power, real anywhere*/
        if(!has_symbol(u, x->op.symbol)) /*a^x, real only for a over 0*/
            return float_at(u, x, pa, &v) && v > 0;
        /*A x^2 + B x + C off its values at -1 0 1, and 2 to make sure that's all it is*/
        for(i = 0; i < 3; i++) {
            k = ast_MakeInt(i - 1);
            if(k == NULL || !float_at(u, x, k, &q[i])) {
                ast_Cleanup(k);
                return false;
            }
            ast_Cleanup(k);
        }
        C = q[1];
        A = (q[0] + q[2]) / 2 - C;
        B = (q[2] - q[0]) / 2;
        k = ast_MakeInt(2);
        if(k == NULL || !float_at(u, x, k, &v) || fabs(v - (4 * A + 2 * B + C)) > 1e-6 * (1 + fabs(v))) {
            ast_Cleanup(k);
            return false;
        }
        ast_Cleanup(k);
        if(!float_at(u, x, pa, &ua) || !float_at(u, x, pb, &ub))
            return false;
        m = ua < ub ? ua : ub;
        if(A > 0) { /*the vertex, if it's inside*/
            v = -B / (2 * A);
            if(!float_at(x, x, pa, &lo) || !float_at(x, x, pb, &hi))
                return false;
            if(v > (lo < hi ? lo : hi) && v < (lo < hi ? hi : lo)) {
                if(A * v * v + B * v + C <= 1e-9)
                    return false;
            }
        }
        return m >= -1e-9;
    case OP_DIV: case OP_ADD: case OP_MULT: case OP_SIN: case OP_COS: case OP_TAN_INV: case OP_ABS:
        return true;
    default:
        return false;
    }
}

/*f defined the whole way from a to b, so F(b) - F(a) is the area. 1/x^2 from -1 to 1 would come out -2.
false for a hole or a bound in between, or anything domain can't pin down that fine_between can't either*/
static bool defined_between(pcas_ast_t *I) {
    domain_t d;
    double a, b, t, h;
    bool ok;
    int i;

    if(!approx_double(ast_ChildGet(I, 2), &a) || !approx_double(ast_ChildGet(I, 3), &b)
       || !domain(ast_ChildGet(I, 0), ast_ChildGet(I, 1)->op.symbol, &d))
        return false;
    if(a > b) {
        t = a;
        a = b;
        b = t;
    }
    ok = !d.empty;
    if(ok && d.partial) { /*and f there at both ends, 1/sqrt(9-x^2) at 3 isn't*/
        pcas_ast_t *f = ast_ChildGet(I, 0), *x = ast_ChildGet(I, 1);

        ok = fine_between(f, x, ast_ChildGet(I, 2), ast_ChildGet(I, 3))
             && float_at(f, x, ast_ChildGet(I, 2), &h) && float_at(f, x, ast_ChildGet(I, 3), &h);
    }
    if(ok && d.lower != NULL)
        ok = approx_double(d.lower, &h) && (d.lower_open ? a > h : a >= h);
    if(ok && d.upper != NULL)
        ok = approx_double(d.upper, &h) && (d.upper_open ? b < h : b <= h);
    for(i = 0; i < d.nholes && ok; i++) {
        if(has_symbol(d.holes[i], SYM_N)) /*a family, tan's pi/2 + pi n*/
            ok = fine_between(ast_ChildGet(I, 0), ast_ChildGet(I, 1), ast_ChildGet(I, 2), ast_ChildGet(I, 3));
        else
            ok = approx_double(d.holes[i], &h) && (h < a || h > b);
    }
    domain_free(&d);
    return ok;
}

/*F(b) - F(a), F taken. the pluged in line, then what it comes to*/
static pcas_ast_t *between(pcas_ast_t *I, pcas_ast_t *F, steps_t *s) {
    pcas_ast_t *x = ast_ChildGet(I, 1), *top = ast_Copy(F), *d;
    step_t *l;

    if(top != NULL)
        substitute(top, x, ast_ChildGet(I, 3));
    substitute(F, x, ast_ChildGet(I, 2));
    d = ast_MakeBinary(OP_ADD, top, ast_MakeBinary(OP_MULT, ast_MakeInt(-1), F));
    if(d == NULL)
        return NULL;
    if(s != NULL && s->n > 0) {
        steps_rule(&s->line[s->n - 1], "F(b) - F(a)");
        if((l = steps_expr(s, ast_Copy(d))) != NULL)
            l->as_is = true; /*F(b) first like on paper, sorted it puts the minus first*/
    }
    simplify(d, SIMP_ALL);
    if(s != NULL)
        steps_expr(s, ast_Copy(d));
    return d;
}

bool eval_integral_nodes(pcas_ast_t *e) {
    pcas_ast_t *c, *F;
    bool changed = false;

    if(pcas_cancelled || e->type != NODE_OPERATOR)
        return false;
    for(c = ast_ChildGet(e, 0); c != NULL; c = c->next)
        changed |= eval_integral_nodes(c);
    if(!isoptype(e, OP_INTEGRAL) || ast_ChildGet(e, 1)->type != NODE_SYMBOL
       || (ast_ChildLength(e) != 2 && ast_ChildLength(e) != 4) || (ast_ChildLength(e) == 4 && !defined_between(e))
       || (F = integral_of(e, NULL)) == NULL)
        return changed;
    if(ast_ChildLength(e) == 4 && (F = between(e, F, NULL)) == NULL)
        return changed;
    simplify(F, SIMP_ALL);
    replace_node(e, F);
    return true;
}

bool steps_integral(pcas_ast_t *I, steps_t *out) {
    pcas_ast_t *F, *done;
    int k = ast_ChildLength(I);
    step_t *l;

    out->n = 0;
    if(!isoptype(I, OP_INTEGRAL) || ast_ChildGet(I, 1)->type != NODE_SYMBOL || (k != 2 && k != 4)
       || (k == 4 && !defined_between(I)) || (F = integral_of(I, out)) == NULL) {
        steps_free(out);
        return false;
    }
    if(k == 4) {
        F = between(I, F, out);
    } else if((done = ast_Copy(F)) != NULL) { /*tidied if that changes it, then + C*/
        simplify(done, SIMP_ALL);
        if(!ast_Compare(done, F))
            steps_expr(out, ast_Copy(done));
        if(out->n > 0)
            steps_rule(&out->line[out->n - 1], "+ C");
        simplify_canonical_form(done, CANONICAL_ALL); /*what the rest of the lines get from steps_shown*/
        if((l = steps_expr(out, ast_MakeBinary(OP_ADD, done, ast_MakeSymbol(SYM_C)))) != NULL)
            l->as_is = true; /*the canonical form puts C first*/
    }
    if(F == NULL || pcas_cancelled) {
        ast_Cleanup(F);
        steps_free(out);
        return false;
    }
    ast_Cleanup(F);
    steps_shown(out);
    return true;
}
