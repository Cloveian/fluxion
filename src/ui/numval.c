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

/* f at a decimal x, NAN when it isn't a plain number there. the engine has no
   floats so x goes in as a fraction over 65536, close enough for the test points
   ineq and range poke f at */
float num_at(pcas_ast_t *f, Symbol var, float x) {
    pcas_ast_t *e = ast_Copy(f), *sym = ast_MakeSymbol(var), *v;
    float y = NAN;

    if(fabsf(x) < 30000)
        v = ast_MakeNumber(num_FromFraction((mp_small)lroundf(x * 65536), 65536));
    else
        v = ast_MakeNumber(num_FromInt((mp_small)lroundf(x)));
    substitute(e, sym, v);
    approx(e);
    simplify(e, SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_RATIONAL | SIMP_EVAL);
    if(!num_float(e, &y) || !isfinite(y))
        y = NAN;

    ast_Cleanup(e);
    ast_Cleanup(sym);
    ast_Cleanup(v);
    return y;
}
