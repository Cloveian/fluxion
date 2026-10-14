#ifdef __TICE__

/* calculator side of keys.h. scans the keypad with keypadc, a new key fires
   once; arrows, clear and del repeat while held (like the TI's own repeat). */

#include "keys.h"

#include <keypadc.h>
#include <time.h>

#define FIRST_REPEAT (CLOCKS_PER_SEC / 4)       /* held 250 ms before it repeats */
#define NEXT_REPEAT  (CLOCKS_PER_SEC / 35)      /* then 35 a second */
#define SETTLE       (CLOCKS_PER_SEC / 50)      /* 20 ms after a press, see keys_wait(). might need to bump this, some keys still double once in a while */

typedef struct {
    unsigned char group, bit;
    key_t key;
} map_t;

static const map_t map[] = {
    { 1, kb_Graph, K_GRAPH },   { 1, kb_Trace, K_TRACE },   { 1, kb_Zoom, K_ZOOM },
    { 1, kb_Window, K_WINDOW }, { 1, kb_Yequ, K_YEQU },     { 1, kb_2nd, K_2ND },
    { 1, kb_Mode, K_MODE },     { 1, kb_Del, K_DEL },
    { 2, kb_Sto, K_STO },       { 2, kb_Ln, K_LN },         { 2, kb_Log, K_LOG },
    { 2, kb_Square, K_SQUARE }, { 2, kb_Recip, K_RECIP },   { 2, kb_Math, K_MATH },
    { 2, kb_Alpha, K_ALPHA },
    { 3, kb_0, K_0 },           { 3, kb_1, K_1 },           { 3, kb_4, K_4 },
    { 3, kb_7, K_7 },           { 3, kb_Comma, K_COMMA },   { 3, kb_Sin, K_SIN },
    { 3, kb_Apps, K_APPS },     { 3, kb_GraphVar, K_XT },
    { 4, kb_DecPnt, K_DOT },    { 4, kb_2, K_2 },           { 4, kb_5, K_5 },
    { 4, kb_8, K_8 },           { 4, kb_LParen, K_LPAREN }, { 4, kb_Cos, K_COS },
    { 4, kb_Prgm, K_PRGM },     { 4, kb_Stat, K_STAT },
    { 5, kb_Chs, K_NEG },       { 5, kb_3, K_3 },           { 5, kb_6, K_6 },
    { 5, kb_9, K_9 },           { 5, kb_RParen, K_RPAREN }, { 5, kb_Tan, K_TAN },
    { 5, kb_Vars, K_VARS },
    { 6, kb_Enter, K_ENTER },   { 6, kb_Add, K_ADD },       { 6, kb_Sub, K_SUB },
    { 6, kb_Mul, K_MUL },       { 6, kb_Div, K_DIV },       { 6, kb_Power, K_POW },
    { 6, kb_Clear, K_CLEAR },
    { 7, kb_Down, K_DOWN },     { 7, kb_Left, K_LEFT },     { 7, kb_Right, K_RIGHT },
    { 7, kb_Up, K_UP },
};

static key_t held = K_NONE;
static clock_t next_repeat;
static unsigned char old_mode, old_int;

/* the keypad scans continuously (keys_begin), so kb_Data is always current */
static key_t scan(void) {
    unsigned i;

    for(i = 0; i < sizeof map / sizeof map[0]; i++)
        if(kb_Data[map[i].group] & map[i].bit)
            return map[i].key;
    return K_NONE;
}

static bool repeats(key_t k) {
    /* return k == K_UP || k == K_DOWN;  // just arrows at first, but clear/del repeating feels better */
    return k == K_UP || k == K_DOWN || k == K_LEFT || k == K_RIGHT || k == K_CLEAR || k == K_DEL;
}

void keys_begin(void) {
    /* [on] is read as "held right now" (Ctrl), not latched. the setting sticks
       around between programs (thanks TI), so set it every time */
    kb_DisableOnLatch();
    kb_ClearOnLatch();

    /* continuous scanning latches KB_DATA_CHANGED on every press and release,
       even while the app is busy drawing. the OS's keypad interrupt would
       clear it first, so that's off until keys_end() */
    old_mode = kb_GetMode();
    old_int = kb_EnableInt;
    kb_EnableInt = 0;
    kb_SetMode(MODE_3_CONTINUOUS);

    /* don't take the key that started the program as the first press */
    while(scan() != K_NONE);
    kb_IntAcknowledge = KB_DATA_CHANGED;
}

void keys_end(void) {
    kb_SetMode(old_mode);
    kb_EnableInt = old_int;
    kb_ClearOnLatch();
}

keypress_t keys_wait(void) {
    static bool on_down, on_used;  /* whether [on] is held, and a key hit with it */
    keypress_t press;

    for(;;) {
        bool changed = (kb_IntStatus & KB_DATA_CHANGED) != 0;
        key_t k;

        kb_IntAcknowledge = KB_DATA_CHANGED;
        k = scan();

        /* [on] isn't in the key matrix. down then up with no key in
           between is a tap of its own, K_ON */
        if(kb_On && !on_down) {
            on_down = true;
            on_used = false;
        } else if(!kb_On && on_down) {
            on_down = false;
            if(!on_used) {
                press.key = K_ON;
                press.on = false;
                return press;
            }
        }

        if(k == K_NONE) {
            held = K_NONE;
            continue;
        }

        /* same key still down but the keypad changed, so it got let go and pressed
           again while we were busy drawing. a new press, not a hold (this was the
           double tap that kept getting eaten) */
        if(k != held || changed) {
            clock_t pressed = clock();

            held = k;
            next_repeat = pressed + FIRST_REPEAT;

            /* this press's own change (it can land just after the scan) and
               the contacts bouncing would look like another press next time */
            while(clock() - pressed < SETTLE)
                kb_IntAcknowledge = KB_DATA_CHANGED;
            break;
        }

        if(repeats(k) && clock() >= next_repeat) {
            next_repeat = clock() + NEXT_REPEAT;
            break;
        }
    }

    on_used |= on_down;
    press.key = held;
    press.on = kb_On;
    return press;
}

keypress_t keys_peek(void) {
    keypress_t press;

    /* held from here on, so keys_wait() dosen't take it as a new press after */
    press.key = held = scan();
    press.on = kb_On;
    return press;
}

#else
typedef int keys_ti_unused; /* nothing here on the PC */
#endif
