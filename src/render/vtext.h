#ifndef FLUXION_VTEXT_H
#define FLUXION_VTEXT_H

#include <stdbool.h>
#include "draw.h"

/* text from the embedded DejaVu Sans faces, the desktop's. a font is an em in layout pixels (drawn at ppu real
   pixels to each) and the ascent and descent the layout is given. draw.c calls these for the fonts that are ours */
const font_t *vtext_font(float em, int ascent, int descent);
bool vtext_owns(const font_t *font);
int vtext_width(const char *text, const font_t *font, font_style_t style);
void vtext_draw(int x, int top, const char *text, const font_t *font, font_style_t style);

#endif
