#ifdef _TINSPIRE

/* nspire side of window_present(): screen_pc.c's framebuffer through the palette to
   565, then Ndless's lcd_blit (which turns it round for the CX screens that sit sideways) */

#include "screen.h"
#include "window_pc.h"

#include <libndls.h>
#include <stdlib.h>

static unsigned short lcd[SCREEN_H * SCREEN_W]; /* 150 KB */
static bool ready;

static void give_back(void) {
    lcd_init(SCR_TYPE_INVALID); /* the OS's own screen mode */
}

void window_present(void) {
    const unsigned char *px = screen_pixels();
    unsigned short pal[COL_COUNT];
    unsigned char rgb[3];
    int i;

    if(!ready) {
        lcd_init(SCR_320x240_565);
        atexit(give_back);
        ready = true;
    }
    for(i = 0; i < COL_COUNT; i++) {
        screen_rgb((unsigned char)i, rgb);
        pal[i] = (unsigned short)((rgb[0] >> 3) << 11 | (rgb[1] >> 2) << 5 | rgb[2] >> 3);
    }
    for(i = 0; i < SCREEN_W * SCREEN_H; i++)
        lcd[i] = pal[px[i]];
    lcd_blit(lcd, SCR_320x240_565);
}

#else
typedef int window_nspire_unused; /* nothing here off the nspire */
#endif
