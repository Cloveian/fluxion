#include "hover.h"

#ifndef HOVER_NONE

#define HOT_MAX 64 /* menu rows, popup cells and the soft keys of one frame */

typedef struct { short x, y, w, h, n; } hot_t; /* n 0 is one rect, over 0 a bar of n cells */

static int mx = -1, my = -1;
static hot_t hot[HOT_MAX];
static int hot_n;
static bool overflow; /* more than fit, so a move can't be judged */

static bool inside(int px, int py, int x, int y, int w, int h) {
    return px >= x && px < x + w && py >= y && py < y + h;
}

static void note(int x, int y, int w, int h, int n) {
    if(hot_n == HOT_MAX) {
        overflow = true;
        return;
    }
    hot[hot_n].x = (short)x;
    hot[hot_n].y = (short)y;
    hot[hot_n].w = (short)w;
    hot[hot_n].h = (short)h;
    hot[hot_n].n = (short)n;
    hot_n++;
}

/* which spots of the last frame the mouse is over, and which cell of a bar, as one number to compre */
static unsigned long under(int px, int py) {
    unsigned long s = 0;
    int i;

    for(i = 0; i < hot_n; i++) {
        const hot_t *t = &hot[i];
        unsigned long d = 0;

        if(inside(px, py, t->x, t->y, t->w, t->h))
            d = 1 + (t->n > 0 ? (unsigned long)((px - t->x) * t->n / t->w) : 0);
        s = s * 31 + d;
    }
    return s;
}

void hover_frame(void) {
    hot_n = 0;
    overflow = false;
}

bool hover_set(int x, int y) {
    unsigned long before = under(mx, my);

    mx = x;
    my = y;
    return overflow || under(mx, my) != before;
}

bool hover_in(int x, int y, int w, int h) {
    note(x, y, w, h, 0);
    return inside(mx, my, x, y, w, h);
}

int hover_cell(const rect_t *bar, int n) {
    if(n <= 0)
        return -1;
    note(bar->x, bar->y, bar->w, bar->h, n);
    if(!inside(mx, my, bar->x, bar->y, bar->w, bar->h))
        return -1;
    return (mx - bar->x) * n / bar->w;
}

#endif
