#ifndef FLUXION_POPUP_H
#define FLUXION_POPUP_H

/* the grids of symbols and functions that open at the cursor */

#include <stdbool.h>
#include "input/actions.h"

/* what picking a cell does to the line */
typedef struct {
    void (*will_edit)(void);
    void (*insert_func)(const char *name);
    void (*insert_char)(char c);
} popup_host_t;

void popup_init(const popup_host_t *host);

bool popup_is_open(void);
/* which is a POPUP_ from actions.h */
void popup_open(int which);
/* false if it means nothing in a popup */
bool popup_act(action_t a);
/* a click at x y as the action for it, A_ESC outside the grid */
action_t popup_click(int x, int y);
/* just above the cursor, cur_x and cur_top are where it was drawn */
void popup_draw(int cur_x, int cur_top);

#endif
