#ifndef FLUXION_INPUT_H
#define FLUXION_INPUT_H

/* the keypad and mouse as the app sees them, ready made actions and no key codes */

#include <stdbool.h>
#include "ui/actions.h"

typedef struct {
    action_t a; /* with the modifers that are on (tapped, locked or held) */
    action_t plain; /* key on its own, for menus and pages where [2nd] doesnt matter */
    bool pointer; /* click or drag at x y, a and plain are empty */
    bool drag; /* button still down */
    bool shift; /* shift-click extends the selection */
    int x, y;
    bool quit; /* window closed */
} input_t;

/* waits for the next key or click */
input_t input_wait(void);

/* engine's poll while it works, a modifier tapped meanwhile counts for the next key. true when the cancel key is down */
bool input_poll(void);
/* when the work starts */
void input_poll_reset(void);

#endif
