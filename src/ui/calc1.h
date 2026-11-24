#ifndef FLUXION_CALC1_H
#define FLUXION_CALC1_H

#include <stdbool.h>
#include "render/layout.h"

/* tangent( critical( extrema( inflect( increasing(, calc 1 on top of the derivative and solve. true if cmd
   was one of these, and then the answer (or a message) is already pushed. src is cut up at the commas */
bool calc1_run(const char *cmd, char *src, box_t *in);

/* the steps page for tangent(, critical(, extremma( or inflect(, src is what's inside. NULL when it opened, or what to say */
const char *calc1_work(const char *cmd, char *src);

#endif
