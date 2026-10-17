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
#include <time.h>   /* CLOCKS_PER_SEC for the idle timeout */

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

    /* [on] tapped on its own (held together with a key it's .on instead) */
    K_ON,

    /* PC only - letters come straight off the keyboard, no alpha needed */
    K_LETTER_A, K_LETTER_Z = K_LETTER_A + 25,
    K_CAPITAL_A, K_CAPITAL_Z = K_CAPITAL_A + 25,

    K_QUIT  /* the PC window got closed */
} key_t;

/* [on], [2nd] and [alpha] held down while the key was pressed. tapped on their
   own (down and up, nothing in between) they come as K_ON / K_2ND / K_ALPHA */
typedef struct {
    key_t key;
    bool on;    /* ctrl, i.e. [on] being held */
    bool second, alpha;
} keypress_t;

void keys_begin(void);
void keys_end(void);

/* waits for the next press. arrows, clear and del repeat while held. */
keypress_t keys_wait(void);

/* doesn't wait. the key down right now (K_NONE if none) and [on], for checking
   while the engine works; that key won't come back from keys_wait() as a press. */
keypress_t keys_peek(void);

/*
    going dark. after KEYS_IDLE_OFF of waiting with no key, keys_wait() calls
    keys_on_sleep(true) (the app turns the screen off) and the next key calls it
    with false and gets eaten, like the TI's own APD does with the key that wakes
    it. keys_sleep() does it right away ([2nd][alpha][on]). the idle clock starts
    when keys_wait() is entered, so a long solve doesn't count as idling.
*/
#define KEYS_IDLE_OFF (CLOCKS_PER_SEC * 120)   /* 2 min for now, a mode setting later */
extern void (*keys_on_sleep)(bool asleep);
void keys_sleep(void);

#ifndef __TICE__
/* PC only, for tests. presses come from this list instead of the keyboard, then
   quit. each is a key name (enter, clear, y=, x^2, 2nd, ...) or one character to
   type, with "on+", "2nd+" or "alpha+" in front for held ones. */
void keys_script(char *const *presses, int count);

/* PC and browser: the key a script name stands for, or the one for a typed
   character. K_NONE if it's neither. in web.c, it has no SDL */
key_t keys_named(const char *name);
key_t keys_from_char(char c);
#endif

#endif
