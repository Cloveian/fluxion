#ifndef FLUXION_CALC1_H
#define FLUXION_CALC1_H

#include <stdbool.h>
#include "render/layout.h"

/* tangent( critical( extrema( inflect(, calc 1 on top of the derivative and solve. true if cmd
   was one of these, and then the answer (or a message) is already pushed. src is cut up at the commas */
bool calc1_run(const char *cmd, char *src, box_t *in);

#endif
