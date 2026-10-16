#ifndef FLUXION_PAGE_H
#define FLUXION_PAGE_H

/*
    full screen pages over the main screen: the catalog now, a mode menu and
    whatever else later. a page is a titled list, each row has a label, maybe a
    value on the right (a setting's state, the keys that type it) and a help
    text that shows under the list for the highlighted row.

    pages stack, so one can open another (mode -> a submenu) and [clear] goes
    back one. the app hands its keys to page_key() while one is open.
*/

#include <stdbool.h>
#include "platform/keys.h"

typedef struct {
    const char *title;
    int count;
    const char *(*label)(int i);
    const char *(*value)(int i); /* NULL for none, on the page or for a row */
    const char *(*help)(int i); /* same */
    bool (*pick)(int i); /* [enter], true closes the page */
    bool (*header)(int i); /* section title rows the cursor hops over, NULL if none */
} page_t;

void page_open(const page_t *page);
void page_close(void);
bool page_is_open(void);

/* letter is what [alpha] made of the key (0 if nothing), it jumps to the first
   row starting with it */
void page_key(keypress_t p, char letter);

/* everything under the status bar, from top to the bottom of the screen */
void page_draw(int top);

#endif
