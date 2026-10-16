#ifndef FLUXION_GEOM_H
#define FLUXION_GEOM_H

#include <stdbool.h>
#include "render/layout.h"

/* slope( dist( mid( par( perp(, the coordinate geometry commands. true if cmd
   was one of these, and then the answer (or a message) is already pushed. src
   is the argument the editor made, it gets cut up at the commas. takes in, for
   the history */
bool geom_run(const char *cmd, char *src, box_t *in);

#endif
