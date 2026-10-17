#ifndef FLUXION_INEQ_H
#define FLUXION_INEQ_H

#include "render/layout.h"
#include "engine/ast.h"

/* solve(f < g, x) and friends, run_solve hands over f - g and the sign. rel is
   < > or CH_LE / CH_GE from the editor. the answer (or a message) gets pushed
   with in. f stays the caller's */
void ineq_run(pcas_ast_t *f, Symbol var, char shown, char rel, box_t *in);

/* one piece of an interval answer: lo < x < hi, x <= hi, x > lo, or x = c when
   lo and hi are the same tree. NULL for no end on that side, the bools say if the
   end is in. range( uses it too, with y. the trees are copied, not kept */
box_t *ineq_row(char shown, pcas_ast_t *lo, bool lo_in, pcas_ast_t *hi, bool hi_in);

#endif
