#ifndef FLUXION_SERIES_H
#define FLUXION_SERIES_H

#include <stdbool.h>
#include "render/layout.h"

/* sum( and product(, true if cmd was one of them and the answer (or a message) is already pushed. src gets cut up at the commas */
bool series_run(const char *cmd, char *src, box_t *in);

#endif
