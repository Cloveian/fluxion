#ifndef FLUXION_KEYS_H
#define FLUXION_KEYS_H

/*
    key presses, the same on both:
      keys_ti.c  calculator: keypadc, with key repeat
      keys_pc.c  PC: the SDL window's keyboard, mapped to TI keys
    one key per TI key. 2nd and alpha are keys like any other: the app keeps
    track of them, like the TI does. [on] is a modifier (Ctrl), reported with
    each press. see docs/design-choices/input.md.
*/

#include <stdbool.h>

typedef enum {
    K_NONE,

    /* top rows */
    K_YEQU, K_WINDOW, K_ZOOM, K_TRACE, K_GRAPH, /* the soft keys, left to right */
    K_2ND, K_MODE, K_DEL,
    K_ALPHA, K_XT, K_STAT,    /* K_XT is the [x,t,theta,n] key */
    K_MATH, K_APPS, K_PRGM, K_VARS, K_CLEAR,
    K_RECIP, K_SIN, K_COS, K_TAN, K_POW,
    K_SQUARE, K_COMMA, K_LPAREN, K_RPAREN, K_DIV,
    K_LOG, K_MUL, K_LN, K_SUB, K_STO, K_ADD,

    /* number pad */
    K_0, K_1, K_2, K_3, K_4, K_5, K_6, K_7, K_8, K_9,
    K_DOT, K_NEG, K_ENTER,

    K_UP, K_DOWN, K_LEFT, K_RIGHT,

    /* PC only - letters come straight off the keyboard, no alpha needed */
    K_LETTER_A, K_LETTER_Z = K_LETTER_A + 25,

    K_QUIT  /* the PC window got closed */
} key_t;

typedef struct {
    key_t key;
    bool on;    /* ctrl, i.e. [on] being held */
} keypress_t;

void keys_begin(void);
void keys_end(void);

/* waits for the next press. arrows, clear and del repeat while held. */
keypress_t keys_wait(void);

/* doesn't wait. the key down right now (K_NONE if none) and [on], for checking
   while the engine works; that key won't come back from keys_wait() as a press. */
keypress_t keys_peek(void);

#ifndef __TICE__
/* PC only, for tests. presses come from this list instead of the keyboard, then
   quit. each is a key name (enter, clear, y=, x^2, 2nd, ...) or one character to
   type, with "on+" in front for [on] held. */
void keys_script(char *const *presses, int count);
#endif

#endif
