#ifndef FLUXION_DRAW_H
#define FLUXION_DRAW_H

#include "layout.h"
#include "fonts.h"

/*
    the font ladder (see layout.h): smallest first, and which one is normal math.
    default: 5x8, 7x13, [9x15], 10x20. Fluxion draws text itself, so any font
    height works on the calculator too (graphx fonts are stuck at 8 pixels).
*/
void draw_set_ladder(const font_t *const *fonts, int count, int base);
const font_t *draw_font_for(unsigned char size);

/* for labels like timings, one size below normal math */
const font_t *draw_label_font(void);

/* measurements for layout_build(), matching the fonts above */
extern layout_font_t draw_font;

/* bold / italic, done at draw time so there's no second font. a bitmask, so you
   can OR them. nothing's styled by default yet */
typedef enum {
    STYLE_NORMAL = 0,
    STYLE_BOLD   = 1,
    STYLE_ITALIC = 2
} font_style_t;

/* text with its first row at top, in the current colour */
void draw_text(int x, int top, const char *text, const font_t *font);
void draw_text_styled(int x, int top, const char *text, const font_t *font, font_style_t style);
int draw_text_width(const char *text, const font_t *font);

/* draws b with its origin (left edge, baseline) at screen (x, y), in the current colour */
void draw_box(const box_t *b, int x, int y);

#endif
