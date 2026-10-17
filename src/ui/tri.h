#ifndef FLUXION_TRI_H
#define FLUXION_TRI_H

#include <stdbool.h>
#include "render/layout.h"

/* triangle( amplitude( pand( por(. works like geom_run, true if cmd was one
   of these and the answer (or a message) is pushed already. src gets cut up
   at the commas, in is for the history */
bool tri_run(const char *cmd, char *src, box_t *in);

#endif
