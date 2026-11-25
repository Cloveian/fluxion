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
    int parts; /*integrations by parts so far*/
} subs_t;

#define PARTS_MAX 6 /*x^5 e^x needs 5*/

static pcas_ast_t *integral_of(pcas_ast_t *I, steps_t *s);

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

/*sqrt(c - k u^2), sqrt(c + k u^2), 1/sqrt(c + k u^2), u = ax+b. 1/sqrt(c - k u^2) is the asin in table_pow*/
static const char *sqrt_forms(pcas_ast_t *b, pcas_ast_t *n, pcas_ast_t *x, pcas_ast_t **F) {
    pcas_ast_t *u, *c, *k, *a, *sk, *root, *plus, *second, *G;
    int half = is_value(n, 1, 2) ? 1 : is_value(n, -1, 2) ? -1 : 0, sg;

    for(sg = 1; half != 0 && sg >= -1; sg -= 2) {
        if(half < 0 && sg < 0)
            continue;
        if((u = square_plus(b, x, sg, &c, &k)) == NULL)
            continue;
        if((a = linear(u, x)) != NULL) {
            sk = sqrt_of(ast_Copy(k));
            root = sqrt_of(ast_Copy(b));
            plus = ln_abs(ast_MakeBinary(OP_ADD, ast_MakeBinary(OP_MULT, ast_Copy(sk), ast_Copy(u)), ast_Copy(root)));
            if(half < 0) {
                G = ast_MakeBinary(OP_DIV, plus, ast_Copy(sk));
            } else {
                if(sg > 0) {
                    second = plus;
                } else {
                    second = ast_MakeUnary(OP_SIN_INV, ast_MakeBinary(OP_DIV, ast_MakeBinary(OP_MULT, ast_Copy(sk), ast_Copy(u)), sqrt_of(ast_Copy(c))));
                    ast_Cleanup(plus);
                }
                G = ast_MakeBinary(OP_ADD, ast_MakeBinary(OP_DIV, ast_MakeBinary(OP_MULT, ast_Copy(u), ast_Copy(root)), ast_MakeInt(2)),
                                   ast_MakeBinary(OP_MULT, ast_MakeBinary(OP_DIV, ast_Copy(c), ast_MakeBinary(OP_MULT, ast_MakeInt(2), ast_Copy(sk))), second));
            }
            ast_Cleanup(sk);
            ast_Cleanup(root);
            *F = over(G, a);
        }
        ast_Cleanup(c);
        ast_Cleanup(k);
        if(a != NULL)
            return "square root rule";
    }
    return NULL;
}

