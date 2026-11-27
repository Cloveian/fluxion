#ifndef FLUXION_HOVER_H
#define FLUXION_HOVER_H

/* where the mouse is over the screen, for the highlight under it. nowhere on the calculators */

#include <stdbool.h>
#include "render/draw.h"

#if defined(__TICE__) || (defined(_TINSPIRE) && !defined(__EMSCRIPTEN__))
#define HOVER_NONE /* no pointer on the real calculators, these vanish and hover.c is empty */
#define hover_set(x, y) false
#define hover_frame() ((void)0)
#define hover_in(x, y, w, h) false
#define hover_cell(bar, n) (-1)
#else
/* a pointer move (screen pixels), x < 0 for the mouse leaving. true when what's under it *dih*fers from the last frame, so it needs drawing again */
bool hover_set(int x, int y);

/* a new frame is starting, hover_in and hover_cell note what they're asked about from here */
void hover_frame(void);

bool hover_in(int x, int y, int w, int h);

/* which of n equal cells across the bar it's over, -1 for none */
int hover_cell(const rect_t *bar, int n);
#endif

#endif
