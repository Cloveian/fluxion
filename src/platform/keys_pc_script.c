#ifndef __TICE__

/* scripted input for tests and renders, keys_pc.c and the desktop input ask here before they look at the keyboard */

#include "keys_pc.h"
#include "clipboard.h"
#include "screen.h"

#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>

/* scripted presses instead of the keyboard (for tests), see keys_script() */
static char *const *script;
static int script_len, script_pos;

void keys_script(char *const *presses, int count) {
    script = presses;
    script_len = count;
    script_pos = 0;
}

bool pc_script_on(void) {
    return script != NULL;
}

/*
    tokens that aren't presses, these get looked at mid-run too (keys_peek):
      wait:6        the next press waits 6 s
      png:out.png   the screen right then, at 2x
      clip:3x+2     puts that on the clipboard, for a paste to find
      click:x,y     mouse click at a framebuffer pixel (alpha+click: selects), drag:x,y is the button held, hover:x,y no button
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

/* click:x,y drag:x,y hover:x,y. the first letter says which, no comma is y 0 */
static bool parse_point(const char *s, int *x, int *y) {
    const char *comma = strchr(s, ',');

    if(strncmp(s, "click:", 6) != 0 && strncmp(s, "drag:", 5) != 0 && strncmp(s, "hover:", 6) != 0)
        return false;
    *x = atoi(strchr(s, ':') + 1);
    *y = comma != NULL ? atoi(comma + 1) : 0;
    return true;
}

/* a key name, or a single char to type. "on+" "2nd+" "alpha+" hold that one down for it */
static keypress_t parse_press(const char *s) {
    keypress_t press = { K_NONE, 0 };
    int x, y;

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
    if(parse_point(s, &x, &y)) { /* click:120,200  drag:200,200, with alpha+ it's a shift-click */
        press.key = s[0] == 'c' ? K_POINT_DOWN : s[0] == 'd' ? K_POINT_DRAG : K_POINT_MOVE;
        press.x = (short)x;
        press.y = (short)y;
        return press;
    }
    press.key = keys_named(s);
    return press;
}

/* the next press for keys_wait, K_QUIT when the script's done */
keypress_t pc_script_wait(void) {
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

/* the busy: press if one's next, for keys_peek */
keypress_t pc_script_peek(void) {
    keypress_t press = { K_NONE, 0 };

    if(!script_hold() && script_pos < script_len && strncmp(script[script_pos], "busy:", 5) == 0)
        return parse_press(script[script_pos++] + 5);
    return press;
}

/*
    the desktop input's tokens, as events. wait: png: clip: busy: work the same
      key:ctrl+c    a key down. ctrl+ alt+ shift+ in front, then a name (enter bs del esc tab left right up down home end f1-f7 f11 kpenter) or one character
      type:3x+2     typed text, a character at a time
      click:x,y     drag:x,y and hover:x,y too, shift+ in front is a shift-click
      wheel:-1      1 up, -1 down
*/
static const struct { const char *name; int key; } key_names[] = {
    { "enter", PK_ENTER }, { "kpenter", PK_KP_ENTER }, { "bs", PK_BACKSPACE }, { "del", PK_DELETE },
    { "esc", PK_ESCAPE }, { "tab", PK_TAB }, { "left", PK_LEFT }, { "right", PK_RIGHT },
    { "up", PK_UP }, { "down", PK_DOWN }, { "home", PK_HOME }, { "end", PK_END },
    { "f1", PK_F1 }, { "f2", PK_F2 }, { "f3", PK_F3 }, { "f4", PK_F4 }, { "f5", PK_F5 }, { "f6", PK_F6 }, { "f7", PK_F7 }, { "f11", PK_F11 },
};

static unsigned strip_mods(const char **s) {
    unsigned mods = 0;

    for(;;) {
        if(strncmp(*s, "ctrl+", 5) == 0) {
            mods |= PM_CTRL;
            *s += 5;
        } else if(strncmp(*s, "alt+", 4) == 0) {
            mods |= PM_ALT;
            *s += 4;
        } else if(strncmp(*s, "shift+", 6) == 0) {
            mods |= PM_SHIFT;
            *s += 6;
        } else {
            return mods;
        }
    }
}

static void parse_event(const char *s, pc_event_t *ev) {
    unsigned i;

    memset(ev, 0, sizeof *ev);
    if(strncmp(s, "key:", 4) == 0) {
        s += 4;
        ev->mods = strip_mods(&s);
        ev->kind = EV_KEY_DOWN;
        for(i = 0; i < sizeof key_names / sizeof key_names[0]; i++)
            if(strcmp(s, key_names[i].name) == 0)
                ev->key = key_names[i].key;
        if(ev->key == PK_NONE && s[0] != '\0' && s[1] == '\0')
            ev->key = s[0] >= 'A' && s[0] <= 'Z' ? s[0] - 'A' + 'a' : s[0];
        return;
    }

    ev->mods = strip_mods(&s); /* shift+click:x,y */
    if(parse_point(s, &ev->x, &ev->y)) {
        ev->kind = s[0] == 'c' ? EV_MOUSE_DOWN : s[0] == 'd' ? EV_MOUSE_DRAG : EV_MOUSE_MOVE;
    } else if(strncmp(s, "wheel:", 6) == 0) {
        ev->kind = EV_WHEEL;
        ev->wheel = atoi(s + 6) > 0 ? 1 : -1;
    }
}

static int type_at; /* characters of the type: token sent so far */

/* the next scripted event, EV_QUIT when it's done */
void pc_script_event(pc_event_t *ev) {
    memset(ev, 0, sizeof *ev);
    for(;;) {
        const char *s;

        while(script_hold())
            SDL_Delay(10);
        if(script_pos >= script_len) {
            ev->kind = EV_QUIT;
            return;
        }
        s = script[script_pos];
        if(strncmp(s, "busy:", 5) == 0) {
            script_pos++;
        } else if(strncmp(s, "type:", 5) == 0) {
            if(s[5 + type_at] == '\0') {
                script_pos++;
                type_at = 0;
                continue;
            }
            ev->kind = EV_TEXT;
            ev->text[0] = s[5 + type_at++];
            return;
        } else {
            script_pos++;
            parse_event(s, ev);
            return;
        }
    }
}

bool pc_script_busy_event(pc_event_t *ev) {
    if(script_hold() || script_pos >= script_len || strncmp(script[script_pos], "busy:", 5) != 0)
        return false;
    parse_event(script[script_pos++] + 5, ev);
    return true;
}

#else
typedef int keys_pc_script_unused; /* nothing here on the calculator */
#endif
