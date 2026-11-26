#ifndef FLUXION_EVENTS_PC_H
#define FLUXION_EVENTS_PC_H

/* PC only. what the SDL window sends, as plain data. keys_pc.c turns it into presses, pc_input.c into actions */

#include <stdbool.h>

/* a key: typed ones are their ASCII value, lower case, the rest are named */
enum {
    PK_NONE,
    PK_BACKSPACE = 0x100, PK_DELETE, PK_ENTER, PK_ESCAPE, PK_TAB,
    PK_LEFT, PK_RIGHT, PK_UP, PK_DOWN, PK_HOME, PK_END,
    PK_F1, PK_F2, PK_F3, PK_F4, PK_F5, PK_F6, PK_F7, PK_F11,
    PK_KP_0, PK_KP_1, PK_KP_2, PK_KP_3, PK_KP_4, PK_KP_5, PK_KP_6, PK_KP_7, PK_KP_8, PK_KP_9, /* in a row */
    PK_KP_DOT, PK_KP_PLUS, PK_KP_MINUS, PK_KP_MUL, PK_KP_DIV, PK_KP_ENTER,
    PK_SHIFT, PK_ALT, PK_CTRL,  /* either side */
    PK_MOD_OTHER,   /* gui, caps lock, num lock ... */
    PK_OTHER
};

enum { PM_CTRL = 1, PM_ALT = 2, PM_SHIFT = 4 };

typedef enum {
    EV_NONE,        /* something came with nothing in it for us */
    EV_QUIT,
    EV_REDRAW,      /* window size or scale changed, already applied */
    EV_FOCUS_LOST,
    EV_KEY_DOWN, EV_KEY_UP,
    EV_TEXT,
    EV_MOUSE_DOWN, EV_MOUSE_DRAG, EV_MOUSE_MOVE,    /* x y in layout pixels */
    EV_MOUSE_LEAVE, /* out of the window */
    EV_WHEEL        /* 1 up, -1 down */
} pc_ev_kind_t;

typedef struct {
    pc_ev_kind_t kind;
    int key;            /* key events */
    unsigned mods;      /* PM_ bits when it happened */
    bool repeat;
    char text[8];       /* EV_TEXT, utf-8 */
    int x, y;
    int wheel;
} pc_event_t;

/* waits up to ms (-1 for ever), false if it ran out */
bool pc_event_wait(pc_event_t *ev, int ms);

/* F11 and Ctrl + - 0 belong to the window, not to the input. whoever gets a key down asks first */
typedef enum { PW_NOT_MINE, PW_TAKEN, PW_REDRAW } pc_window_t;
pc_window_t pc_window_key(const pc_event_t *ev);

/* doesn't wait: the keys down right now, lowest scancode first, how many. the mods now */
int pc_keys_held(int *out, int max);
unsigned pc_mods_now(void);
/* drops the key events still queued */
void pc_keys_flush(void);

#endif
