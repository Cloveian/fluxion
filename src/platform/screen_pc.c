#ifndef __TICE__

/* PC version of screen.h. draws into a framebuffer of palette indexes, 320x240 unless the
   window or --size changed it, which the SDL window shows or screen_save_png() writes out. */

#include "screen.h"
#include "window_pc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef SCREEN_RESIZABLE
int screen_w = 320, screen_h = 240;
void (*screen_resized)(void);
static unsigned char *fb;
#define FB(x, y) fb[(y) * screen_w + (x)]

/* blank framebuffer for the size now */
static void fb_make(void) {
    free(fb);
    fb = calloc((size_t)screen_w * (size_t)screen_h, 1);
}

void screen_resize(int w, int h) {
    screen_w = w < SCREEN_MIN_W ? SCREEN_MIN_W : w;
    screen_h = h < SCREEN_MIN_H ? SCREEN_MIN_H : h;
    fb_make();
    if(screen_resized != NULL)
        screen_resized();
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
void screen_palette(const unsigned char rgb[COL_COUNT][3]) { memcpy(palette, rgb, sizeof palette); }

/* just the one framebuffer here, so drawing is always "direct", only needs showing */
void screen_direct(bool on) {
    if(!on)
        window_present();
}

void screen_clear(void) {
#ifdef SCREEN_RESIZABLE
    if(fb == NULL)
        fb_make();
    memset(fb, COL_BG, (size_t)screen_w * (size_t)screen_h);
#else
    memset(fb, COL_BG, sizeof fb);
#endif
}
void screen_color(unsigned char color) { current = color; }

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

const unsigned char *screen_pixels(void) {
#ifdef SCREEN_RESIZABLE
    if(fb == NULL)
        fb_make();
    return fb;
#else
    return &fb[0][0];
#endif
}

void screen_rgb(unsigned char color, unsigned char rgb[3]) {
    memcpy(rgb, palette[color < COL_COUNT ? color : COL_FG], 3);
}

bool screen_png_smooth;

int screen_smooth_factor(int scale) {
    return scale % 3 == 0 ? 3 : scale % 2 == 0 ? 2 : 1;
}

/* the index at x, y, the edges repeated */
static unsigned char index_at(int x, int y) {
    x = x < 0 ? 0 : x >= SCREEN_W ? SCREEN_W - 1 : x;
    y = y < 0 ? 0 : y >= SCREEN_H ? SCREEN_H - 1 : y;
    return FB(x, y);
}

/* scale2x and scale3x (AdvMAME): the corners that meet a same-coloured neighbour on two sides get rounded off. they only compare colours, which is all the palette has */
void screen_expand(int f, unsigned char *rgb) {
    unsigned char lut[COL_COUNT][3], out[9];
    int x, y, k, i;

    for(i = 0; i < COL_COUNT; i++)
        screen_rgb((unsigned char)i, lut[i]);
    for(y = 0; y < SCREEN_H; y++) {
        for(x = 0; x < SCREEN_W; x++) {
            unsigned char e = index_at(x, y), b = index_at(x, y - 1), d = index_at(x - 1, y);
            unsigned char r = index_at(x + 1, y), h = index_at(x, y + 1);

            if(f == 2) {
                out[0] = d == b && d != h && b != r ? b : e;
                out[1] = b == r && b != d && r != h ? r : e;
                out[2] = h == d && h != r && d != b ? d : e;
                out[3] = r == h && r != b && h != d ? h : e;
            } else {
                unsigned char a = index_at(x - 1, y - 1), c = index_at(x + 1, y - 1);
                unsigned char g = index_at(x - 1, y + 1), n = index_at(x + 1, y + 1);

                out[0] = d == b && b != r && d != h ? d : e;
                out[1] = (d == b && b != r && d != h && e != c) || (b == r && b != d && r != h && e != a) ? b : e;
                out[2] = b == r && b != d && r != h ? r : e;
                out[3] = (d == b && b != r && d != h && e != g) || (d == h && d != b && h != r && e != a) ? d : e;
                out[4] = e;
                out[5] = (b == r && b != d && r != h && e != n) || (r == h && r != b && h != d && e != c) ? r : e;
                out[6] = d == h && d != b && h != r ? d : e;
                out[7] = (d == h && d != b && h != r && e != n) || (r == h && r != b && h != d && e != g) ? h : e;
                out[8] = r == h && r != b && h != d ? r : e;
            }
            for(k = 0; k < f * f; k++) {
                size_t at = ((size_t)y * (size_t)f + (size_t)(k / f)) * (size_t)SCREEN_W * (size_t)f + (size_t)x * (size_t)f + (size_t)(k % f);

                memcpy(&rgb[at * 3], lut[out[k]], 3);
            }
        }
    }
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
    unsigned char *big = NULL;  /* smoothed pixels, fac by fac of them per framebuffer pixel */
    int fac = 1;

    if(scale < 1) scale = 1;
    if(scale > 8) scale = 8;
    w = SCREEN_W * scale;
    h = SCREEN_H * scale;
    row_len = 1 + w * 3;

    idat = malloc(2 + (size_t)h * (5 + row_len) + 4);
    if(screen_png_smooth)
        fac = screen_smooth_factor(scale);
    if(fac > 1) /* each of those is scale / fac pixels wide in the file */
        big = malloc((size_t)SCREEN_W * (size_t)SCREEN_H * (size_t)(fac * fac) * 3);
    if(idat == NULL || (fac > 1 && big == NULL)) {
        free(idat);
        free(big);
        return -1;
    }
    if(big != NULL)
        screen_expand(fac, big);
    p = idat;

    *p++ = 0x78;
    *p++ = 0x01;
    for(y = 0; y < h; y++) {
        unsigned char *row;
        const unsigned char *src = NULL;

        if(big != NULL)
            src = big + (size_t)(y * fac / scale) * (size_t)SCREEN_W * (size_t)fac * 3;
        *p++ = y == h - 1;          /* last block */
        *p++ = row_len & 0xFF;  *p++ = (row_len >> 8) & 0xFF;
        *p++ = ~row_len & 0xFF; *p++ = (~row_len >> 8) & 0xFF;

        row = p;
        *p++ = 0;                   /* no filter */
        for(x = 0; x < w; x++) {
            if(src != NULL)
                memcpy(p, src + (size_t)(x * fac / scale) * 3, 3);
            else
                screen_rgb(FB(x / scale, y / scale), p);
            p += 3;
        }

        for(i = 0; i < row_len; i++) {
            a = (a + row[i]) % 65521;
            b = (b + a) % 65521;
        }
    }
    put32(p, (b << 16) | a);
    p += 4;
    free(big);

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
