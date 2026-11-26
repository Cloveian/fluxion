#ifndef FLUXION_GEOM_H
#define FLUXION_GEOM_H

#include <stdbool.h>
#include "engine/ast.h"
#include "render/layout.h"

/* a - b, there's no subtract node, it's a plus the negative. takes both */
pcas_ast_t *minus(pcas_ast_t *a, pcas_ast_t *b);

/* slope( dist( mid( par( perp(, the coordinate geometry commands. true if cmd
   was one of these, and then the answer (or a message) is already pushed. src
   is the argument the editor made, it gets cut up at the commas. takes in, for
   the history */
bool geom_run(const char *cmd, char *src, box_t *in);

#endif
