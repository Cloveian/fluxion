#ifndef FLUXION_NUMVAL_H
#define FLUXION_NUMVAL_H

#include <stdbool.h>
#include "engine/ast.h"

/* the float of a tree that approx() already turned into plain numbers
   (+ * / of numbers). false if anything else is left in it */
bool num_float(const pcas_ast_t *e, float *x);

/* f with var = x, as a float. NAN if that isn't a plain number */
float num_at(pcas_ast_t *f, Symbol var, float x);

#endif
