#ifndef FLUXION_WORK_H
#define FLUXION_WORK_H

/*
    the steps page, show the work for a solve. the methods are the soft keys, and
    the app hands it its keys while it's open, like a page.
*/

#include <stdbool.h>
#include "platform/keys.h"

/* src is the line inside solve( ), it gets cut up. NULL when it opened, or what to say */
const char *work_open(char *src);
bool work_is_open(void);
void work_close(void);
void work_key(keypress_t p);
/* everything under the status bar */
void work_draw(void);

#endif
