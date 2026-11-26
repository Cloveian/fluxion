#ifndef FLUXION_AREAS_H
#define FLUXION_AREAS_H

#include <stdbool.h>
#include "render/draw.h"

/* the bands down the screen, where everything in ui/ gets its edges */
typedef struct {
    rect_t top; /* modifiers, form, battery */
    rect_t body; /* history and the input line, or a page */
    rect_t keys; /* soft-key names */
} areas_t;

extern areas_t areas;

#define MARGIN 4 /* from the screen's edge to the bars and the history */

/* the nspire has no keys under its screen: no folder bar, [menu] drops a list of the five down from the top like its own */
#ifdef _TINSPIRE
#define NSPIRE 1
#else
#define NSPIRE 0
#endif

/* where a page's content ends, above the key row or past it when the page has no keys */
int areas_page_bottom(bool keys);

/* where the history's band ends, the nspire's main screen gets the bar's rows too */
int areas_body_bottom(void);

/* after the fonts, the top bar is as tall as the label font */
void areas_init(void);

#endif
