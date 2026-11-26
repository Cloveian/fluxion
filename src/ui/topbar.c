/* the bar along the top: the keypad's modifier padlocks, the form, the battery and the working dot */

#include "topbar.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "areas.h"
#include "input/input.h"
#include "platform/power.h"
#include "platform/screen.h"
#include "render/draw.h"

#define BATTERY_W   13
#define BATTERY_READ (CLOCKS_PER_SEC * 10)     /* the level is read this often (reading every frame was slow) */
#define LOCK_SLOT   13 /* per padlock, fixed so one turning on doesn't shove the others along */
#define BAR_LEFT    (areas.top.x + MARGIN)
#define BAR_RIGHT   (areas.top.x + areas.top.w - MARGIN)
#define BAR_BEFORE_BATTERY (BAR_RIGHT - BATTERY_W - 6) /* right edge of the form name and the busy dot */

static int battery = -1;
static clock_t battery_next;

/* 3x5 letters punched out of the padlocks, top bit on the left: 2, A, C, S */
static const unsigned char lock_letters[4][5] = {
    { 6, 1, 2, 4, 7 },
    { 2, 5, 7, 5, 5 },
    { 3, 4, 4, 4, 3 },
    { 3, 4, 2, 1, 6 }
};

/* 9 x 11, top left at x, y. a lock is the padlock with the letter punched out
   of it, a plain sticky press is just the letter on its own, bigger, so you can
   tell the two apart at a glance (the open-shackle version looked locked too) */
static void draw_padlock(int x, int y, char name, bool locked) {
    int r, c, letter = (int)(strchr("2ACS", name) - "2ACS");

    screen_color(COL_ACCENT);
    if(!locked) {
        for(r = 0; r < 5; r++)
            for(c = 0; c < 3; c++)
                if(lock_letters[letter][r] & (4 >> c))
                    screen_fill(x + 1 + 2 * c, y + 1 + 2 * r, 2, 2);
        return;
    }
    screen_hline(x + 3, y, 3);
    screen_vline(x + 2, y + 1, 3);
    screen_vline(x + 6, y + 1, 3);
    screen_fill(x, y + 4, 9, 7);

    /* the letter is just holes in the body, it still reads at 1x */
    screen_color(COL_BG);
    for(r = 0; r < 5; r++)
        for(c = 0; c < 3; c++)
            if(lock_letters[letter][r] & (4 >> c))
                screen_hline(x + 3 + c, y + 5 + r, 1);
}

/* BATTERY_W x 7, top left at x, y, two columns of fill per bar (0-4) */
static void draw_battery(int x, int y, int level) {
    screen_hline(x, y, 12);
    screen_hline(x, y + 6, 12);
    screen_vline(x, y, 7);
    screen_vline(x + 11, y, 7);
    screen_vline(x + 12, y + 2, 3);
    if(level > 0)
        screen_fill(x + 2, y + 2, level * 2, 3);
}

void topbar_draw(const char *form) {
    int h = areas.top.h, top = areas.top.y, y = top + (h - 1 - 11) / 2;
    int left = BAR_LEFT, right = BAR_RIGHT, i;
    input_mod_t mods[3];

    if(battery < 0 || clock() >= battery_next) {
        battery = power_battery();
        battery_next = clock() + BATTERY_READ;
    }

    screen_color(COL_BG);
    screen_fill(areas.top.x, top, areas.top.w, h);
    screen_color(COL_DIM);
    screen_hline(left, top + h - 1, right - left);
    draw_battery(right - BATTERY_W, top + (h - 1 - 7) / 2, battery);

    input_mods(mods);
    for(i = 0; i < 3; i++)
        if(mods[i].letter != '\0')
            draw_padlock(left + i * LOCK_SLOT, y, mods[i].letter, mods[i].locked);

    /* the form, when it isn't the normal one, over by the battery */
    if(form != NULL) {
        const font_t *small = draw_label_font();

        screen_color(COL_DIM);
        draw_text(BAR_BEFORE_BATTERY - draw_text_width(form, small), top + 1, form, small);
    }
}

/* a dot going round in the status bar (left of the battery) while the engine
   runs, so a slow simplify doesn't look frozen. drawn straight onto the screen,
   swapping buffers made the cursor flash */
void topbar_working(unsigned frame) {
    /* 2x2 dots round an 8x8 box, clockwise from the top */
    static const unsigned char dots[8][2] = {
        { 3, 0 }, { 5, 1 }, { 6, 3 }, { 5, 5 }, { 3, 6 }, { 1, 5 }, { 0, 3 }, { 1, 1 }
    };
    int x = BAR_BEFORE_BATTERY - 8, y = areas.top.y + (areas.top.h - 1 - 8) / 2;
    unsigned i;

    /* a head and two dimmer dots behind it, going round */
    screen_direct(true);
    for(i = 0; i < 8; i++) {
        unsigned behind = (frame - i) % 8;

        screen_color(behind == 0 ? COL_ACCENT : behind <= 2 ? COL_DIM : COL_BG);
        screen_fill(x + dots[i][0], y + dots[i][1], 2, 2);
    }
    screen_direct(false);
}

/* "12s, [on]+[mode] stops it" after the padlocks. no guess at the total, 1/x+1/(x+1)=1 is one short line and ~17 s on the calc */
void topbar_hint(unsigned long s) {
    const font_t *small = draw_label_font();
    int x = BAR_LEFT + 3 * LOCK_SLOT + 4;
    char t[32];

    snprintf(t, sizeof t, "%lus, %s stops it", s, input_cancel_hint());
    screen_direct(true);
    screen_color(COL_BG);
    screen_fill(x, areas.top.y, draw_text_width(t, small), areas.top.h - 1);
    screen_color(COL_DIM);
    draw_text(x, areas.top.y + 1, t, small);
    screen_direct(false);
}

