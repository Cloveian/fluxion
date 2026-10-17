#ifndef FLUXION_RANGE_H
#define FLUXION_RANGE_H

#include <stdbool.h>
#include "render/layout.h"

/* range(f, x), the y values f takes. false for any other cmd, otherwise the
   answer or a message is on the history with in. src gets cut at its comma */
bool range_run(const char *cmd, char *src, box_t *in);

#endif
