#ifndef FLUXION_ACTIONS_H
#define FLUXION_ACTIONS_H

/* what the app can be asked to do. the bind tables turn keys and clicks into these */

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
    A_POINT, A_POINT_DRAG, /* n x and m y on the screen, drag and shift-click select */
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
    int m; /* y of a point */
} action_t;

#endif
