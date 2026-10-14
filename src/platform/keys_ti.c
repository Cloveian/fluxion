#ifdef __TICE__

/* calculator side of keys.h. scans the keypad with keypadc, a new key fires
   once; arrows, clear and del repeat while held (like the TI's own repeat). */

#include "keys.h"

#include <keypadc.h>
#include <time.h>

#define FIRST_REPEAT (CLOCKS_PER_SEC * 2 / 5)   /* 400 ms before repeating */
#define NEXT_REPEAT  (CLOCKS_PER_SEC / 16)      /* then every ~60 ms */

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

static key_t scan(void) {
    unsigned i;

    kb_Scan();
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

    /* don't take the key that started the program as the first press */
    while(scan() != K_NONE);
}

void keys_end(void) {
    kb_ClearOnLatch();
}

keypress_t keys_wait(void) {
    keypress_t press;

    for(;;) {
        key_t k = scan();

        if(k == K_NONE) {
            held = K_NONE;
            continue;
        }

        if(k != held) {
            held = k;
            next_repeat = clock() + FIRST_REPEAT;
            break;
        }

        if(repeats(k) && clock() >= next_repeat) {
            next_repeat = clock() + NEXT_REPEAT;
            break;
        }
    }

    press.key = held;
    press.on = kb_On;
    return press;
}

keypress_t keys_peek(void) {
    keypress_t press;

    /* held from here on, so keys_wait() doesn't take it as a new press after */
    press.key = held = scan();
    press.on = kb_On;
    return press;
}

#else
typedef int keys_ti_unused; /* nothing here on the PC */
#endif
