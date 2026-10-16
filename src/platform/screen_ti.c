#ifdef __TICE__

/* calculator side of screen.h. graphx, drawing to the back buffer and
   swapping on screen_show(). */

#include "screen.h"

#include <graphx.h>
#include <sys/lcd.h>

static bool light_theme;
static unsigned char backlight; /* ours, what waking up goes back to */
static unsigned char os_backlight; /* the OS's, handed back on the way out */
static bool asleep;

void screen_begin(void) {
    gfx_Begin();
    gfx_SetDrawBuffer();
    screen_theme(light_theme);  /* gfx_Begin put its own palette in */

    os_backlight = backlight = lcd_BacklightLevel;
}

void screen_end(void) {
    gfx_End();
    lcd_BacklightLevel = os_backlight;
}

void screen_theme(bool light) {
    light_theme = light;

    /* same colours as screen_pc.c */
    if(light) {
        gfx_palette[COL_BG]     = gfx_RGBTo1555(245, 245, 248);
        gfx_palette[COL_FG]     = gfx_RGBTo1555(24, 24, 32);
        gfx_palette[COL_DIM]    = gfx_RGBTo1555(120, 120, 140);
        gfx_palette[COL_ACCENT] = gfx_RGBTo1555(40, 110, 220);
        gfx_palette[COL_SELECT] = gfx_RGBTo1555(200, 215, 245);
    } else {
        gfx_palette[COL_BG]     = gfx_RGBTo1555(24, 24, 32);
        gfx_palette[COL_FG]     = gfx_RGBTo1555(235, 235, 240);
        gfx_palette[COL_DIM]    = gfx_RGBTo1555(120, 120, 140);
        gfx_palette[COL_ACCENT] = gfx_RGBTo1555(110, 170, 255);
        gfx_palette[COL_SELECT] = gfx_RGBTo1555(40, 64, 112);
    }
}

/* the register goes 0 brightest to 255 off, 32 a step leaves 7 still readable */
void screen_brightness(int level) {
    if(level < 0) level = 0;
    if(level > 7) level = 7;
    backlight = (unsigned char)(level * 32);
    if(!asleep)
        lcd_BacklightLevel = backlight;
}

/* the OS's level from screen_begin in our steps, so a first run starts where
   the OS was instead of jumping */
int screen_brightness_current(void) {
    int level = os_backlight / 32;

    return level > 7 ? 7 : level;
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

/* just the backlight, not the LCD controller: graphx keeps working and nothing
   needs setting up again. 255 is darkest. waking up goes back to whatever
   screen_brightness last said, not what the register had */
void screen_power(bool on) {
    asleep = !on;
    lcd_BacklightLevel = on ? backlight : 255;
}

#else
typedef int screen_ti_unused; /* nothing here on the PC */
#endif
