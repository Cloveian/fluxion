#ifndef FLUXION_SHAPES_H
#define FLUXION_SHAPES_H

#include <stdbool.h>
#include "render/layout.h"

/* circle( rect( square( trap( sphere( cyl( cone( cube( box( polygon(, every
   formula for the shape at once. false means cmd wasn't a shape and app.c
   tries the next one, true means it's pushed (answer, usage line or a
   message). chops src at the commas, in goes on the history with it */
bool shapes_run(const char *cmd, char *src, box_t *in);

#endif
