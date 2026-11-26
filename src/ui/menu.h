#ifndef FLUXION_MENU_H
#define FLUXION_MENU_H

/* the calculator's navigation: the five folders on the bar under the screen, the menus that open over it, and a page's own five keys as a list on the nspire */

#include <stdbool.h>
#include "input/actions.h"
#include "render/layout.h"
#include "ui/catalog.h"

/* what a menu needs from the app. it never touches the editor or the history itself */
typedef struct {
    bool (*pick)(const item_t *it, bool dry); /* what picking an item does, dry only asks */
    box_t *(*preview)(const item_t *it, const layout_font_t *font); /* a row drawn as what picking it types, NULL leaves text */
    void (*will_edit)(void); /* a pick is about to change the line */
    void (*say)(const char *message); /* the status line */
    bool (*said)(void); /* something's on the status line */
    int (*checked)(const item_t *it); /* a setting's switch, 0 or 1, -1 for one that's an action */
} menu_host_t;

void menu_init(const menu_host_t *host);
void menu_free(void);

bool menu_is_open(void);
void menu_open(int folder);
void menu_open_top(void); /* nspire [menu] */
void menu_open_soft(const char *const *names);
void menu_close(void);
/* false if it means nothing in a menu */
bool menu_act(action_t a);

/* the pointer: a click on a row of the open menu as the action for its key, a click on the bar as A_SOFT */
bool menu_click(int x, int y, action_t *a);
bool menu_bar_click(int x, int y, action_t *a);

void menu_draw_bar(void);
/* a page's own five keys on the bar, lit is the one picked (or -1), off a bit per greyed out one */
void menu_draw_keys(const char *const names[5], int lit, unsigned off);
void menu_draw(void);

#endif
