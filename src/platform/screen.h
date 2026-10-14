#ifndef FLUXION_SCREEN_H
#define FLUXION_SCREEN_H

/*
    everything Fluxion does to the screen goes through here.
      screen_ti.c  calculator: graphx, double buffered
      screen_pc.c  PC: a 320x240 framebuffer, shown in an SDL window or saved as a PNG
    above this layer (layout, drawing, editor, screens) the code is the same on both.
*/

#define SCREEN_W 320
#define SCREEN_H 240

/* palette indexes. the colours behind them are the same on both. */
enum {
    COL_BG, /* background */
    COL_FG,
    COL_DIM, /* timings, labels */
    COL_ACCENT, /* cursor + highlights */
    COL_COUNT
};

void screen_begin(void);
void screen_end(void);

void screen_clear(void);                        /* fill with COL_BG */
void screen_color(unsigned char color);         /* for everything drawn after */

void screen_fill(int x, int y, int w, int h);
void screen_hline(int x, int y, int length);
void screen_vline(int x, int y, int length);
void screen_line(int x0, int y0, int x1, int y1);
/* (text is drawn by render/draw.c with these, so any font size works on both) */

void screen_show(void);                         /* show what's been drawn */

#ifndef __TICE__
/* PC only */
const unsigned char *screen_pixels(void);       /* SCREEN_W * SCREEN_H palette indexes */
void screen_rgb(unsigned char color, unsigned char rgb[3]);
int screen_save_png(const char *path, int scale);   /* scale 1-8, 0 on success */
#endif

#endif