/*b^n off the table, F new*/
static const char *table_pow(pcas_ast_t *f, pcas_ast_t *x, pcas_ast_t **F) {
    pcas_ast_t *b = ast_ChildGet(f, 0), *n = ast_ChildGet(f, 1), *u, *a, *c, *k, *G;
    Symbol s = x->op.symbol;
    OperatorType op;
    int sign;

    if(!has_symbol(n, s) && (a = linear(b, x)) != NULL) {
        if(is_value(n, -1, 1)) {
            pcas_ast_t *inside = ast_MakeBinary(OP_DIV, ast_Copy(b), ast_Copy(a));

            simplify(inside, SIMP_ALL); /*ln|(2u+2)/2| is ln|u+1|, the ln(2) is only a constant*/
            *F = over(ln_abs(inside), a);
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
    return sqrt_forms(b, n, x, F);
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
    case OP_SEC: G = ln_abs(ast_MakeBinary(OP_ADD, ast_Copy(f), ast_MakeUnary(OP_TAN, ast_Copy(u)))); break;
    case OP_CSC: G = ln_abs(ast_MakeBinary(OP_ADD, ast_Copy(f), ast_MakeBinary(OP_MULT, ast_MakeInt(-1), ast_MakeUnary(OP_COT, ast_Copy(u))))); break;
    case OP_ABS: G = ast_MakeBinary(OP_DIV, ast_MakeBinary(OP_MULT, ast_Copy(u), ast_Copy(f)), ast_MakeInt(2)); name = "abs rule"; break;
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

/*which piece goes first as u in by parts (LIATE), 0 when it isn't one of those: 5 log, 4 inverse trig, 3 a power of x, 2 sin or cos, 1 a number to the x*/
static int liate(pcas_ast_t *p, pcas_ast_t *x) {
    Symbol s = x->op.symbol;

    if(ast_Compare(p, x))
        return 3;
    if(p->type != NODE_OPERATOR || !has_symbol(p, s))
        return 0;
    switch(optype(p)) {
    case OP_LOG: return 5;
    case OP_SIN_INV: case OP_COS_INV: case OP_TAN_INV: return 4;
    case OP_SIN: case OP_COS: return 2;
    case OP_POW:
        if(ast_Compare(ast_ChildGet(p, 0), x) && !has_symbol(ast_ChildGet(p, 1), s))
            return 3;
        if(isoptype(ast_ChildGet(p, 0), OP_LOG) && !has_symbol(ast_ChildGet(p, 1), s)) /*ln(x)^2 goes first like ln(x)*/
            return 5;
        return !has_symbol(ast_ChildGet(p, 0), s) ? 1 : 0;
    default:
        return 0;
    }
}

/*∫u dv = u v - ∫v du. u is the higest of LIATE in a product, or the whole of a log or inverse trig with dv = 1. v has to come out whole (no integral left in it), the new integral goes round the rules again*/
static const char *by_parts(pcas_ast_t *e, subs_t *subs) {
    static int depth; /*v is worked out by integral_of, which can want its own parts*/
    pcas_ast_t *f = ast_ChildGet(e, 0), *x = ast_ChildGet(e, 1), *u = NULL, *dv, *c, *v, *du, *I;
    int rank = 0, best = 0, tie = 0;

    if(subs->parts >= PARTS_MAX || depth > 1)
        return NULL;
    if(isoptype(f, OP_MULT)) {
        for(c = ast_ChildGet(f, 0); c != NULL; c = c->next) {
            rank = liate(c, x);
            if(rank > best) {
                best = rank;
                u = c;
                tie = 0;
            } else if(rank == best && rank > 0) {
                tie = 1;
            }
        }
        if(u == NULL || tie)
            return NULL;
        dv = ast_MakeOperator(OP_MULT);
        for(c = ast_ChildGet(f, 0); c != NULL && dv != NULL; c = c->next)
            if(c != u)
                ast_ChildAppend(dv, ast_Copy(c));
        if(dv != NULL && ast_ChildLength(dv) == 1) { /*a product of one is just it*/
            c = ast_ChildRemoveIndex(dv, 0);
            ast_Cleanup(dv);
            dv = c;
        }
    } else if(liate(f, x) >= 4) {
        u = f;
        dv = ast_MakeInt(1);
    } else {
        return NULL;
    }
    if(dv == NULL)
        return NULL;

    I = integral(dv, x);
    depth++;
    v = I != NULL ? integral_of(I, NULL) : NULL;
    depth--;
    ast_Cleanup(I);
    du = v != NULL ? slope_of(u, x) : NULL;
    if(v == NULL || du == NULL || pcas_cancelled) {
        ast_Cleanup(v);
        ast_Cleanup(du);
        return NULL;
    }
    subs->parts++;
    replace_node(e, ast_MakeBinary(OP_ADD,
        ast_MakeBinary(OP_MULT, ast_Copy(u), ast_Copy(v)),
        ast_MakeBinary(OP_MULT, ast_MakeInt(-1), integral(ast_MakeBinary(OP_MULT, v, du), x))));
    return "integration by parts";
}

/*e becomes the integrals of the terms of g multiplied out. g taken*/
static void split_terms(pcas_ast_t *e, pcas_ast_t *g, pcas_ast_t *x) {
    expand(g, EXP_ALL);
    simplify(g, SIMP_LIGHT);
    if(isoptype(g, OP_ADD)) { /*a term each, the like terms pass would factor 1/4+cos/2+cos^2/4 back into a square*/
        pcas_ast_t *sum = ast_MakeOperator(OP_ADD), *t;

        for(t = ast_ChildGet(g, 0); t != NULL && sum != NULL; t = t->next)
            ast_ChildAppend(sum, integral(ast_Copy(t), x));
        ast_Cleanup(g);
        g = sum;
        replace_node(e, g);
    } else {
        replace_node(e, integral(g, x));
    }
}

/*((1 + sign cos(2u))/2)^k, u copied*/
static pcas_ast_t *half_angle(pcas_ast_t *u, int sign, mp_small k) {
    pcas_ast_t *cos2u = ast_MakeUnary(OP_COS, ast_MakeBinary(OP_MULT, ast_MakeInt(2), ast_Copy(u)));

    return ast_MakeBinary(OP_POW, /*the sum as the base, expand only does the power of one*/
            ast_MakeBinary(OP_ADD, ast_MakeBinary(OP_DIV, ast_MakeInt(1), ast_MakeInt(2)),
                ast_MakeBinary(OP_MULT, ast_MakeBinary(OP_DIV, ast_MakeInt(sign), ast_MakeInt(2)), cos2u)),
            ast_MakeInt(k));
}

/*sin(u)^n and cos(u)^n, u linear, n 2 to 6*/
static const char *trig_power(pcas_ast_t *e) {
    pcas_ast_t *f = ast_ChildGet(e, 0), *x = ast_ChildGet(e, 1), *b, *u, *a, *g, *trig;
    mp_small n;
    bool sine;

    if(!isoptype(f, OP_POW) || !whole_number(ast_ChildGet(f, 1), &n) || n < 2 || n > 6)
        return NULL;
    b = ast_ChildGet(f, 0);
    if(!isoptype(b, OP_SIN) && !isoptype(b, OP_COS))
        return NULL;
    u = ast_ChildGet(b, 0);
    if((a = linear(u, x)) == NULL)
        return NULL;
    ast_Cleanup(a);
    sine = isoptype(b, OP_SIN);
    if(n % 2 == 0) {
        g = half_angle(u, sine ? -1 : 1, n / 2);
    } else {
        trig = ast_MakeUnary(sine ? OP_COS : OP_SIN, ast_Copy(u)); /*one out front, the rest (1-cos(u)^2)^((n-1)/2) for u-substitution*/
        g = ast_MakeBinary(OP_MULT, ast_Copy(b),
                ast_MakeBinary(OP_POW,
                    ast_MakeBinary(OP_ADD, ast_MakeInt(1),
                        ast_MakeBinary(OP_MULT, ast_MakeInt(-1), ast_MakeBinary(OP_POW, trig, ast_MakeInt(2)))),
                    ast_MakeInt((int)((n - 1) / 2))));
    }
    split_terms(e, g, x);
    return n % 2 == 0 ? "power reduction" : "trig identity";
}

/*e^E sin(T) in one go, E = ax+c and T = bx+d: e^E (a sin T - b cos T)/(a^2+b^2), and a cos T + b sin T on top for cos(T)*/
static const char *exp_trig(pcas_ast_t *e) {
    pcas_ast_t *f = ast_ChildGet(e, 0), *x = ast_ChildGet(e, 1), *ex = NULL, *tr = NULL, *c, *a, *b, *num, *den, *s, *co;
    bool sine;

    if(!isoptype(f, OP_MULT) || ast_ChildLength(f) != 2)
        return NULL;
    for(c = ast_ChildGet(f, 0); c != NULL; c = c->next) {
        if(isoptype(c, OP_POW) && ast_ChildGet(c, 0)->type == NODE_SYMBOL && ast_ChildGet(c, 0)->op.symbol == SYM_EULER)
            ex = c;
        else if(isoptype(c, OP_SIN) || isoptype(c, OP_COS))
            tr = c;
    }
    if(ex == NULL || tr == NULL)
        return NULL;
    a = linear(ast_ChildGet(ex, 1), x);
    b = linear(ast_ChildGet(tr, 0), x);
    if(a == NULL || b == NULL) {
        ast_Cleanup(a);
        ast_Cleanup(b);
        return NULL;
    }
    sine = isoptype(tr, OP_SIN);
    s = ast_MakeUnary(OP_SIN, ast_Copy(ast_ChildGet(tr, 0)));
    co = ast_MakeUnary(OP_COS, ast_Copy(ast_ChildGet(tr, 0)));
    num = sine ? ast_MakeBinary(OP_ADD, ast_MakeBinary(OP_MULT, ast_Copy(a), s),
                                ast_MakeBinary(OP_MULT, ast_MakeInt(-1), ast_MakeBinary(OP_MULT, ast_Copy(b), co)))
               : ast_MakeBinary(OP_ADD, ast_MakeBinary(OP_MULT, ast_Copy(a), co), ast_MakeBinary(OP_MULT, ast_Copy(b), s));
    den = ast_MakeBinary(OP_ADD, ast_MakeBinary(OP_POW, a, ast_MakeInt(2)), ast_MakeBinary(OP_POW, b, ast_MakeInt(2)));
    replace_node(e, ast_MakeBinary(OP_MULT, ast_Copy(ex), ast_MakeBinary(OP_DIV, num, den)));
    return "exponential times trig";
}

/*b^k, a number for 0 and b itself for 1. b copied*/
static pcas_ast_t *power_of(pcas_ast_t *b, long k) {
    if(k == 0)
        return ast_MakeInt(1);
    return k == 1 ? ast_Copy(b) : ast_MakeBinary(OP_POW, ast_Copy(b), ast_MakeInt((mp_small)k));
}

/*tan(u)^n and cot(u)^n from 2, sec(u)^n and csc(u)^n from 3, u linear. reduction formula, a power 2 lower left to integrate*/
static const char *trig_reduce(pcas_ast_t *e) {
    pcas_ast_t *f = ast_ChildGet(e, 0), *x = ast_ChildGet(e, 1), *b, *a, *first, *rest, *low;
    OperatorType op;
    mp_small n;
    bool plain;

    if(!isoptype(f, OP_POW) || !whole_number(ast_ChildGet(f, 1), &n) || n < 2 || n > 8 || ast_ChildGet(f, 0)->type != NODE_OPERATOR)
        return NULL;
    b = ast_ChildGet(f, 0);
    op = optype(b);
    plain = op == OP_TAN || op == OP_COT; /*the sec and csc ones carry a tan or cot*/
    if(!(plain || ((op == OP_SEC || op == OP_CSC) && n >= 3)) || (a = linear(ast_ChildGet(b, 0), x)) == NULL)
        return NULL;
    low = integral(power_of(b, n - 2), x);
    if(plain) { /*tan^(n-1)/(a(n-1)) - tan^(n-2)*/
        first = ast_MakeBinary(OP_DIV, power_of(b, n - 1), ast_MakeBinary(OP_MULT, a, ast_MakeInt((mp_small)(n - 1))));
        if(op == OP_COT)
            first = ast_MakeBinary(OP_MULT, ast_MakeInt(-1), first);
        rest = ast_MakeBinary(OP_MULT, ast_MakeInt(-1), low);
    } else { /*sec^(n-2) tan/(a(n-1)) + (n-2)/(n-1) sec^(n-2)*/
        first = ast_MakeBinary(OP_DIV,
                    ast_MakeBinary(OP_MULT, power_of(b, n - 2), ast_MakeUnary(op == OP_SEC ? OP_TAN : OP_COT, ast_Copy(ast_ChildGet(b, 0)))),
                    ast_MakeBinary(OP_MULT, a, ast_MakeInt((mp_small)(n - 1))));
        if(op == OP_CSC)
            first = ast_MakeBinary(OP_MULT, ast_MakeInt(-1), first);
        rest = ast_MakeBinary(OP_MULT, ast_MakeBinary(OP_DIV, ast_MakeInt((mp_small)(n - 2)), ast_MakeInt((mp_small)(n - 1))), low);
    }
    replace_node(e, ast_MakeBinary(OP_ADD, first, rest));
    return "reduction formula";
}

/*sin(u)^m cos(u)^n with both even: half angles, multiplied out*/
static const char *trig_product(pcas_ast_t *e) {
    pcas_ast_t *f = ast_ChildGet(e, 0), *x = ast_ChildGet(e, 1), *p, *q, *s = NULL, *c = NULL, *u, *a, *g;
    mp_small m, n;

    if(!isoptype(f, OP_MULT) || ast_ChildLength(f) != 2)
        return NULL;
    for(p = ast_ChildGet(f, 0); p != NULL; p = p->next) {
        if(!isoptype(p, OP_POW) || ast_ChildGet(p, 0)->type != NODE_OPERATOR)
            return NULL;
        q = ast_ChildGet(p, 0);
        if(isoptype(q, OP_SIN))
            s = p;
        else if(isoptype(q, OP_COS))
            c = p;
    }
    if(s == NULL || c == NULL)
        return NULL;
    u = ast_ChildGet(ast_ChildGet(s, 0), 0);
    if(!ast_Compare(u, ast_ChildGet(ast_ChildGet(c, 0), 0)) || !whole_number(ast_ChildGet(s, 1), &m) || !whole_number(ast_ChildGet(c, 1), &n)
       || m < 2 || n < 2 || m % 2 != 0 || n % 2 != 0 || m + n > 8 || (a = linear(u, x)) == NULL)
        return NULL;
    ast_Cleanup(a);
    g = ast_MakeBinary(OP_MULT, half_angle(u, -1, m / 2), half_angle(u, 1, n / 2));
    split_terms(e, g, x);
    return "power reduction";
}

/*P/Q with P as big as Q or bigger, long *dih*vision: the quotient plus the remainder over Q*/
static const char *long_division(pcas_ast_t *e) {
    pcas_ast_t *f = ast_ChildGet(e, 0), *x = ast_ChildGet(e, 1), *q, *r, *sum;
    int dp, dq;
    bool exact;

    if(!isoptype(f, OP_DIV))
        return NULL;
    dq = poly_degree(ast_ChildGet(f, 1), x->op.symbol, NULL);
    dp = poly_degree(ast_ChildGet(f, 0), x->op.symbol, NULL);
    if(dq < 1 || dp < dq || (q = poly_quotient(ast_ChildGet(f, 0), ast_ChildGet(f, 1), x->op.symbol, &exact)) == NULL)
        return NULL;
    if(exact) {
        replace_node(e, integral(q, x));
        return "polynomial division";
    }
    r = poly_remainder(ast_ChildGet(f, 0), ast_ChildGet(f, 1), x->op.symbol);
    if(r == NULL || (sum = ast_MakeOperator(OP_ADD)) == NULL) {
        ast_Cleanup(q);
        ast_Cleanup(r);
        return NULL;
    }
    ast_ChildAppend(sum, integral(q, x));
    ast_ChildAppend(sum, integral(ast_MakeBinary(OP_DIV, r, ast_Copy(ast_ChildGet(f, 1))), x));
    replace_node(e, sum);
    return "polynomial division";
}

/*P/Q with Q of degree 2 to 4 over *dih*stinct rational roots and P of a lower degree: A1/(x-r1) + A2/(x-r2) + ..., Ai = P(ri)/Q'(ri). new, NULL when it isn't one of those*/
pcas_ast_t *partial_sum(pcas_ast_t *top, pcas_ast_t *bottom, Symbol var) {
    pcas_ast_t *roots[SOLVE_MAX] = { NULL }, *lead = NULL, *sum, *t, *num, *den, *x = ast_MakeSymbol(var);
    int dp, dq, i, j, n;
    bool ok = true;

    dq = poly_degree(bottom, var, &lead);
    dp = poly_degree(top, var, NULL); /*-1 for sin(x) or e^x up there*/
    if(x == NULL || dq < 2 || dq > 4 || dp < 0 || dp >= dq) {
        ast_Cleanup(lead);
        ast_Cleanup(x);
        return NULL;
    }
    n = solve_ex(bottom, var, roots, false);
    for(i = 0; i < n && ok; i++) {
        fold_fraction(roots[i]);
        ok = roots[i]->type == NODE_NUMBER;
    }
    if(n != dq || !ok || (sum = ast_MakeOperator(OP_ADD)) == NULL) {
        for(i = 0; i < n; i++)
            ast_Cleanup(roots[i]);
        ast_Cleanup(lead);
        ast_Cleanup(x);
        return NULL;
    }
    for(i = 0; i < n; i++) {
        num = ast_MakeBinary(OP_MULT, ast_MakeInt(1), ast_Copy(top)); /*substitute works on what's under the node*/
        substitute(num, x, roots[i]);
        den = ast_Copy(lead);
        for(j = 0; j < n; j++)
            if(j != i)
                den = ast_MakeBinary(OP_MULT, den,
                        ast_MakeBinary(OP_ADD, ast_Copy(roots[i]), ast_MakeBinary(OP_MULT, ast_MakeInt(-1), ast_Copy(roots[j]))));
        t = ast_MakeBinary(OP_DIV, num, den);
        simplify(t, SIMP_ALL);
        ast_ChildAppend(sum, ast_MakeBinary(OP_MULT, t,
                ast_MakeBinary(OP_DIV, ast_MakeInt(1),
                    ast_MakeBinary(OP_ADD, ast_Copy(x), ast_MakeBinary(OP_MULT, ast_MakeInt(-1), ast_Copy(roots[i]))))));
    }
    for(i = 0; i < n; i++)
        ast_Cleanup(roots[i]);
    ast_Cleanup(lead);
    ast_Cleanup(x);
    return sum;
}

static const char *partial_fractions(pcas_ast_t *e) {
    pcas_ast_t *f = ast_ChildGet(e, 0), *x = ast_ChildGet(e, 1), *sum;

    if(!isoptype(f, OP_DIV) || (sum = partial_sum(ast_ChildGet(f, 0), ast_ChildGet(f, 1), x->op.symbol)) == NULL)
        return NULL;
    replace_node(e, integral(sum, x));
    return "partial fractions";
}

/*a coefficient poly_coefs left (NULL is 0), as its own copy*/
static pcas_ast_t *coef_copy(pcas_ast_t *c) {
    return c != NULL ? ast_Copy(c) : ast_MakeInt(0);
}

/*(p x + q)/(a x^2 + b x + c) with roots that aren't rational (partial fractions took those): p/(2a) ln|Q| + (q - p b/(2a)) times 1/Q. 1/Q is an atan when b^2-4ac is under 0, a ln of the roots over 0 and -2/(2ax+b) at 0*/
static const char *quadratic_bottom(pcas_ast_t *e) {
    pcas_ast_t *f = ast_ChildGet(e, 0), *x = ast_ChildGet(e, 1), *Q = ast_ChildGet(f, 1), *cq[3], *ct[3] = {NULL};
    pcas_ast_t *A, *B, *C, *p, *q, *twoA, *lin, *D, *r = NULL, *inv = NULL, *res;
    double dv;
    int i, dv_top;

    if(!isoptype(f, OP_DIV) || poly_degree(Q, x->op.symbol, NULL) != 2
       || (dv_top = poly_degree(ast_ChildGet(f, 0), x->op.symbol, NULL)) < 0 || dv_top > 1)
        return NULL;
    poly_coefs(Q, x->op.symbol, cq);
    poly_coefs(ast_ChildGet(f, 0), x->op.symbol, ct);
    A = coef_copy(cq[2]);
    B = coef_copy(cq[1]);
    C = coef_copy(cq[0]);
    p = coef_copy(ct[1]);
    q = coef_copy(ct[0]);
    for(i = 0; i < 3; i++) {
        ast_Cleanup(cq[i]);
        ast_Cleanup(ct[i]);
    }
    twoA = ast_MakeBinary(OP_MULT, ast_MakeInt(2), ast_Copy(A));
    lin = ast_MakeBinary(OP_ADD, ast_MakeBinary(OP_MULT, ast_Copy(twoA), ast_Copy(x)), ast_Copy(B));
    D = ast_MakeBinary(OP_ADD, ast_MakeBinary(OP_POW, ast_Copy(B), ast_MakeInt(2)),
                       ast_MakeBinary(OP_MULT, ast_MakeInt(-4), ast_MakeBinary(OP_MULT, ast_Copy(A), ast_Copy(C))));
    simplify(D, SIMP_ALL);
    res = NULL;
    if(approx_double(D, &dv)) {
        if(dv < 0) {
            r = sqrt_of(ast_MakeBinary(OP_MULT, ast_MakeInt(-1), ast_Copy(D)));
            inv = ast_MakeBinary(OP_MULT, ast_MakeBinary(OP_DIV, ast_MakeInt(2), ast_Copy(r)),
                                 ast_MakeUnary(OP_TAN_INV, ast_MakeBinary(OP_DIV, ast_Copy(lin), ast_Copy(r))));
        } else if(dv > 0) {
            r = sqrt_of(ast_Copy(D));
            inv = ast_MakeBinary(OP_MULT, ast_MakeBinary(OP_DIV, ast_MakeInt(1), ast_Copy(r)),
                                 ln_abs(ast_MakeBinary(OP_DIV, ast_MakeBinary(OP_ADD, ast_Copy(lin), ast_MakeBinary(OP_MULT, ast_MakeInt(-1), ast_Copy(r))),
                                                       ast_MakeBinary(OP_ADD, ast_Copy(lin), ast_Copy(r)))));
        } else {
            inv = ast_MakeBinary(OP_DIV, ast_MakeInt(-2), ast_Copy(lin));
        }
        res = ast_MakeBinary(OP_ADD,
                ast_MakeBinary(OP_MULT, ast_MakeBinary(OP_DIV, ast_Copy(p), ast_Copy(twoA)), ln_abs(ast_Copy(Q))),
                ast_MakeBinary(OP_MULT,
                    ast_MakeBinary(OP_ADD, ast_Copy(q), ast_MakeBinary(OP_MULT, ast_MakeInt(-1),
                        ast_MakeBinary(OP_DIV, ast_MakeBinary(OP_MULT, ast_Copy(p), ast_Copy(B)), ast_Copy(twoA)))),
                    inv));
        replace_node(e, res);
    }
    ast_Cleanup(A);
    ast_Cleanup(B);
    ast_Cleanup(C);
    ast_Cleanup(p);
    ast_Cleanup(q);
    ast_Cleanup(twoA);
    ast_Cleanup(lin);
    ast_Cleanup(D);
    ast_Cleanup(r);
    return res != NULL ? "complete the square" : NULL;
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
    if(isoptype(f, OP_DIV) && isoptype(ast_ChildGet(f, 1), OP_MULT) && (c = constant_part(ast_ChildGet(f, 1), x, &rest)) != NULL) { /*1/(2e^(2x))*/
        replace_node(e, ast_MakeBinary(OP_DIV, integral(ast_MakeBinary(OP_DIV, ast_Copy(ast_ChildGet(f, 0)), rest), x), c));
        return "constant multiple";
    }
    if((name = table(f, x, &n)) != NULL) {
        replace_node(e, n);
        return name;
    }
    if((name = trig_power(e)) != NULL)
        return name;
    if((name = trig_reduce(e)) != NULL)
        return name;
    if((name = trig_product(e)) != NULL)
        return name;
    if((name = usub(e, subs)) != NULL)
        return name;
    if((name = exp_trig(e)) != NULL)
        return name;
    if((name = long_division(e)) != NULL)
        return name;
    if((name = partial_fractions(e)) != NULL)
        return name;
    if((name = quadratic_bottom(e)) != NULL)
        return name;
    if((name = by_parts(e, subs)) != NULL)
        return name;
    if(isoptype(f, OP_DIV) && liate(ast_ChildGet(f, 1), x) == 1 && isoptype(ast_ChildGet(f, 1), OP_POW)) { /*X/e^(2X) is X e^(-2X), simplify wrote it the first way*/
        pcas_ast_t *bottom = ast_ChildGet(f, 1), *as = ast_MakeBinary(OP_MULT, ast_Copy(ast_ChildGet(f, 0)),
            ast_MakeBinary(OP_POW, ast_Copy(ast_ChildGet(bottom, 0)), ast_MakeBinary(OP_MULT, ast_MakeInt(-1), ast_Copy(ast_ChildGet(bottom, 1)))));

        simplify(as, SIMP_NORMALIZE | SIMP_COMMUTATIVE); /*not eval, that writes the -2X back as a bottom*/
        n = integral(as, x);
        if(n != NULL && (name = by_parts(n, subs)) != NULL) {
            replace_node(e, n);
            return name;
        }
        ast_Cleanup(n);
    }
    return NULL;
}

/*F for the integral node I, new. NULL when some piece has no rule. the lines go in s when it isn't NULL*/
static pcas_ast_t *integral_of(pcas_ast_t *I, steps_t *s) {
    pcas_ast_t *cur, *found[STEPS_FOUND_MAX], *x = ast_ChildGet(I, 1), *t;
    const char *name;
    step_t *l = NULL;
    subs_t subs;
    int n = 0, i, had, rounds = 0;

    subs.n = 0;
    subs.parts = 0;
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
