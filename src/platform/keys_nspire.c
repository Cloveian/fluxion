#ifdef _TINSPIRE

/*
    nspire side of keys.h, its own keypad. what they do is input/bind_nspire.c.
    polled every 10 ms, arrows and del repeat while held.
*/

#include "keys.h"

#include <libndls.h>

#define POLL_MS      10
#define FIRST_REPEAT 25     /* held 250 ms before it repeats */
#define NEXT_REPEAT  3      /* then ~33 a second */
#define IDLE_POLLS   ((long)KEYS_IDLE_OFF / CLOCKS_PER_SEC * (1000 / POLL_MS))

typedef struct {
    const t_key *nk;
    keycode_t key;
} map_t;

static const map_t map[] = {
    { &KEY_NSPIRE_0, K_0 }, { &KEY_NSPIRE_1, K_1 }, { &KEY_NSPIRE_2, K_2 }, { &KEY_NSPIRE_3, K_3 },
    { &KEY_NSPIRE_4, K_4 }, { &KEY_NSPIRE_5, K_5 }, { &KEY_NSPIRE_6, K_6 }, { &KEY_NSPIRE_7, K_7 },
    { &KEY_NSPIRE_8, K_8 }, { &KEY_NSPIRE_9, K_9 },
    { &KEY_NSPIRE_PERIOD, K_DOT },      { &KEY_NSPIRE_NEGATIVE, K_NEG },
    { &KEY_NSPIRE_ENTER, K_ENTER },     { &KEY_NSPIRE_RET, K_RETURN },
    { &KEY_NSPIRE_PLUS, K_ADD },        { &KEY_NSPIRE_MINUS, K_SUB },
    { &KEY_NSPIRE_MULTIPLY, K_MUL },    { &KEY_NSPIRE_DIVIDE, K_DIV },
    { &KEY_NSPIRE_EXP, K_POW },         { &KEY_NSPIRE_SQU, K_SQUARE },
    { &KEY_NSPIRE_eEXP, K_EX },         { &KEY_NSPIRE_TENX, K_TENX },
    { &KEY_NSPIRE_LP, K_LPAREN },       { &KEY_NSPIRE_RP, K_RPAREN },
    { &KEY_NSPIRE_COMMA, K_COMMA },     { &KEY_NSPIRE_EQU, K_EQUALS },
    { &KEY_NSPIRE_SIN, K_SIN },         { &KEY_NSPIRE_COS, K_COS },
    { &KEY_NSPIRE_TAN, K_TAN },         { &KEY_NSPIRE_TRIG, K_TRIG },
    { &KEY_NSPIRE_PI, K_PI },           { &KEY_NSPIRE_EE, K_EE },
    { &KEY_NSPIRE_QUESEXCL, K_MARKS },  { &KEY_NSPIRE_FRAC, K_TEMPLATE },
    { &KEY_NSPIRE_CAT, K_CAT },         { &KEY_NSPIRE_VAR, K_VARS },
    { &KEY_NSPIRE_DEL, K_DEL },         { &KEY_NSPIRE_ESC, K_ESC },
    { &KEY_NSPIRE_MENU, K_MENU },       { &KEY_NSPIRE_DOC, K_DOC },
    { &KEY_NSPIRE_HOME, K_HOME },
};

/* the touchpad. one switch under the whole pad, ndless picks the arrow off
   where the finger is when it's pushed */
static const map_t pad[] = {
    { &KEY_NSPIRE_UP, K_UP }, { &KEY_NSPIRE_DOWN, K_DOWN },
    { &KEY_NSPIRE_LEFT, K_LEFT }, { &KEY_NSPIRE_RIGHT, K_RIGHT },
    { &KEY_NSPIRE_CLICK, K_CLICK },
};

static const t_key *const letters[26] = {
    &KEY_NSPIRE_A, &KEY_NSPIRE_B, &KEY_NSPIRE_C, &KEY_NSPIRE_D, &KEY_NSPIRE_E, &KEY_NSPIRE_F,
    &KEY_NSPIRE_G, &KEY_NSPIRE_H, &KEY_NSPIRE_I, &KEY_NSPIRE_J, &KEY_NSPIRE_K, &KEY_NSPIRE_L,
    &KEY_NSPIRE_M, &KEY_NSPIRE_N, &KEY_NSPIRE_O, &KEY_NSPIRE_P, &KEY_NSPIRE_Q, &KEY_NSPIRE_R,
    &KEY_NSPIRE_S, &KEY_NSPIRE_T, &KEY_NSPIRE_U, &KEY_NSPIRE_V, &KEY_NSPIRE_W, &KEY_NSPIRE_X,
    &KEY_NSPIRE_Y, &KEY_NSPIRE_Z
};

