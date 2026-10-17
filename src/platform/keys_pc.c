#ifndef __TICE__

/*
    PC version of keys.h: the SDL window's keyboard, mapped to TI keys.

      typing        digits, letters, + - * / ^ ( ) , .
      ~             [(-)], negate    =         [2nd][math]
      numpad        digits . + - * / enter, with NumLock on or off
      Backspace     [clear]          Delete    [del]
      Enter         [enter]          arrows    arrows
      Shift alone   [alpha]          Alt alone [2nd]
      Shift+letter  a capital (same as [2nd][alpha] + letter)
      Shift+arrows  [alpha] + arrow, selects
      Alt+key       [2nd], then the key
      Ctrl alone    [on]             Ctrl+key  the key, with [on] held
      Tab           [2nd]
      F1-F5         the soft keys    Esc       [2nd][mode], quit

    Shift, Alt and Ctrl count as "alone" when they go down and up with no other
    key or character in between; they send their key on the release. Shift with
    another key just types the shifted character (Shift+= is +, Shift+9 is ().
*/

#include "keys.h"
#include "window_pc.h"

#include <string.h>
#include <SDL3/SDL.h>

void keys_begin(void) {}
void keys_end(void) {}

/* scripted presses instead of the keyboard (for tests), see keys_script() */
static char *const *script;
static int script_len, script_pos;

void keys_script(char *const *presses, int count) {
    script = presses;
    script_len = count;
    script_pos = 0;
}

static key_t from_text(char c);

/* one scripted press - a key name, or a single char to type. "on+" "2nd+" "alpha+" hold that one down for it */
static keypress_t scripted(void) {
    static const struct { const char *name; key_t key; } names[] = {
        { "enter", K_ENTER }, { "clear", K_CLEAR }, { "del", K_DEL },
        { "left", K_LEFT }, { "right", K_RIGHT }, { "up", K_UP }, { "down", K_DOWN },
        { "2nd", K_2ND }, { "alpha", K_ALPHA }, { "mode", K_MODE },
        { "y=", K_YEQU }, { "window", K_WINDOW }, { "zoom", K_ZOOM }, { "trace", K_TRACE }, { "graph", K_GRAPH },
        { "x^2", K_SQUARE }, { "x^-1", K_RECIP }, { "sin", K_SIN }, { "cos", K_COS }, { "tan", K_TAN },
        { "ln", K_LN }, { "log", K_LOG }, { "neg", K_NEG }, { "xt", K_XT }, { "sto", K_STO }, { "math", K_MATH }, { "vars", K_VARS },
        { "on", K_ON }, { "apps", K_APPS }, { "prgm", K_PRGM }, { "stat", K_STAT },
    };
    keypress_t press = { K_QUIT, false };
    const char *s;
    unsigned i;

    if(script_pos >= script_len)
        return press;    /* script's done, quit */

    s = script[script_pos++];
    /* held ones, any order. alpha+ is how the web demo holds alpha over a whole word */
    for(;;) {
        if(strncmp(s, "on+", 3) == 0) {
            press.on = true;
            s += 3;
        } else if(strncmp(s, "2nd+", 4) == 0) {
            press.second = true;
            s += 4;
        } else if(strncmp(s, "alpha+", 6) == 0) {
            press.alpha = true;
            s += 6;
        } else {
            break;
        }
    }
    for(i = 0; i < sizeof names / sizeof names[0]; i++) {
        if(strcmp(s, names[i].name) == 0) {
            press.key = names[i].key;
            return press;
        }
    }
    press.key = s[0] != '\0' && s[1] == '\0' ? from_text(s[0]) : K_NONE;
    return press;
}

static key_t from_text(char c) {
    if(c >= '0' && c <= '9')
        return (key_t)(K_0 + (c - '0'));
    if(c >= 'a' && c <= 'z')
        return (key_t)(K_LETTER_A + (c - 'a'));
    if(c >= 'A' && c <= 'Z')
        return (key_t)(K_CAPITAL_A + (c - 'A'));

    switch(c) {
    case '+': return K_ADD;
    case '-': return K_SUB;
    case '*': return K_MUL;
    case '/': return K_DIV;
    case '^': return K_POW;
    case '(': return K_LPAREN;
    case ')': return K_RPAREN;
    case ',': return K_COMMA;
    case '.': return K_DOT;
    case '~': return K_NEG;
    default:  return K_NONE;
    }
}

static key_t from_key(SDL_Keycode k) {
    switch(k) {
    case SDLK_BACKSPACE: return K_CLEAR;
    case SDLK_DELETE:    return K_DEL;
    case SDLK_RETURN:
    case SDLK_KP_ENTER:  return K_ENTER;
    case SDLK_LEFT:      return K_LEFT;
    case SDLK_RIGHT:     return K_RIGHT;
    case SDLK_UP:        return K_UP;
    case SDLK_DOWN:      return K_DOWN;
    case SDLK_TAB:       return K_2ND;
    case SDLK_F1:        return K_YEQU;
    case SDLK_F2:        return K_WINDOW;
    case SDLK_F3:        return K_ZOOM;
    case SDLK_F4:        return K_TRACE;
    case SDLK_F5:        return K_GRAPH;
    default:             return K_NONE;
    }
}

