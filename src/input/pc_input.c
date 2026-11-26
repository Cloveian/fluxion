#ifdef COMPILE_PC

/* the desktop's input, the real keyboard and mouse as actions. no [2nd] / [alpha], what's typed is what's inserted, Esc doesn't quit */

#include "input.h"

#include <ctype.h>
#include <string.h>

#include "editor/editor.h"
#include "platform/keys_pc.h"

typedef struct {
    int key;
    unsigned mods; /* ctrl and alt exactly, shift only when it's here (so Shift+Left comes before Left) */
    action_t a;
    const char *text; /* for the catalog's key column */
} shortcut_t;

#define DO(k, m, op, n, text) { k, m, { op, n, NULL, 0 }, text }

static const shortcut_t shortcuts[] = {
    DO(PK_ENTER, PM_CTRL, A_NEXT_FORM, 0, "Ctrl+Enter"),
    DO(PK_ENTER, PM_SHIFT, A_DECIMAL, 0, "Shift+Enter"),
    DO(PK_ENTER, 0, A_ENTER, 0, "Enter"),
    DO(PK_KP_ENTER, 0, A_ENTER, 0, "Enter"),
    DO(PK_BACKSPACE, PM_CTRL, A_CLEAR_LINE, 1, "Ctrl+Backspace"),
    DO(PK_BACKSPACE, 0, A_CLEAR, 0, "Backspace"),
    DO(PK_DELETE, 0, A_DELETE, 0, "Delete"),

    DO(PK_LEFT, PM_CTRL, A_LINE_START, 0, "Ctrl+Left"),
    DO(PK_RIGHT, PM_CTRL, A_LINE_END, 0, "Ctrl+Right"),
    DO(PK_LEFT, PM_SHIFT, A_SELECT_LEFT, 0, "Shift+Left"),
    DO(PK_RIGHT, PM_SHIFT, A_SELECT_RIGHT, 0, "Shift+Right"),
    DO(PK_LEFT, 0, A_LEFT, 0, "Left"), DO(PK_RIGHT, 0, A_RIGHT, 0, "Right"),
    DO(PK_UP, 0, A_UP, 0, "Up"), DO(PK_DOWN, 0, A_DOWN, 0, "Down"),
    DO(PK_HOME, 0, A_LINE_START, 0, "Home"), DO(PK_END, 0, A_LINE_END, 0, "End"),

    DO(PK_F1, 0, A_SOFT, 0, "F1"), DO(PK_F2, 0, A_SOFT, 1, "F2"), DO(PK_F3, 0, A_SOFT, 2, "F3"),
    DO(PK_F4, 0, A_SOFT, 3, "F4"), DO(PK_F5, 0, A_SOFT, 4, "F5"),
    DO(PK_F6, 0, A_VARS, 0, "F6"), DO(PK_F7, 0, A_SETTINGS, 0, "F7"),
    DO(PK_TAB, 0, A_POPUP, POPUP_MATH, "Tab"),
    DO(PK_ESCAPE, 0, A_ESC, 0, "Esc"),

    DO('k', PM_CTRL, A_CATALOG, 0, "Ctrl+K"),
    DO('q', PM_CTRL, A_QUIT, 0, "Ctrl+Q"),
    DO('x', PM_CTRL, A_CUT, 0, "Ctrl+X"), DO('c', PM_CTRL, A_COPY, 0, "Ctrl+C"), DO('v', PM_CTRL, A_PASTE, 0, "Ctrl+V"),
    DO('z', PM_CTRL, A_UNDO, 0, "Ctrl+Z"), DO('a', PM_CTRL, A_SELECT_ALL, 0, "Ctrl+A"),
};

/* a typed character, the editor's own codes for the math ones */
static action_t char_action(int c) {
    action_t a = { A_CHAR, c, NULL, 0 };

    switch(c) {
    case '+': a.n = ED_PLUS; break;
    case '-': a.n = ED_SUB; break;
    case '*': a.n = ED_TIMES; break;
    case '/': a.n = ED_DIVIDE; break;
    case '~': a.n = ED_NEG; break;
    case '^': a.op = A_TEMPLATE; a.n = ED_POW; break;
    default:
        if(c <= 0 || c >= 0x80 || (!isalnum(c) && strchr(".,()=<>!%", c) == NULL))
            a.op = A_NONE;
        break;
    }
    return a;
}

static bool skip_text; /* the key already did it, its text would do it twice */

