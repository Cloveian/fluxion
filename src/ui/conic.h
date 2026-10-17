#ifndef FLUXION_CONIC_H
#define FLUXION_CONIC_H

#include <stdbool.h>
#include "render/layout.h"

/* ellipse( hyperbola(, both ways round. true if cmd was one of these, and then
   the answer (or a message) is already pushed, like geom_run. src is the
   argument the editor made and it gets cut up, in is for the history */
bool conic_run(const char *cmd, char *src, box_t *in);

#endif
