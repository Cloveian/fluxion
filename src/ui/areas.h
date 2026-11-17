#ifndef FLUXION_AREAS_H
#define FLUXION_AREAS_H

#include "render/draw.h"

/* the bands down the screen, where everything in ui/ gets its edges */
typedef struct {
    rect_t top; /* modifiers, form, battery */
    rect_t body; /* history and the input line, or a page */
    rect_t keys; /* soft-key names */
} areas_t;

extern areas_t areas;

/* after the fonts, the top bar is as tall as the label font */
void areas_init(void);

#endif
