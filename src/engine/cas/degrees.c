#include "cas.h"

/*Fluxion: degree mode. the engine only knows radians, so before anything runs
  every trig argument gets a pi/180 and every inverse trig result a 180/pi. all
  exact, sin(30) becomes sin(pi/6) and the identities take it from there. the
  cost is sin(x) coming back as sin(pi x/180) when x stays a variable (todo,
  undo that for showing, the nspire hides it)*/

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