/* numpad keys, by where they are (the scancode), so NumLock doesn't matter */
static key_t from_keypad(SDL_Scancode sc, SDL_Keycode k) {
    switch(sc) {
    case SDL_SCANCODE_KP_0:        return K_0;
    case SDL_SCANCODE_KP_1:        return K_1;
    case SDL_SCANCODE_KP_2:        return K_2;
    case SDL_SCANCODE_KP_3:        return K_3;
    case SDL_SCANCODE_KP_4:        return K_4;
    case SDL_SCANCODE_KP_5:        return K_5;
    case SDL_SCANCODE_KP_6:        return K_6;
    case SDL_SCANCODE_KP_7:        return K_7;
    case SDL_SCANCODE_KP_8:        return K_8;
    case SDL_SCANCODE_KP_9:        return K_9;
    case SDL_SCANCODE_KP_PERIOD:   return K_DOT;
    case SDL_SCANCODE_KP_PLUS:     return K_ADD;
    case SDL_SCANCODE_KP_MINUS:    return K_SUB;
    case SDL_SCANCODE_KP_MULTIPLY: return K_MUL;
    case SDL_SCANCODE_KP_DIVIDE:   return K_DIV;
    case SDL_SCANCODE_KP_ENTER:    return K_ENTER;
    default: break;
    }
    switch(k) {
    case SDLK_KP_0:        return K_0;
    case SDLK_KP_1:        return K_1;
    case SDLK_KP_2:        return K_2;
    case SDLK_KP_3:        return K_3;
    case SDLK_KP_4:        return K_4;
    case SDLK_KP_5:        return K_5;
    case SDLK_KP_6:        return K_6;
    case SDLK_KP_7:        return K_7;
    case SDLK_KP_8:        return K_8;
    case SDLK_KP_9:        return K_9;
    case SDLK_KP_PERIOD:   return K_DOT;
    case SDLK_KP_PLUS:     return K_ADD;
    case SDLK_KP_MINUS:    return K_SUB;
    case SDLK_KP_MULTIPLY: return K_MUL;
    case SDLK_KP_DIVIDE:   return K_DIV;
    case SDLK_KP_ENTER:    return K_ENTER;
    default:               return K_NONE;
    }
}

/* presses still to hand out, one per keys_wait() (Alt+key, =) */
#define PENDING_MAX 4
static keypress_t pending[PENDING_MAX];
static int pending_first, pending_count;

static void push(key_t key, bool on) {
    if(key == K_NONE || pending_count >= PENDING_MAX)
        return;
    pending[(pending_first + pending_count) % PENDING_MAX].key = key;
    pending[(pending_first + pending_count) % PENDING_MAX].on = on;
    pending_count++;
}

/* a typed character: = is [2nd][math], like on the TI */
static void push_char(char c, bool on) {
    if(c == '=') {
        push(K_2ND, on);
        push(K_MATH, on);
    } else {
        push(from_text(c), on);
    }
}

static bool shift_alone;    /* Shift down, nothing else since */
static bool alt_alone;      /* Alt down, nothing else since */
static bool ctrl_alone;     /* Ctrl down, nothing else since */
static bool skip_text;      /* keystroke already handled at key-down, so drop its text */

static bool is_shift(SDL_Keycode k) { return k == SDLK_LSHIFT || k == SDLK_RSHIFT; }
static bool is_alt(SDL_Keycode k)   { return k == SDLK_LALT || k == SDLK_RALT; }
static bool is_ctrl(SDL_Keycode k)  { return k == SDLK_LCTRL || k == SDLK_RCTRL; }

static bool is_modifier(SDL_Keycode k) {
    return is_shift(k) || is_alt(k) || is_ctrl(k)
        || k == SDLK_LGUI || k == SDLK_RGUI || k == SDLK_MODE
        || k == SDLK_CAPSLOCK || k == SDLK_NUMLOCKCLEAR;
}

/* a key went down, queue its presses if it has any */
static void key_down(const SDL_KeyboardEvent *ev) {
    bool on = (ev->mod & SDL_KMOD_CTRL) != 0;
    bool alt = (ev->mod & SDL_KMOD_ALT) != 0;
    bool shift = (ev->mod & SDL_KMOD_SHIFT) != 0;
    key_t key;

    /* new keystroke, so whatever text comes next belongs to it */
    skip_text = false;

    if(ev->repeat && (is_shift(ev->key) || is_alt(ev->key) || is_ctrl(ev->key)))
        return;  /* still the same tap */

    shift_alone = is_shift(ev->key);
    alt_alone = is_alt(ev->key);
    ctrl_alone = is_ctrl(ev->key);
    if(is_modifier(ev->key))
        return;     /* nothing's sent until it's released */

    if(ev->key == SDLK_ESCAPE) {
        push(K_QUIT, on);
        return;
    }

    key = from_keypad(ev->scancode, ev->key);
    if(key != K_NONE) {
        skip_text = true;   /* NumLock on, the digit comes through as text too */
        if(alt)
            push(K_2ND, on);
        push(key, on);
        return;
    }

    key = from_key(ev->key);
    if(key != K_NONE) {
        if(alt)
            push(K_2ND, on);
        if(shift && (key == K_LEFT || key == K_RIGHT || key == K_UP || key == K_DOWN))
            push(K_ALPHA, on);  /* [alpha] + arrow selects */
        push(key, on);
        return;
    }

    /* Alt + a typed key, grab its character here and not its text */
    /* character? i hardly know 'er bwahahahwaa */
    if(alt) {
        skip_text = true;
        if(ev->key == '=') {
            push_char('=', on);     /* that's already [2nd][math] */
        } else if(ev->key < 0x80 && from_text((char)ev->key) != K_NONE) {
            push(K_2ND, on);
            push(from_text((char)ev->key), on);
        }
    }
}

