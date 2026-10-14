#ifdef __TICE__

/* calculator side of screen.h. graphx, drawing to the back buffer and
   swapping on screen_show(). */

#include "screen.h"

#include <graphx.h>

void screen_begin(void) {
    gfx_Begin();
    gfx_SetDrawBuffer();

    /* same colours as screen_pc.c */
    gfx_palette[COL_BG]     = gfx_RGBTo1555(24, 24, 32);
    gfx_palette[COL_FG]     = gfx_RGBTo1555(235, 235, 240);
    gfx_palette[COL_DIM]    = gfx_RGBTo1555(120, 120, 140);
    gfx_palette[COL_ACCENT] = gfx_RGBTo1555(110, 170, 255);
}

void screen_end(void) {
    gfx_End();
}

void screen_clear(void) {
    gfx_FillScreen(COL_BG);
}

void screen_color(unsigned char color) {
    gfx_SetColor(color);
}

void screen_fill(int x, int y, int w, int h) { gfx_FillRectangle(x, y, w, h); }
void screen_hline(int x, int y, int length)  { gfx_HorizLine(x, y, length); }
void screen_vline(int x, int y, int length)  { gfx_VertLine(x, y, length); }
void screen_line(int x0, int y0, int x1, int y1) { gfx_Line(x0, y0, x1, y1); }

void screen_show(void) {
    gfx_SwapDraw();
}

void screen_direct(bool on) {
    if(on)
        gfx_SetDrawScreen();
    else
        gfx_SetDrawBuffer();
}

#else
typedef int screen_ti_unused; /* nothing here on the PC */
#endif
