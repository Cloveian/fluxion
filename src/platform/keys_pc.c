#ifndef __TICE__

/*
    PC version of keys.h: the SDL window's keyboard, mapped to TI keys.

      typing        digits, letters, + - * / ^ ( ) , .
      ~             [(-)], negate    =         [2nd][math]
      numpad        digits . + - * / enter, with NumLock on or off
      Backspace     [clear]          Delete    [del]
      Home / End    start / end of the line
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
#include "clipboard.h"
#include "screen.h"
#include "window_pc.h"

#include <stdlib.h>
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

/*
    tokens that aren't presses, these get looked at mid-run too (keys_peek):
      wait:6        the next press waits 6 s
      png:out.png   the screen right then, at 2x
      clip:3x+2     puts that on the clipboard, for a paste to find
      click:x,y     mouse click at a framebuffer pixel (alpha+click: selects), drag:x,y is the button held
      busy:on+mode  held down while the engine runs, skipped if it's done by then
*/
static Uint64 wait_until; /* SDL ms, clock() is CPU time and stands still in a wait */

/* true while a wait: is still going. png: ones get saved on the way past */
static bool script_hold(void) {
    while(script_pos < script_len) {
        const char *s = script[script_pos];

        if(strncmp(s, "wait:", 5) == 0) {
            if(wait_until == 0)
                wait_until = SDL_GetTicks() + (Uint64)atoi(s + 5) * 1000;
            if(SDL_GetTicks() < wait_until)
                return true;
            wait_until = 0;
        } else if(strncmp(s, "png:", 4) == 0) {
            screen_save_png(s + 4, 2);
        } else if(strncmp(s, "clip:", 5) == 0) {
            clipboard_put(s + 5);
        } else {
            return false;
        }
        script_pos++;
    }
    return false;
}

/* a key name, or a single char to type. "on+" "2nd+" "alpha+" hold that one down for it */
static keypress_t parse_press(const char *s) {
    keypress_t press = { K_NONE, 0 };

    /* held ones, any order. alpha+ is how the web demo holds alpha over a whole word */
    for(;;) {
        if(strncmp(s, "on+", 3) == 0) {
            press.held |= M_ON;
            s += 3;
        } else if(strncmp(s, "2nd+", 4) == 0) {
            press.held |= M_2ND;
            s += 4;
        } else if(strncmp(s, "alpha+", 6) == 0) {
            press.held |= M_ALPHA;
            s += 6;
        } else {
            break;
        }
    }
    if(strncmp(s, "click:", 6) == 0 || strncmp(s, "drag:", 5) == 0) { /* click:120,200  drag:200,200, with alpha+ it's a shift-click */
        const char *comma = strchr(s, ',');

        press.key = s[0] == 'c' ? K_POINT_DOWN : K_POINT_DRAG;
        press.x = (short)atoi(strchr(s, ':') + 1);
        press.y = comma != NULL ? (short)atoi(comma + 1) : 0;
        return press;
    }
    press.key = keys_named(s);
    return press;
}

/* the next press for keys_wait, K_QUIT when the script's done */
static keypress_t scripted(void) {
    keypress_t quit = { K_QUIT, 0 };

    for(;;) {
        while(script_hold())
            SDL_Delay(10);
        if(script_pos >= script_len)
            return quit;
        if(strncmp(script[script_pos], "busy:", 5) != 0)
            return parse_press(script[script_pos++]);
        script_pos++;
    }
}

static keycode_t from_key(SDL_Keycode k) {
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
    case SDLK_HOME:      return K_HOME;
    case SDLK_END:       return K_END;
    case SDLK_F1:        return K_YEQU;
    case SDLK_F2:        return K_WINDOW;
    case SDLK_F3:        return K_ZOOM;
    case SDLK_F4:        return K_TRACE;
    case SDLK_F5:        return K_GRAPH;
    default:             return K_NONE;
    }
}

