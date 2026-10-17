#include "numval.h"
#include "engine/cas/cas.h"

#include <math.h>

/* what's left after approx: numbers, and simplify likes 5.01 as 501/100 in a
   DIV node, so the + * / of them too */
bool num_float(const pcas_ast_t *e, float *x) {
    const pcas_ast_t *k;
    float y;

    if(e->type == NODE_NUMBER) {
        *x = approx_int(mp_rat_numer_ref(e->op.num)) / approx_int(mp_rat_denom_ref(e->op.num));
        return true;
    }
    if(!isoptype(e, OP_ADD) && !isoptype(e, OP_MULT) && !isoptype(e, OP_DIV))
        return false;
    for(k = e->op.operator.base; k != NULL; k = k->next) {
        if(!num_float(k, &y))
            return false;
        if(k == e->op.operator.base)
            *x = y;
        else
            *x = isoptype(e, OP_ADD) ? *x + y : isoptype(e, OP_MULT) ? *x * y : *x / y;
    }
    return true;
}
