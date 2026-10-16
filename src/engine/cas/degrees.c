#include "cas.h"

/*Fluxion: degree mode. the engine only knows radians, so before anything runs
  every trig argument gets a pi/180 and every inverse trig result a 180/pi. all
  exact, sin(30) becomes sin(pi/6) and the identities take it from there. when
  x stays a variable the pi/180 comes back out at the end (degrees_out below)*/

static bool trig(const pcas_ast_t *e) {
    if(e->type != NODE_OPERATOR)
        return false;
    switch(optype(e)) {
    case OP_SIN: case OP_COS: case OP_TAN:
    case OP_COT: case OP_SEC: case OP_CSC:
        return true;
    default:
        return false;
    }
}

static bool inverse(const pcas_ast_t *e) {
    return e->type == NODE_OPERATOR
           && (optype(e) == OP_SIN_INV || optype(e) == OP_COS_INV || optype(e) == OP_TAN_INV);
}

/*pi/180, or 180/pi the other way*/
static pcas_ast_t *ratio(bool to_radians) {
    pcas_ast_t *pi = ast_MakeSymbol(SYM_PI), *n = ast_MakeNumber(num_FromInt(180));

    return to_radians ? ast_MakeBinary(OP_DIV, pi, n) : ast_MakeBinary(OP_DIV, n, pi);
}

pcas_ast_t *degrees_in(pcas_ast_t *e) {
    unsigned i;

    if(e == NULL || e->type != NODE_OPERATOR || pcas_cancelled)
        return e;

    for(i = 0; i < ast_ChildLength(e); i++) {
        pcas_ast_t *c = ast_ChildGet(e, i), *fixed = degrees_in(c);

        if(fixed != c) { /*it got wrapped, so swap the wrapper in*/
            ast_ChildRemoveIndex(e, i);
            ast_ChildInsert(e, fixed, i);
        }
    }

    if(trig(e)) {
        pcas_ast_t *arg = ast_ChildRemoveIndex(e, 0);

        ast_ChildAppend(e, ast_MakeBinary(OP_MULT, arg, ratio(true)));
        return e;
    }
    if(inverse(e))
        return ast_MakeBinary(OP_MULT, e, ratio(false));
    return e;
}

/*and back out for showing. after simplify a trig argument that stayed symbolic
  is (stuff pi)/d, or pi/(d stuff) when the x was under. pull the pi and scale
  by 180/d so sin(x) shows as sin(x) again and sin(2x) doesnt come back as
  sin(x pi/90). a sum goes term by term and if one term isnt that shape the
  whole argument stays, it really has a pi in it then. the inverse ones
  (180 asin(x)/pi) still show the long way, todo*/

static bool is_pi(const pcas_ast_t *e) {
    return e->type == NODE_SYMBOL && e->op.symbol == SYM_PI;
}

static bool has_pi(const pcas_ast_t *n) {
    unsigned i;

    if(is_pi(n))
        return true;
    if(!isoptype(n, OP_MULT))
        return false;
    for(i = 0; i < ast_ChildLength((pcas_ast_t*)n); i++)
        if(is_pi(ast_ChildGet((pcas_ast_t*)n, i)))
            return true;
    return false;
}

/*the number in the denominator, or NULL when the term isnt our shape*/
static pcas_ast_t *degree_den(pcas_ast_t *t) {
    pcas_ast_t *den;

    if(!isoptype(t, OP_DIV) || !has_pi(ast_ChildGet(t, 0)))
        return NULL;
    den = ast_ChildGet(t, 1);
    if(den->type == NODE_NUMBER)
        return den;
    if(isoptype(den, OP_MULT) && ast_ChildGet(den, 0)->type == NODE_NUMBER)
        return ast_ChildGet(den, 0); /*simplify keeps the number first*/
    return NULL;
}

/*takes the pi out of n, has_pi said its there*/
static pcas_ast_t *strip_pi(pcas_ast_t *n) {
    unsigned i;

    if(is_pi(n)) {
        ast_Cleanup(n);
        return ast_MakeNumber(num_FromInt(1));
    }
    for(i = 0; i < ast_ChildLength(n); i++)
        if(is_pi(ast_ChildGet(n, i)))
            break;
    ast_Cleanup(ast_ChildRemoveIndex(n, i));
    if(ast_ChildLength(n) == 1) { /*no one-child products lying around*/
        pcas_ast_t *only = ast_ChildRemoveIndex(n, 0);
        ast_Cleanup(n);
        return only;
    }
    return n;
}

/*one term. degree_den already said it fits*/
static pcas_ast_t *undo(pcas_ast_t *t) {
    pcas_ast_t *d = degree_den(t), *num, *den, *k;

    k = ast_MakeNumber(num_FromInt(180));
    mp_rat_div(k->op.num, d->op.num, k->op.num);
    num = strip_pi(ast_ChildRemoveIndex(t, 0));
    den = ast_ChildRemoveIndex(t, 0);
    ast_Cleanup(t);
    if(den == d) {
        ast_Cleanup(den);
        return ast_MakeBinary(OP_MULT, k, num);
    }
    ast_Cleanup(ast_ChildRemoveIndex(den, 0)); /*the number, out from under*/
    if(ast_ChildLength(den) == 1) {
        pcas_ast_t *only = ast_ChildRemoveIndex(den, 0);
        ast_Cleanup(den);
        den = only;
    }
    return ast_MakeBinary(OP_DIV, ast_MakeBinary(OP_MULT, k, num), den);
}

void degrees_out(pcas_ast_t *e) {
    unsigned i;
    pcas_ast_t *arg;

    if(e == NULL || e->type != NODE_OPERATOR || pcas_cancelled)
        return;
    for(i = 0; i < ast_ChildLength(e); i++)
        degrees_out(ast_ChildGet(e, i));
    if(!trig(e))
        return;

    arg = ast_ChildGet(e, 0);
    if(isoptype(arg, OP_ADD)) {
        for(i = 0; i < ast_ChildLength(arg); i++)
            if(degree_den(ast_ChildGet(arg, i)) == NULL)
                return;
        for(i = 0; i < ast_ChildLength(arg); i++)
            ast_ChildInsert(arg, undo(ast_ChildRemoveIndex(arg, i)), i);
    } else if(degree_den(arg) != NULL) {
        ast_ChildRemoveIndex(e, 0);
        arg = undo(arg);
        ast_ChildAppend(e, arg);
    } else
        return;
    /*the 180/d times things is raw, this tidies 1*x and (180/7)*x. no identity
      flags so it cant put the pi back*/
    simplify(arg, SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_RATIONAL | SIMP_EVAL);
}
