#ifndef __TICE__

/* PC version of screen.h. the web page and the nspire get a 320x240 framebuffer of palette indexes, the desktop
   one is RGB at the window's real size (or --size times --scale) so text can be blended. drawing still takes layout
   pixels, each one ppu real pixels across. the SDL window shows it, screen_save_png() writes it out */

#include "screen.h"
#include "window_pc.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef SCREEN_RESIZABLE
int screen_w = 320, screen_h = 240;
void (*screen_resized)(void);
static unsigned char *fb; /* 3 bytes a pixel, pw x ph real pixels */
static int pw = 320, ph = 240;
static float ppu = 1; /* real pixels to a layout pixel */
static unsigned char now[3]; /* rgb of current */
#define FB(x, y) (&fb[((size_t)(y) * (size_t)pw + (size_t)(x)) * 3])
#define PX(v) ((int)lroundf((float)(v) * ppu)) /* where a layout pixel edge lands */

/* blank framebuffer for the size now */
static void fb_make(void) {
    free(fb);
    fb = calloc((size_t)pw * (size_t)ph * 3, 1);
}

void screen_set_view(int w, int h, float per) {
    pw = w < 1 ? 1 : w; /* 0 while minimzed */
    ph = h < 1 ? 1 : h;
    ppu = per < 0.1f ? 1 : per;
    /* under the minimum layout the pixel gets smaller to fit it */
    if((float)pw < (float)SCREEN_MIN_W * ppu)
        ppu = (float)pw / SCREEN_MIN_W;
    if((float)ph < (float)SCREEN_MIN_H * ppu)
        ppu = (float)ph / SCREEN_MIN_H;
    screen_w = (int)((float)pw / ppu + 0.001f);
    screen_h = (int)((float)ph / ppu + 0.001f);
    fb_make();
    if(screen_resized != NULL)
        screen_resized();
}

float screen_ppu(void) { return ppu; }

void screen_pixel_size(int *w, int *h) {
    *w = pw;
    *h = ph;
}
#else
static unsigned char fb[SCREEN_H][SCREEN_W];
#define FB(x, y) fb[(y)][(x)]
#endif
static unsigned char current = COL_FG;

static unsigned char palette[COL_COUNT][3]; /* ui/theme.c fills it */
static int brightness;  /* nothing reads it, it's here so the PC keeps the same state */

void screen_begin(void) { screen_clear(); }
void screen_end(void) {}
void screen_show(void) { window_present(); }   /* does nothing unless a window is open */
void screen_power(bool on) { (void)on; } /* no backlight to turn off here */
void screen_brightness(int level) { brightness = level; }
int screen_brightness_current(void) { return 2; } /* no OS level to start from, 2 is as good as any */
void screen_palette(const unsigned char rgb[COL_COUNT][3]) {
    memcpy(palette, rgb, sizeof palette);
    screen_color(current);
}

/* just the one framebuffer here, so drawing is always "direct", only needs showing */
void screen_direct(bool on) {
    if(!on)
        window_present();
}

void screen_clear(void) {
#ifdef SCREEN_RESIZABLE
    size_t i, n = (size_t)pw * (size_t)ph;

    if(fb == NULL)
        fb_make();
    for(i = 0; i < n; i++)
        memcpy(&fb[i * 3], palette[COL_BG], 3);
#else
    memset(fb, COL_BG, sizeof fb);
#endif
}

void screen_color(unsigned char color) {
    current = color;
#ifdef SCREEN_RESIZABLE
    screen_rgb(color, now);
#endif
}

#ifdef SCREEN_RESIZABLE
/* real pixels x0 to x1 and y0 to y1, cut to the framebuffer */
static void rect(int x0, int y0, int x1, int y1) {
    int x, y;

    x0 = x0 < 0 ? 0 : x0;
    y0 = y0 < 0 ? 0 : y0;
    x1 = x1 > pw ? pw : x1;
    y1 = y1 > ph ? ph : y1;
    for(y = y0; y < y1; y++)
        for(x = x0; x < x1; x++)
            memcpy(FB(x, y), now, 3);
}

static void pixel(int x, int y) {
    rect(PX(x), PX(y), PX(x + 1), PX(y + 1));
}

void screen_fill(int x, int y, int w, int h) {
    rect(PX(x), PX(y), PX(x + w), PX(y + h));
}
#else
static void pixel(int x, int y) {
    if(x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H)
        FB(x, y) = current;
}

void screen_fill(int x, int y, int w, int h) {
    int i, j;
    for(j = 0; j < h; j++)
        for(i = 0; i < w; i++)
            pixel(x + i, y + j);
}
#endif

void screen_hline(int x, int y, int length) { screen_fill(x, y, length, 1); }
void screen_vline(int x, int y, int length) { screen_fill(x, y, 1, length); }

