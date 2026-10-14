#ifndef __TICE__

/* PC version of screen.h. draws into a 320x240 framebuffer of palette indexes,
   which the SDL window shows or screen_save_png() writes out. */

#include "screen.h"
#include "window_pc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char fb[SCREEN_H][SCREEN_W];
static unsigned char current = COL_FG;

/* same colours as screen_ti.c */
static const unsigned char palette[COL_COUNT][3] = {
    {  24,  24,  32 },  /* COL_BG */
    { 235, 235, 240 },  /* COL_FG */
    { 120, 120, 140 },  /* COL_DIM */
    { 110, 170, 255 },  /* COL_ACCENT */
};

void screen_begin(void) { screen_clear(); }
void screen_end(void) {}
void screen_show(void) { window_present(); }   /* does nothing unless a window is open */

/* just the one framebuffer here, so drawing is always "direct", only needs showing */
void screen_direct(bool on) {
    if(!on)
        window_present();
}

void screen_clear(void) { memset(fb, COL_BG, sizeof fb); }
void screen_color(unsigned char color) { current = color; }

static void pixel(int x, int y) {
    if(x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H)
        fb[y][x] = current;
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

const unsigned char *screen_pixels(void) { return &fb[0][0]; }

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

    if(scale < 1) scale = 1;
    if(scale > 8) scale = 8;
    w = SCREEN_W * scale;
    h = SCREEN_H * scale;
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
            screen_rgb(fb[y / scale][x / scale], p);
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
