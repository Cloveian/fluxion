/*Fluxion: limits, worked out the way you would on paper. not in upstream PineappleCAS.*/

#include "cas.h"

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

/*new, NULL when there isn't one or it gives up*/
static pcas_ast_t *limit_of(pcas_ast_t *f, pcas_ast_t *var, pcas_ast_t *a) {
    pcas_ast_t *g = ast_Copy(f), *top, *bottom, *t, *b, *out = NULL;
    bool cancelled;
    int tries;

    comdenom(g, true); /*one fraction, top and bottom factored*/
    if(!isoptype(g, OP_DIV)) {
        out = value_at(g, var, a);
        ast_Cleanup(g);
        return out;
    }
    top = ast_Copy(ast_ChildGet(g, 0));
    bottom = ast_Copy(ast_ChildGet(g, 1));
    ast_Cleanup(g);

    for(tries = 0; tries < TRIES_MAX && !pcas_cancelled; tries++) {
        t = value_at(top, var, a);
        b = value_at(bottom, var, a);

        if(!is_ast_int(b, 0)) { /*plugs straight in*/
            out = ast_MakeBinary(OP_DIV, t, b);
            simplify(out, SIMP_ALL);
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
            out = value_at(g, var, a);
            ast_Cleanup(g);
            break;
        }
        if(cancelled) {
            ast_Cleanup(top);
            ast_Cleanup(bottom);
            top = ast_Copy(ast_ChildGet(g, 0));
            bottom = ast_Copy(ast_ChildGet(g, 1));
        } else {
            t = slope(top, var);
            b = slope(bottom, var);
            ast_Cleanup(top);
            ast_Cleanup(bottom);
            top = t;
            bottom = b;
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

    out = limit_of(ast_ChildGet(e, 0), ast_ChildGet(e, 1), ast_ChildGet(e, 2));
    if(out == NULL)
        return changed;
    replace_node(e, out);
    return true;
}
