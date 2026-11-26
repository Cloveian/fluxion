#ifndef FLUXION_SCREEN_H
#define FLUXION_SCREEN_H

/*
    everything Fluxion does to the screen goes through here.
      screen_ti.c  calculator: graphx, double buffered
      screen_pc.c  PC: a framebuffer, shown in an SDL window or saved as a PNG
    above this layer (layout, drawing, editor, screens) the code is the same on both.
*/

#include <stdbool.h>

/* calculators and the web page are one fixed size, the PC window is any */
#if defined(COMPILE_PC) && !defined(__EMSCRIPTEN__) && !defined(__TICE__) && !defined(_TINSPIRE)
#define SCREEN_RESIZABLE
extern int screen_w, screen_h;
#define SCREEN_W screen_w
#define SCREEN_H screen_h
#define SCREEN_MIN_W 240 /* the soft key names stop fitting under that */
#define SCREEN_MIN_H 160

/* new size, clamped to the minumum. the framebuffer starts blank, then screen_resized runs */
void screen_resize(int w, int h);
extern void (*screen_resized)(void);
#else
#define SCREEN_W 320
#define SCREEN_H 240
#endif

/* palette indexes. the colours behind them are the same on both. */
enum {
    COL_BG, /* background */
    COL_FG,
    COL_DIM, /* timings, labels */
    COL_ACCENT, /* cursor + highlights */
    COL_SELECT, /* behind selected stuff, dark so the text on it still reads */
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

/* true draws straight onto what's showing, for a small change between frames
   (the busy sign). screen_show() would bring back the frame before, it's double
   buffered. false: back to drawing the next frame */
void screen_direct(bool on);

/* the backlight. off after sitting idle (keys.h) or on [2nd][alpha][on]. drawing
   still works while its off, it's just dark. on the PC it does nothing */
void screen_power(bool on);

/* [2nd] up/down. 0 brightest .. 7 darkest, our own steps, not the OS's. the
   PC just remembers it, there's no backlight on a monitor to poke */
void screen_brightness(int level);

/* what the OS had when screen_begin ran, in those same steps. the default
   before anything's saved */
int screen_brightness_current(void);

/* the colours behind the COL_ indexes, so nothing that draws has to know. kept across screen_begin */
void screen_palette(const unsigned char rgb[COL_COUNT][3]);

#ifndef __TICE__
/* PC only */
#ifdef SCREEN_RESIZABLE
const unsigned char *screen_rgb_buffer(void);   /* SCREEN_W * SCREEN_H * 3 bytes of RGB */
#else
const unsigned char *screen_pixels(void);       /* SCREEN_W * SCREEN_H palette indexes */
#endif
void screen_rgb(unsigned char color, unsigned char rgb[3]);
int screen_save_png(const char *path, int scale);   /* scale 1-8, 0 on success */
#endif

#endif
