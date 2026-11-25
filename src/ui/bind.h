#ifndef FLUXION_BIND_H
#define FLUXION_BIND_H

/* what a key does. one table per keypad: bind_ce.c (CE, PC, web) and bind_nspire.c */

#include "platform/keys.h"

typedef enum {
    A_NONE,

    /* typing */
    A_CHAR,         /* n */
    A_FUNC,         /* s, sin(), csc(), ln() ... */
    A_TEMPLATE,     /* n an ED_ kind, fraction, sqrt ... */
    A_POWER,        /* s in an exponent and back out, x^2 x^-1 */
    A_TYPE,         /* s typed out, then template n if it isn't 0. e^, 10^, ans */
    A_ITEM,         /* s, the catalog word, picked like from a menu */

    /* the line */
    A_LEFT, A_RIGHT, A_UP, A_DOWN,
    A_SELECT_LEFT, A_SELECT_RIGHT,
    A_LINE_START, A_LINE_END,
    A_BACKSPACE, A_DELETE,
    A_CLEAR,        /* the CE's [clear], backspace while typing, out of whatever's open */
    A_CLEAR_LINE,   /* n 1 and the line's empty: clears the screen */
    A_CUT, A_COPY, A_PASTE, A_UNDO, A_SELECT_ALL,
    A_POINT, A_POINT_DRAG, /* n is x | y << 12 on the screen, drag and shift-click select */
    A_POPUP_PICK,   /* n the item, a click on it in an open popup */
    A_HIST_POINT,   /* n entry * 2, + 1 for its answer. highlights it, again enters it */
    A_ENTER, A_NEXT_FORM, A_DECIMAL,

    /* the rest of the app */
    A_SOFT,         /* n 0-4, the key under the screen. the folders on the main one */
    A_MENU,         /* the nspire's dropdown */
    A_POPUP,        /* n, POPUP_ */
    A_CATALOG, A_VARS, A_SETTINGS,
    A_ESC,          /* out of what's open, a level at a time */
    A_QUIT,
    A_BRIGHTNESS,   /* n +1 / -1 */
    A_SCREEN_OFF,
    A_CANCEL,       /* stops a long one, nothing otherwize */
    A_MOD           /* n the M_ bit, tapped */
} act_t;

enum { POPUP_MATH, POPUP_TRIG, POPUP_CONSTANTS, POPUP_MARKS };

typedef struct {
    act_t op;
    int n;
    const char *s;
} action_t;

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
