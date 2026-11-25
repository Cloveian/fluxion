#ifndef FLUXION_HOVER_H
#define FLUXION_HOVER_H

/* where the mouse is over the screen, for the highlight under it. nowhere on the calculators */

#include <stdbool.h>
#include "render/draw.h"

/* a pointer move (screen pixels), x < 0 for the mouse leaving */
void hover_set(int x, int y);

bool hover_in(int x, int y, int w, int h);

/* which of n equal cells across the bar it's over, -1 for none */
int hover_cell(const rect_t *bar, int n);

#endif