void keys_begin(void) {}
void keys_end(void) {}

static keycode_t down = K_NONE;     /* down at the last poll, for repeat */

/* pushed anywhere, the corners too */
static bool pad_pushed(void) {
    return isKeyPressed(KEY_NSPIRE_UP) || isKeyPressed(KEY_NSPIRE_UPRIGHT) || isKeyPressed(KEY_NSPIRE_RIGHT)
        || isKeyPressed(KEY_NSPIRE_RIGHTDOWN) || isKeyPressed(KEY_NSPIRE_DOWN) || isKeyPressed(KEY_NSPIRE_DOWNLEFT)
        || isKeyPressed(KEY_NSPIRE_LEFT) || isKeyPressed(KEY_NSPIRE_LEFTUP) || isKeyPressed(KEY_NSPIRE_CLICK);
}

static bool on_pad(keycode_t k) {
    unsigned i;

    for(i = 0; i < sizeof pad / sizeof pad[0]; i++)
        if(pad[i].key == k)
            return true;
    return false;
}

/* the key down right now, first one found. a push on the touchpad stays the
   arrow it started as till it's let go, a finger slideing on it doesn't turn it */
static keycode_t scan(void) {
    unsigned i;

    if(!any_key_pressed())
        return K_NONE;
    if(on_pad(down) && pad_pushed())
        return down;
    for(i = 0; i < 26; i++)
        if(isKeyPressed(*letters[i]))
            return (keycode_t)(K_LETTER_A + i);
    for(i = 0; i < sizeof map / sizeof map[0]; i++)
        if(isKeyPressed(*map[i].nk))
            return map[i].key;
    for(i = 0; i < sizeof pad / sizeof pad[0]; i++)
        if(isKeyPressed(*pad[i].nk))
            return pad[i].key;
    return K_NONE;
}

static bool repeats(keycode_t k) {
    return k == K_UP || k == K_DOWN || k == K_LEFT || k == K_RIGHT || k == K_DEL;
}
static int repeat_in;   /* polls till it fires again */

/* going dark, see keys.h */
void (*keys_on_sleep)(bool asleep) = NULL;
static bool asleep;
static long idle_polls;

static void wake(void) {
    asleep = false;
    idle_polls = 0;
    if(keys_on_sleep != NULL)
        keys_on_sleep(false);
}

void keys_sleep(void) {
    if(!asleep && keys_on_sleep != NULL) {
        asleep = true;
        keys_on_sleep(true);
    }
}

/* ctrl and shift, read on their own. a tap with nothing pressed in between is
   K_CTRL / K_SHIFT, the app makes those sticky */
typedef struct {
    bool down, used;
} mod_t;

static mod_t mods[2];
static const keycode_t mod_keys[2] = { K_CTRL, K_SHIFT };
static const unsigned mod_bits[2] = { M_CTRL, M_SHIFT };

static unsigned held_now(void) {
    return (isKeyPressed(KEY_NSPIRE_CTRL) ? M_CTRL : 0) | (isKeyPressed(KEY_NSPIRE_SHIFT) ? M_SHIFT : 0);
}

keypress_t keys_wait(void) {
    keypress_t press;
    unsigned now;
    keycode_t k;
    int i;

    idle_polls = 0;   /* idle means waiting here, a long solve isn't */
    for(;;) {
        msleep(POLL_MS);
        if(!asleep && ++idle_polls > IDLE_POLLS)
            keys_sleep();

        now = held_now();
        for(i = 0; i < 2; i++) {
            bool is = (now & mod_bits[i]) != 0;

            if(is == mods[i].down)
                continue;
            mods[i].down = is;
            if(is) {
                mods[i].used = false;
            } else if(!mods[i].used) {
                if(asleep) {    /* the tap that wakes it doesn't count */
                    wake();
                    continue;
                }
                press.key = mod_keys[i];
                press.held = 0;
                return press;
            }
        }

        k = scan();
        if(k == K_NONE) {
            down = K_NONE;
            continue;
        }
        if(k != down) {
            down = k;
            repeat_in = FIRST_REPEAT;
            if(asleep) {    /* eaten, it only woke the screen */
                wake();
                continue;
            }
            break;
        }
        if(repeats(k) && --repeat_in <= 0) {
            repeat_in = NEXT_REPEAT;
            break;
        }
    }

    for(i = 0; i < 2; i++)
        mods[i].used |= mods[i].down;
    press.key = down;
    press.held = now;
    return press;
}

keypress_t keys_peek(void) {
    keypress_t press;

    /* down from here on, so keys_wait() doesn't take it as a new press after */
    press.key = down = scan();
    press.held = held_now();
    return press;
}

#else
typedef int keys_nspire_unused; /* nothing here off the nspire */
#endif
