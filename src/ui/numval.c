#include "numval.h"

#include <math.h>

static float int_value(mp_int z) {
    float v = 0;
    mp_size i;

    for(i = MP_USED(z); i > 0; i--)
        v = ldexpf(v, (int)MP_DIGIT_BIT) + (float)MP_DIGITS(z)[i - 1];
    return MP_SIGN(z) == MP_NEG ? -v : v;
}

/* what's left after approx: numbers, and simplify likes 5.01 as 501/100 in a
   DIV node, so the + * / of them too */
bool num_float(const pcas_ast_t *e, float *x) {
    const pcas_ast_t *k;
    float y;

    if(e->type == NODE_NUMBER) {
        *x = int_value(mp_rat_numer_ref(e->op.num)) / int_value(mp_rat_denom_ref(e->op.num));
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
