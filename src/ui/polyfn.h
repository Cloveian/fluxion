#ifndef FLUXION_POLYFN_H
#define FLUXION_POLYFN_H

#include <stdbool.h>
#include "render/layout.h"

/* fromrts( and inverse(. same as geom_run: true if cmd was one of these and the
   answer (or a message) is already pushed. src gets cut up at the commas */
bool polyfn_run(const char *cmd, char *src, box_t *in);

#endif
