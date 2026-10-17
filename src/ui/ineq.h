#ifndef FLUXION_INEQ_H
#define FLUXION_INEQ_H

#include "render/layout.h"
#include "engine/ast.h"

/* solve(f < g, x) and friends, run_solve hands over f - g and the sign. rel is
   < > or CH_LE / CH_GE from the editor. the answer (or a message) gets pushed
   with in. f stays the caller's */
void ineq_run(pcas_ast_t *f, Symbol var, char shown, char rel, box_t *in);

#endif