/* Bresenham */
void screen_line(int x0, int y0, int x1, int y1) {
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    for(;;) {
        int e2 = 2 * err;
        pixel(x0, y0);
        if(x0 == x1 && y0 == y1)
            break;
        if(e2 >= dy) { err += dy; x0 += sx; }
        if(e2 <= dx) { err += dx; y0 += sy; }
    }
}

#ifdef SCREEN_RESIZABLE
const unsigned char *screen_rgb_buffer(void) {
    if(fb == NULL)
        fb_make();
    return fb;
}
#else
const unsigned char *screen_pixels(void) {
    return &fb[0][0];
}
#endif

void screen_rgb(unsigned char color, unsigned char rgb[3]) {
    memcpy(rgb, palette[color < COL_COUNT ? color : COL_FG], 3);
}

/* PNG, no libraries. just uncompressed deflate blocks so the files come out big, whatever */
/* only the deflate side, no inflater */
/* inflater? i har... opp... nvm (・_・;) */

static unsigned long crc_table[256];

static unsigned long crc(unsigned long c, const unsigned char *buf, size_t len) {
    size_t i;

    if(crc_table[1] == 0) {
        unsigned long n, k;
        for(n = 0; n < 256; n++) {
            unsigned long v = n;
            for(k = 0; k < 8; k++)
                v = v & 1 ? 0xEDB88320UL ^ (v >> 1) : v >> 1;
            crc_table[n] = v;
        }
    }

    for(i = 0; i < len; i++)
        c = crc_table[(c ^ buf[i]) & 0xFF] ^ (c >> 8);
    return c;
}

static void put32(unsigned char *p, unsigned long v) {
    p[0] = (unsigned char)(v >> 24); p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);  p[3] = (unsigned char)v;
}

static void chunk(FILE *f, const char *type, const unsigned char *data, size_t len) {
    unsigned char head[8], tail[4];
    unsigned long c;

    put32(head, (unsigned long)len);
    memcpy(head + 4, type, 4);
    c = crc(0xFFFFFFFFUL, head + 4, 4);
    c = crc(c, data, len) ^ 0xFFFFFFFFUL;
    put32(tail, c);

    fwrite(head, 1, 8, f);
    fwrite(data, 1, len, f);
    fwrite(tail, 1, 4, f);
}

int screen_save_png(const char *path, int scale) {
    /* one row = filter byte + RGB. each row goes in its own stored deflate block. */
    static const unsigned char signature[8] = { 137, 'P', 'N', 'G', 13, 10, 26, 10 };
    int w, h, row_len, x, y, i;
    unsigned char ihdr[13], *idat, *p;
    unsigned long a = 1, b = 0;   /* adler32 */
    FILE *f;

#ifdef SCREEN_RESIZABLE
    (void)scale; /* the view has it already, main_pc.c set it */
    w = pw;
    h = ph;
#else
    if(scale < 1) scale = 1;
    if(scale > 8) scale = 8;
    w = SCREEN_W * scale;
    h = SCREEN_H * scale;
#endif
    row_len = 1 + w * 3;

    idat = malloc(2 + (size_t)h * (5 + row_len) + 4);
    if(idat == NULL)
        return -1;
    p = idat;

    *p++ = 0x78;
    *p++ = 0x01;
    for(y = 0; y < h; y++) {
        unsigned char *row;

        *p++ = y == h - 1;          /* last block */
        *p++ = row_len & 0xFF;  *p++ = (row_len >> 8) & 0xFF;
        *p++ = ~row_len & 0xFF; *p++ = (~row_len >> 8) & 0xFF;

        row = p;
        *p++ = 0;                   /* no filter */
        for(x = 0; x < w; x++) {
#ifdef SCREEN_RESIZABLE
            memcpy(p, FB(x, y), 3);
#else
            screen_rgb(FB(x / scale, y / scale), p);
#endif
            p += 3;
        }

        for(i = 0; i < row_len; i++) {
            a = (a + row[i]) % 65521;
            b = (b + a) % 65521;
        }
    }
    put32(p, (b << 16) | a);
    p += 4;

    put32(ihdr, (unsigned long)w);
    put32(ihdr + 4, (unsigned long)h);
    ihdr[8] = 8;    /* bits per channel */
    ihdr[9] = 2;
    ihdr[10] = ihdr[11] = ihdr[12] = 0;

    f = fopen(path, "wb");
    if(f == NULL) {
        free(idat);
        return -1;
    }
    fwrite(signature, 1, 8, f);
    chunk(f, "IHDR", ihdr, sizeof ihdr);
    chunk(f, "IDAT", idat, (size_t)(p - idat));
    chunk(f, "IEND", NULL, 0);
    free(idat);
    return fclose(f) == 0 ? 0 : -1;
}

#else
typedef int screen_pc_unused; /* nothing here on the calculator */
#endif