keypress_t keys_peek(void) {
    keypress_t press = { K_NONE, false };
    const bool *down;
    int sc;

    if(script != NULL)
        return press;

    SDL_PumpEvents();
    down = SDL_GetKeyboardState(NULL);
    press.on = (SDL_GetModState() & SDL_KMOD_CTRL) != 0;

    /* only the keys that aren't typed text (enter, arrows, F1-F5, ...) */
    for(sc = 0; sc < SDL_SCANCODE_COUNT && press.key == K_NONE; sc++) {
        if(down[sc])
            press.key = from_key(SDL_GetKeyFromScancode((SDL_Scancode)sc, SDL_KMOD_NONE, false));
    }
    /* Esc is [mode] here (it quits from keys_wait()): Ctrl+Esc = [on]+[mode], cancel */
    if(press.key == K_NONE && down[SDL_SCANCODE_ESCAPE])
        press.key = K_MODE;

    /* its key-down is queued too, don't let keys_wait() see it as a new press */
    if(press.key != K_NONE || press.on) {
        SDL_FlushEvents(SDL_EVENT_KEY_DOWN, SDL_EVENT_TEXT_INPUT);
        shift_alone = alt_alone = ctrl_alone = false;  /* their key-ups might be gone */
    }
    return press;
}

/* same shape as keys_ti.c. there's no backlight here but the app's hook still
   runs, so the sleep logic gets exercised on the PC too */
void (*keys_on_sleep)(bool asleep) = NULL;
static bool asleep;

static void wake(void) {
    asleep = false;
    if(keys_on_sleep != NULL)
        keys_on_sleep(false);
}

void keys_sleep(void) {
    if(!asleep && keys_on_sleep != NULL) {
        asleep = true;
        keys_on_sleep(true);
    }
}

keypress_t keys_wait(void) {
    keypress_t press = { K_NONE, false };

    if(script != NULL)
        return scripted();

    while(pending_count == 0) {
        SDL_Event ev;

        /* a timeout while awake is the idle clock running out */
        if(!SDL_WaitEventTimeout(&ev, asleep ? -1 : (int)(KEYS_IDLE_OFF / (CLOCKS_PER_SEC / 1000)))) {
            if(asleep) {
                push(K_QUIT, false);
                break;
            }
            keys_sleep();
            continue;
        }
        if(asleep && ev.type == SDL_EVENT_KEY_DOWN) {
            wake();     /* that key only turns the screen back on */
            continue;
        }

        switch(ev.type) {
        case SDL_EVENT_QUIT:
            push(K_QUIT, false);
            break;

        case SDL_EVENT_WINDOW_EXPOSED:
            window_present();
            break;

        case SDL_EVENT_WINDOW_FOCUS_LOST:
            /* like Alt+Tab to another window, that Alt wasn't meant for us */
            shift_alone = false;
            alt_alone = false;
            ctrl_alone = false;
            break;

        case SDL_EVENT_KEY_DOWN:
            key_down(&ev.key);
            break;

        case SDL_EVENT_KEY_UP:
            if(is_shift(ev.key.key) && shift_alone) {
                shift_alone = false;
                push(K_ALPHA, (ev.key.mod & SDL_KMOD_CTRL) != 0);
            } else if(is_alt(ev.key.key) && alt_alone) {
                alt_alone = false;
                push(K_2ND, (ev.key.mod & SDL_KMOD_CTRL) != 0);
            } else if(is_ctrl(ev.key.key) && ctrl_alone) {
                ctrl_alone = false;
                push(K_ON, false);
            }
            break;

        case SDL_EVENT_TEXT_INPUT:
            /* printable characters come here (with shift and layout applied) */
            shift_alone = false;
            alt_alone = false;
            ctrl_alone = false;
            if(skip_text)
                break;
            push_char(ev.text.text[0], (SDL_GetModState() & SDL_KMOD_CTRL) != 0);
            break;

        default:
            break;
        }
    }

    press = pending[pending_first];
    pending_first = (pending_first + 1) % PENDING_MAX;
    pending_count--;
    return press;
}

#else
typedef int keys_pc_unused; /* nothing here on the calculator */
#endif
