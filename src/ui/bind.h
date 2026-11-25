#ifndef FLUXION_BIND_H
#define FLUXION_BIND_H

/* what a key does. one table per keypad: bind_ce.c (CE, PC, web) and bind_nspire.c */

#include "platform/keys.h"
#include "ui/actions.h"

/* mods is everything on, held or sticky */
action_t bind_key(keycode_t key, unsigned mods);

/* "[2nd][x^2]", "ctrl+x^2". false if the key has no label */
bool bind_text(keycode_t key, unsigned mods, char *buf, int n);

/* the keypad table, from bind_ce.c / bind_nspire.c */
typedef struct {
    keycode_t key;
    unsigned mods;
    action_t a;
} bind_t;

/* what a key's called, for the catalog's key column */
typedef struct {
    keycode_t key;
    const char *label;
} key_label_t;

typedef struct {
    const bind_t *binds;
    unsigned count;
    const key_label_t *labels;
    unsigned label_count;
    struct { unsigned bit; const char *text; } mod_text[3];    /* in front of the key, in this order */
    const char *digits, *letters;   /* the catalog's 0-9 and a-z, a whole row of keys each */
    unsigned drop[2];   /* nothing bound, try again without these, one at a time. the rest eat the key */
    unsigned caps;      /* all of these + a letter is a capital */
    unsigned caps_with; /* what's left looked up with this ([alpha] on the CE) */
    unsigned shown[3];  /* the sticky ones drawn in the top bar, in order */
    const char *cancel; /* what stops a long one, for the hint */
} keypad_t;

extern const keypad_t keypad;

#endif
