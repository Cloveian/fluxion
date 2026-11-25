#ifndef FLUXION_KEYS_H
#define FLUXION_KEYS_H

/*
    key presses, which key and nothing about what it does (that's ui/bind_*.c):
      keys_ti.c      calculator: keypadc, with key repeat
      keys_pc.c      PC: the SDL window's keyboard, standing in for the CE's keys
      keys_nspire.c  nspire: its own keypad
    one code per key, named off its label. a key both keypads have shares one.
    tapped modifiers are keys like any other, the app keeps them sticky.
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

    /* [on] tapped on its own (held together with a key it's M_ON in .held instead) */
    K_ON,

    /* nspire only. its del, var, ^ and x^2 are the CE's codes, same label */
    K_MENU, K_TRIG, K_ESC, K_DOC, K_HOME, K_CAT, K_TEMPLATE,
    K_EQUALS, K_PI, K_EE, K_TENX, K_EX,     /* 10^x and e^x */
    K_MARKS,    /* ?!> */
    K_RETURN, K_CLICK,  /* the little enter, and pushing the touchpad down */
    K_CTRL, K_SHIFT,    /* tapped alone */

    /* PC and nspire - letters come straight off the keyboard, no alpha needed */
    K_LETTER_A, K_LETTER_Z = K_LETTER_A + 25,
    K_CAPITAL_A, K_CAPITAL_Z = K_CAPITAL_A + 25,

    /* PC and web page: typed characters the CE has no key for, and End (Home is the nspire's) */
    K_LESS, K_GREATER, K_BANG, K_PERCENT, K_END,

    /* PC: the mouse, in the keypress's x and y (screen pixels). a drag is the button still down, held has M_ALPHA with shift */
    K_POINT_DOWN, K_POINT_DRAG,
    K_REDRAW, /* PC: the window changed size, nothing to do but draw */

    K_QUIT  /* the PC window got closed */
} keycode_t;

/* modifier bits, the CE's three then the nspire's two. tapped on their own
   (down and up, nothing in bewteen) they come as K_ON, K_2ND, K_CTRL ... */
enum { M_2ND = 1, M_ALPHA = 2, M_ON = 4, M_CTRL = 8, M_SHIFT = 16 };

typedef struct {
    keycode_t key;
    unsigned held;  /* M_ bits held down with it */
    short x, y;     /* K_POINT_DOWN and K_POINT_DRAG */
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
keycode_t keys_named(const char *name);
keycode_t keys_from_char(char c);
#endif

#endif