/* numpad keys, by where they are (the scancode), so NumLock doesn't matter */
static keycode_t from_keypad(SDL_Scancode sc, SDL_Keycode k) {
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

static keypress_t *push(keycode_t key, bool on) {
    keypress_t *slot;

    if(key == K_NONE || pending_count >= PENDING_MAX)
        return NULL;
    slot = &pending[(pending_first + pending_count) % PENDING_MAX];
    slot->key = key;
    slot->held = on ? M_ON : 0;
    slot->x = slot->y = 0;
    pending_count++;
    return slot;
}

/* the mouse at a window position, shift held is a selecting click. a drag right after a drag replaces it, only where it ended counts */
static void push_pointer(keycode_t key, float wx, float wy) {
    keypress_t *slot;
    int x, y;

    if(key == K_POINT_DRAG && pending_count > 0 && pending[(pending_first + pending_count - 1) % PENDING_MAX].key == K_POINT_DRAG)
        pending_count--;
    slot = push(key, false);
    if(slot == NULL)
        return;
    window_point(wx, wy, &x, &y);
    slot->x = (short)x;
    slot->y = (short)y;
    slot->held = (SDL_GetModState() & SDL_KMOD_SHIFT) != 0 ? M_ALPHA : 0;
}

/* a typed character: = is [2nd][math], like on the TI */
static void push_char(char c, bool on) {
    if(c == '=') {
        push(K_2ND, on);
        push(K_MATH, on);
    } else {
        push(keys_from_char(c), on);
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
    bool bigger, smaller;
    keycode_t key;

    if(ev->key == SDLK_F11) { /* fullscreen, not a calculator key */
        window_fullscreen_toggle();
        return;
    }
    bigger = ev->key == SDLK_EQUALS || ev->key == SDLK_PLUS || ev->key == SDLK_KP_PLUS;
    smaller = ev->key == SDLK_MINUS || ev->key == SDLK_KP_MINUS;
    if(on && (bigger || smaller)) {
        window_zoom(bigger ? 1 : -1); /* the pixel size */
        push(K_REDRAW, false);
        return;
    }

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

    /* Ctrl + C X V Z A, no text comes with those */
    if(on && ev->key > 0 && ev->key < 0x80 && strchr("cxvza", (int)ev->key) != NULL) {
        skip_text = true;
        push(keys_from_char((char)ev->key), true);
        return;
    }

    /* Alt + a typed key, grab its character here and not its text */
    /* character? i hardly know 'er bwahahahwaa */
    if(alt) {
        skip_text = true;
        if(ev->key == '=') {
            push_char('=', on);     /* that's already [2nd][math] */
        } else if(ev->key < 0x80 && keys_from_char((char)ev->key) != K_NONE) {
            push(K_2ND, on);
            push(keys_from_char((char)ev->key), on);
        }
    }
}

keypress_t keys_peek(void) {
    keypress_t press = { K_NONE, 0 };
    const bool *down;
    int sc;

    if(script != NULL) {
        if(!script_hold() && script_pos < script_len && strncmp(script[script_pos], "busy:", 5) == 0)
            return parse_press(script[script_pos++] + 5);
        return press;
    }

    SDL_PumpEvents();
    down = SDL_GetKeyboardState(NULL);
    press.held = (SDL_GetModState() & SDL_KMOD_CTRL) ? M_ON : 0;

    /* only the keys that aren't typed text (enter, arrows, F1-F5, ...) */
    for(sc = 0; sc < SDL_SCANCODE_COUNT && press.key == K_NONE; sc++) {
        if(down[sc])
            press.key = from_key(SDL_GetKeyFromScancode((SDL_Scancode)sc, SDL_KMOD_NONE, false));
    }
    /* Esc is [mode] here (it quits from keys_wait()): Ctrl+Esc = [on]+[mode], cancel */
    if(press.key == K_NONE && down[SDL_SCANCODE_ESCAPE])
        press.key = K_MODE;

    /* its key-down is queued too, don't let keys_wait() see it as a new press */
    if(press.key != K_NONE || (press.held & M_ON)) {
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
    keypress_t press = { K_NONE, 0 };

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

        case SDL_EVENT_WINDOW_RESIZED:
            window_sync();
            push(K_REDRAW, false);
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

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if(ev.button.button == SDL_BUTTON_LEFT)
                push_pointer(K_POINT_DOWN, ev.button.x, ev.button.y);
            break;

        case SDL_EVENT_MOUSE_MOTION:
            if((ev.motion.state & SDL_BUTTON_LMASK) != 0)
                push_pointer(K_POINT_DRAG, ev.motion.x, ev.motion.y);
            break;

        case SDL_EVENT_MOUSE_WHEEL:
            if(ev.wheel.y != 0)
                push(ev.wheel.y > 0 ? K_UP : K_DOWN, false); /* scrolls the history like the arrows */
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
