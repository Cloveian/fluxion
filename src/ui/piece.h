#ifndef FLUXION_PIECE_H
#define FLUXION_PIECE_H

#include <stdbool.h>
#include "render/layout.h"

/* piecewise(. false for any other cmd, otherwise it's handled and the answer
   or a message goes on the history with in. src gets chopped up at the commas
   and the < > signs */
bool piece_run(const char *cmd, char *src, box_t *in);

#endif
