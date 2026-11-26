#ifndef FLUXION_INPUT_H
#define FLUXION_INPUT_H

/* the keypad and mouse as the app sees them, ready made actions and no key codes */

#include <stdbool.h>
#include "ui/actions.h"

typedef struct {
    action_t a; /* with the modifers that are on (tapped, locked or held) */
    action_t plain; /* key on its own, for menus and pages where [2nd] doesnt matter */
    bool pointer; /* click, drag or hover at x y, a and plain are empty */
    bool drag; /* button still down */
    bool hover; /* no button, x below 0 once the mouse left */
    bool shift; /* shift-click extends the selection */
    int x, y;
    bool quit; /* window closed */
    bool eaten; /* modifier tap or screen off, already dealt with */
} input_t;

/* what the app has open, the keypad binds a little differently in each */
typedef enum { IN_TYPING, IN_MENU, IN_POPUP } in_ctx_t;

/* start / stop the keys. on_sleep(true) when the screen should go dark, (false) once it's awake */
void input_begin(void (*on_sleep)(bool asleep));
void input_end(void);

/* waits for the next key or click */
input_t input_wait(in_ctx_t ctx);

/* sticky modifiers for the top bar, a fixed slot each. letter is '2', 'A', 'C' or 'S', 0 when off */
typedef struct {
    char letter;
    bool locked;
} input_mod_t;
void input_mods(input_mod_t out[3]);

/* what stops a long one, "[on]+[mode]" */
const char *input_cancel_hint(void);

/* the key column of the catalog: every binding on this device, what it does and how it's pressed */
unsigned input_binding_count(void);
action_t input_binding(unsigned i, bool *plain);
bool input_binding_text(unsigned i, char *buf, int n);
const char *input_digit_keys(void);
const char *input_letter_keys(void);

/* engine's poll while it works, a modifier tapped meanwhile counts for the next key. true when the cancel key is down */
bool input_poll(void);
/* when the work starts */
void input_poll_reset(void);

#endif
