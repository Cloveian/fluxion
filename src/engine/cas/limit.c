/*Fluxion: limits, worked out the way you would on paper. not in upstream PineappleCAS.*/

#include "cas.h"
#include "steps.h"

#define TRIES_MAX 6 /*cancels and l'hopitals before it gives up, (1-cos(x))/x^2 takes 2*/

/*e with var = a and simplified, new*/
static pcas_ast_t *value_at(pcas_ast_t *e, pcas_ast_t *var, pcas_ast_t *a) {
    pcas_ast_t *v = ast_Copy(e);

    substitute(v, var, a);
    simplify(v, SIMP_ALL);
    return v;
}

/*d/dx of e, new and simplified*/
static pcas_ast_t *slope(pcas_ast_t *e, pcas_ast_t *var) {
    pcas_ast_t *d = ast_Copy(e);

    derivative(d, var, var);
    simplify(d, SIMP_ALL);
    return d;
}

static bool is_int_at(pcas_ast_t *e, pcas_ast_t *var, pcas_ast_t *a, int n) {
    pcas_ast_t *v = value_at(e, var, a);
    bool is = is_ast_int(v, n);

    ast_Cleanup(v);
    return is;
}

/*a value at var = a on paper: no 0 below, no ln(0) or log base 0 / 1, no 0^0 or 0^-1, no tan(pi/2). on the tree as it is, a
simplify after eats it, 0*e^(1/0) came out 0*/
static bool defined_at(pcas_ast_t *e, pcas_ast_t *var, pcas_ast_t *a) {
    pcas_ast_t *c, *v, *base, *trig;
    bool ok = true;

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
        ok = v->type == NODE_NUMBER && mp_rat_compare_zero(v->op.num) > 0;
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
    substitute(raw, var, a);
    l = step(s, l, "plug in", raw);
    step(s, l, NULL, ast_Copy(v));
    return v;
}

/*new, NULL when there isn't one or it gives up. s NULL for no steps*/
static pcas_ast_t *limit_of(pcas_ast_t *f, pcas_ast_t *var, pcas_ast_t *a, steps_t *s) {
    pcas_ast_t *g = ast_Copy(f), *top, *bottom, *t, *b, *out = NULL;
    step_t *l = NULL;
    bool cancelled;
    int tries;

    if(s != NULL) {
        t = lim(f, var, a);
        simplify(ast_ChildGet(t, 0), SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_RATIONAL | SIMP_EVAL); /*x^2_1 parses as x^2+-1*1, it showed a lone minus*/
        l = steps_expr(s, t);
    }
    if(defined_at(f, var, a)) {
        ast_Cleanup(g);
        return plug_in(s, l, f, var, a, value_at(f, var, a));
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

bool eval_limit_nodes(pcas_ast_t *e) {
    pcas_ast_t *child, *out;
    bool changed = false;

    if(pcas_cancelled || e->type != NODE_OPERATOR)
        return false;
    for(child = ast_ChildGet(e, 0); child != NULL; child = child->next)
        changed |= eval_limit_nodes(child);
    if(!isoptype(e, OP_LIMIT) || ast_ChildGet(e, 1)->type != NODE_SYMBOL)
        return changed;

    out = limit_of(ast_ChildGet(e, 0), ast_ChildGet(e, 1), ast_ChildGet(e, 2), NULL);
    if(out == NULL)
        return changed;
    replace_node(e, out);
    return true;
}

bool steps_limit(pcas_ast_t *l, steps_t *out) {
    pcas_ast_t *v;

    out->n = 0;
    if(!isoptype(l, OP_LIMIT) || ast_ChildGet(l, 1)->type != NODE_SYMBOL)
        return false;
    v = limit_of(ast_ChildGet(l, 0), ast_ChildGet(l, 1), ast_ChildGet(l, 2), out);
    if(v == NULL || pcas_cancelled) {
        ast_Cleanup(v);
        steps_free(out);
        return false;
    }
    ast_Cleanup(v);
    steps_shown(out);
    return true;
}
