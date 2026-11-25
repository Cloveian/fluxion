#include "hover.h"

static int mx = -1, my = -1;

void hover_set(int x, int y) {
    mx = x;
    my = y;
}

bool hover_in(int x, int y, int w, int h) {
    return mx >= x && mx < x + w && my >= y && my < y + h;
}

int hover_cell(const rect_t *bar, int n) {
    if(n <= 0 || !hover_in(bar->x, bar->y, bar->w, bar->h))
        return -1;
    return (mx - bar->x) * n / bar->w;
}
