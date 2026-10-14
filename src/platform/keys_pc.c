#ifndef __TICE__

/*
    PC version of keys.h: the SDL window's keyboard, mapped to TI keys.

      typing        digits, letters, + - * / ^ ( ) , .   (~ is (-), negate)
      Backspace     [clear]          Delete    [del]
      Enter         [enter]          arrows    arrows
      Ctrl          [on]             Tab       [2nd]
      F1-F5         the soft keys    Esc       [2nd][mode], quit
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

/* one scripted press - a key name, or a single char to type, "on+" means Ctrl */
static keypress_t scripted(void) {
    static const struct { const char *name; key_t key; } names[] = {
        { "enter", K_ENTER }, { "clear", K_CLEAR }, { "del", K_DEL },
        { "left", K_LEFT }, { "right", K_RIGHT }, { "up", K_UP }, { "down", K_DOWN },
        { "2nd", K_2ND }, { "alpha", K_ALPHA }, { "mode", K_MODE },
        { "y=", K_YEQU }, { "window", K_WINDOW }, { "zoom", K_ZOOM }, { "trace", K_TRACE }, { "graph", K_GRAPH },
        { "x^2", K_SQUARE }, { "x^-1", K_RECIP }, { "sin", K_SIN }, { "cos", K_COS }, { "tan", K_TAN },
        { "ln", K_LN }, { "log", K_LOG }, { "neg", K_NEG }, { "xt", K_XT }, { "sto", K_STO }, { "math", K_MATH },
    };
    keypress_t press = { K_QUIT, false };
    const char *s;
    unsigned i;

    if(script_pos >= script_len)
        return press;    /* script's done, quit */

    s = script[script_pos++];
    if(strncmp(s, "on+", 3) == 0) {
        press.on = true;
        s += 3;
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
        return (key_t)(K_LETTER_A + (c - 'A'));

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

    /* its key-down is queued too, don't let keys_wait() see it as a new press */
    if(press.key != K_NONE || press.on)
        SDL_FlushEvents(SDL_EVENT_KEY_DOWN, SDL_EVENT_TEXT_INPUT);
    return press;
}

keypress_t keys_wait(void) {
    keypress_t press = { K_NONE, false };

    if(script != NULL)
        return scripted();

    while(press.key == K_NONE) {
        SDL_Event ev;

        if(!SDL_WaitEvent(&ev)) {
            press.key = K_QUIT;
            break;
        }

        switch(ev.type) {
        case SDL_EVENT_QUIT:
            press.key = K_QUIT;
            break;

        case SDL_EVENT_WINDOW_EXPOSED:
            window_present();
            break;

        case SDL_EVENT_KEY_DOWN:
            press.on = (ev.key.mod & SDL_KMOD_CTRL) != 0;
            if(ev.key.key == SDLK_ESCAPE)
                press.key = K_QUIT;
            else
                press.key = from_key(ev.key.key);
            break;

        case SDL_EVENT_TEXT_INPUT:
            /* printable characters come here (with shift and layout applied) */
            press.key = from_text(ev.text.text[0]);
            press.on = (SDL_GetModState() & SDL_KMOD_CTRL) != 0;
            break;

        default:
            break;
        }
    }

    return press;
}

#else
typedef int keys_pc_unused; /* nothing here on the calculator */
#endif
