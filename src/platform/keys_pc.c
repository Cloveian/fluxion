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

#include "keys_pc.h"
#include "events_pc.h"

#include <string.h>

void keys_begin(void) {}
void keys_end(void) {}

static keycode_t from_key(int k) {
    switch(k) {
    case PK_BACKSPACE: return K_CLEAR;
    case PK_DELETE:    return K_DEL;
    case PK_ENTER:
    case PK_KP_ENTER:  return K_ENTER;
    case PK_LEFT:      return K_LEFT;
    case PK_RIGHT:     return K_RIGHT;
    case PK_UP:        return K_UP;
    case PK_DOWN:      return K_DOWN;
    case PK_TAB:       return K_2ND;
    case PK_HOME:      return K_HOME;
    case PK_END:       return K_END;
    case PK_F1:        return K_YEQU;
    case PK_F2:        return K_WINDOW;
    case PK_F3:        return K_ZOOM;
    case PK_F4:        return K_TRACE;
    case PK_F5:        return K_GRAPH;
    default:           return K_NONE;
    }
}

static keycode_t from_keypad(int k) {
    switch(k) {
    case PK_KP_DOT:   return K_DOT;
    case PK_KP_PLUS:  return K_ADD;
    case PK_KP_MINUS: return K_SUB;
    case PK_KP_MUL:   return K_MUL;
    case PK_KP_DIV:   return K_DIV;
    case PK_KP_ENTER: return K_ENTER;
    default:          return k >= PK_KP_0 && k <= PK_KP_9 ? (keycode_t)(K_0 + (k - PK_KP_0)) : K_NONE;
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

/* the mouse, shift held is a selecting click. a drag right after a drag repalces it, only where it ended counts, same for a move after a move */
static void push_pointer(keycode_t key, int x, int y, unsigned mods) {
    keypress_t *slot;

    if((key == K_POINT_DRAG || key == K_POINT_MOVE) && pending_count > 0 && pending[(pending_first + pending_count - 1) % PENDING_MAX].key == key)
        pending_count--;
    slot = push(key, false);
    if(slot == NULL)
        return;
    slot->x = (short)x;
    slot->y = (short)y;
    slot->held = (mods & PM_SHIFT) != 0 ? M_ALPHA : 0;
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

static bool is_modifier(int k) {
    return k == PK_SHIFT || k == PK_ALT || k == PK_CTRL || k == PK_MOD_OTHER;
}

/* a key went down, queue its presses if it has any */
static void key_down(const pc_event_t *ev) {
    bool on = (ev->mods & PM_CTRL) != 0;
    bool alt = (ev->mods & PM_ALT) != 0;
    bool shift = (ev->mods & PM_SHIFT) != 0;
    keycode_t key;

    /* new keystroke, so whatever text comes next belongs to it */
    skip_text = false;

    if(ev->repeat && (ev->key == PK_SHIFT || ev->key == PK_ALT || ev->key == PK_CTRL))
        return;  /* still the same tap */

    shift_alone = ev->key == PK_SHIFT;
    alt_alone = ev->key == PK_ALT;
    ctrl_alone = ev->key == PK_CTRL;
    if(is_modifier(ev->key))
        return;     /* nothing's sent until it's released */

    if(ev->key == PK_ESCAPE) {
        push(K_QUIT, on);
        return;
    }

    key = from_keypad(ev->key);
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

#define HELD_MAX 24 /* keys down at once that peek looks through */

keypress_t keys_peek(void) {
    keypress_t press = { K_NONE, 0 };
    int down[HELD_MAX];
    int i, n;
    bool esc = false;

    if(pc_script_on())
        return pc_script_peek();

    n = pc_keys_held(down, HELD_MAX);
    press.held = (pc_mods_now() & PM_CTRL) ? M_ON : 0;

    /* only the keys that aren't typed text (enter, arrows, F1-F5, ...) */
    for(i = 0; i < n && press.key == K_NONE; i++) {
        press.key = from_key(down[i]);
        esc = esc || down[i] == PK_ESCAPE;
    }
    /* Esc is [mode] here (it quits from keys_wait()): Ctrl+Esc = [on]+[mode], cancel */
    if(press.key == K_NONE && esc)
        press.key = K_MODE;

    /* its key-down is queued too, don't let keys_wait() see it as a new press */
    if(press.key != K_NONE || (press.held & M_ON)) {
        pc_keys_flush();
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

    if(pc_script_on())
        return pc_script_wait();

    while(pending_count == 0) {
        pc_event_t ev;

        /* a timeout while awake is the idle clock running out */
        if(!pc_event_wait(&ev, asleep ? -1 : (int)(KEYS_IDLE_OFF / (CLOCKS_PER_SEC / 1000)))) {
            if(asleep) {
                push(K_QUIT, false);
                break;
            }
            keys_sleep();
            continue;
        }
        if(asleep && ev.kind == EV_KEY_DOWN) {
            wake();     /* that key only turns the screen back on */
            continue;
        }

        switch(ev.kind) {
        case EV_QUIT:
            push(K_QUIT, false);
            break;

        case EV_REDRAW:
            push(K_REDRAW, false);
            break;

        case EV_FOCUS_LOST:
            /* like Alt+Tab to another window, that Alt wasn't meant for us */
            shift_alone = false;
            alt_alone = false;
            ctrl_alone = false;
            break;

        case EV_KEY_DOWN: {
            pc_window_t taken = pc_window_key(&ev); /* fullscreen and zoom aren't calculator keys */

            if(taken == PW_REDRAW)
                push(K_REDRAW, false);
            else if(taken == PW_NOT_MINE)
                key_down(&ev);
            break;
        }

        case EV_MOUSE_DOWN:
            push_pointer(K_POINT_DOWN, ev.x, ev.y, ev.mods);
            break;

        case EV_MOUSE_DRAG:
            push_pointer(K_POINT_DRAG, ev.x, ev.y, ev.mods);
            break;

        case EV_MOUSE_MOVE:
            push_pointer(K_POINT_MOVE, ev.x, ev.y, ev.mods);
            break;

        case EV_MOUSE_LEAVE: {
            keypress_t *left = push(K_POINT_MOVE, false);

            if(left != NULL)
                left->x = left->y = -1;
            break;
        }

        case EV_WHEEL:
            push(ev.wheel > 0 ? K_UP : K_DOWN, false); /* scrolls the history like the arrows */
            break;

        case EV_KEY_UP:
            if(ev.key == PK_SHIFT && shift_alone) {
                shift_alone = false;
                push(K_ALPHA, (ev.mods & PM_CTRL) != 0);
            } else if(ev.key == PK_ALT && alt_alone) {
                alt_alone = false;
                push(K_2ND, (ev.mods & PM_CTRL) != 0);
            } else if(ev.key == PK_CTRL && ctrl_alone) {
                ctrl_alone = false;
                push(K_ON, false);
            }
            break;

        case EV_TEXT:
            /* printable characters come here (with shift and layout applied) */
            shift_alone = false;
            alt_alone = false;
            ctrl_alone = false;
            if(skip_text)
                break;
            push_char(ev.text[0], (ev.mods & PM_CTRL) != 0);
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