/* a key down. A_NONE for a typed key, its character comes as text */
static action_t key_action(const pc_event_t *ev) {
    action_t none = { A_NONE, 0, NULL, 0 };
    unsigned i;

    skip_text = false;
    if(ev->key >= PK_KP_0 && ev->key <= PK_KP_9) {
        skip_text = true; /* NumLock on sends the digit as text too */
        return char_action('0' + (ev->key - PK_KP_0));
    }
    switch(ev->key) {
    case PK_KP_DOT:   skip_text = true; return char_action('.');
    case PK_KP_PLUS:  skip_text = true; return char_action('+');
    case PK_KP_MINUS: skip_text = true; return char_action('-');
    case PK_KP_MUL:   skip_text = true; return char_action('*');
    case PK_KP_DIV:   skip_text = true; return char_action('/');
    default: break;
    }

    for(i = 0; i < sizeof shortcuts / sizeof shortcuts[0]; i++) {
        const shortcut_t *s = &shortcuts[i];
        bool shift_ok = (s->mods & PM_SHIFT) == 0 || (ev->mods & PM_SHIFT) != 0;

        if(s->key == ev->key && shift_ok && (s->mods & ~PM_SHIFT) == (ev->mods & ~PM_SHIFT))
            return s->a;
    }
    return none;
}

static void next_event(pc_event_t *ev) {
    if(pc_script_on())
        pc_script_event(ev);
    else
        pc_event_wait(ev, -1);
}

void input_begin(void (*on_sleep)(bool asleep)) {
    (void)on_sleep; /* no screen off on a desktop */
}

void input_end(void) {}

input_t input_wait(in_ctx_t ctx) {
    input_t in;
    pc_event_t ev;

    (void)ctx; /* every context binds the same here */
    memset(&in, 0, sizeof in);
    for(;;) {
        next_event(&ev);
        switch(ev.kind) {
        case EV_QUIT:
            in.quit = true;
            return in;

        case EV_REDRAW: /* nothing to do but draw */
            return in;

        case EV_KEY_DOWN: {
            pc_window_t taken = pc_window_key(&ev);

            if(taken == PW_REDRAW)
                return in;
            if(taken == PW_NOT_MINE) {
                in.a = in.plain = key_action(&ev);
                if(in.a.op != A_NONE)
                    return in;
            }
            break;
        }

        case EV_TEXT:
            if(!skip_text) {
                in.a = in.plain = char_action(ev.text[0]);
                if(in.a.op != A_NONE)
                    return in;
            }
            break;

        case EV_MOUSE_DOWN:
        case EV_MOUSE_DRAG:
        case EV_MOUSE_MOVE:
            in.pointer = true;
            in.drag = ev.kind == EV_MOUSE_DRAG;
            in.hover = ev.kind == EV_MOUSE_MOVE;
            in.shift = (ev.mods & PM_SHIFT) != 0;
            in.x = ev.x;
            in.y = ev.y;
            return in;

        case EV_MOUSE_LEAVE:
            in.pointer = in.hover = true;
            in.x = in.y = -1;
            return in;

        case EV_WHEEL: /* scrolls the history like the arrows */
            in.a.op = in.plain.op = ev.wheel > 0 ? A_UP : A_DOWN;
            return in;

        default:
            break;
        }
    }
}

void input_mods(input_mod_t out[3]) {
    memset(out, 0, 3 * sizeof out[0]);
}

const char *input_cancel_hint(void) {
    return "Esc";
}

unsigned input_binding_count(void) {
    return sizeof shortcuts / sizeof shortcuts[0];
}

action_t input_binding(unsigned i, bool *plain) {
    *plain = shortcuts[i].mods == 0;
    return shortcuts[i].a;
}

bool input_binding_text(unsigned i, char *buf, int n) {
    strncpy(buf, shortcuts[i].text, (size_t)n - 1);
    buf[n - 1] = '\0';
    return true;
}

const char *input_digit_keys(void) {
    return "0-9";
}

const char *input_letter_keys(void) {
    return "a-z";
}

#define HELD_MAX 24 /* keys down at once that the poll looks through */

bool input_poll(void) {
    pc_event_t ev;
    int down[HELD_MAX];
    int i, n;

    if(pc_script_on())
        return pc_script_busy_event(&ev) && ev.kind == EV_KEY_DOWN && ev.key == PK_ESCAPE;

    n = pc_keys_held(down, HELD_MAX);
    for(i = 0; i < n; i++) {
        if(down[i] == PK_ESCAPE) {
            pc_keys_flush(); /* its key-down is queued too, wait shouldn't see it as a press */
            return true;
        }
    }
    return false;
}

void input_poll_reset(void) {}

void input_script(char *const *tokens, int n) {
    keys_script(tokens, n);
}

#else
typedef int pc_input_unused; /* the calculators and the browser use keypad_input.c */
#endif
