#ifndef __TICE__

/* the SDL side of the PC's input: events in, plain pc_event_t out */

#include "events_pc.h"
#include "window_pc.h"

#include <string.h>
#include <SDL3/SDL.h>

/* the keypad's keys by where they are (the scancode), so NumLock doesn't matter. then by keycode */
static int keypad_key(SDL_Scancode sc, SDL_Keycode k) {
    switch(sc) {
    case SDL_SCANCODE_KP_0:        return PK_KP_0;
    case SDL_SCANCODE_KP_1:        return PK_KP_1;
    case SDL_SCANCODE_KP_2:        return PK_KP_2;
    case SDL_SCANCODE_KP_3:        return PK_KP_3;
    case SDL_SCANCODE_KP_4:        return PK_KP_4;
    case SDL_SCANCODE_KP_5:        return PK_KP_5;
    case SDL_SCANCODE_KP_6:        return PK_KP_6;
    case SDL_SCANCODE_KP_7:        return PK_KP_7;
    case SDL_SCANCODE_KP_8:        return PK_KP_8;
    case SDL_SCANCODE_KP_9:        return PK_KP_9;
    case SDL_SCANCODE_KP_PERIOD:   return PK_KP_DOT;
    case SDL_SCANCODE_KP_PLUS:     return PK_KP_PLUS;
    case SDL_SCANCODE_KP_MINUS:    return PK_KP_MINUS;
    case SDL_SCANCODE_KP_MULTIPLY: return PK_KP_MUL;
    case SDL_SCANCODE_KP_DIVIDE:   return PK_KP_DIV;
    case SDL_SCANCODE_KP_ENTER:    return PK_KP_ENTER;
    default: break;
    }
    switch(k) {
    case SDLK_KP_0:        return PK_KP_0;
    case SDLK_KP_1:        return PK_KP_1;
    case SDLK_KP_2:        return PK_KP_2;
    case SDLK_KP_3:        return PK_KP_3;
    case SDLK_KP_4:        return PK_KP_4;
    case SDLK_KP_5:        return PK_KP_5;
    case SDLK_KP_6:        return PK_KP_6;
    case SDLK_KP_7:        return PK_KP_7;
    case SDLK_KP_8:        return PK_KP_8;
    case SDLK_KP_9:        return PK_KP_9;
    case SDLK_KP_PERIOD:   return PK_KP_DOT;
    case SDLK_KP_PLUS:     return PK_KP_PLUS;
    case SDLK_KP_MINUS:    return PK_KP_MINUS;
    case SDLK_KP_MULTIPLY: return PK_KP_MUL;
    case SDLK_KP_DIVIDE:   return PK_KP_DIV;
    case SDLK_KP_ENTER:    return PK_KP_ENTER;
    default:               return PK_NONE;
    }
}

static int raw_key(SDL_Scancode sc, SDL_Keycode k) {
    int pad = keypad_key(sc, k);

    if(pad != PK_NONE)
        return pad;
    switch(k) {
    case SDLK_BACKSPACE: return PK_BACKSPACE;
    case SDLK_DELETE:    return PK_DELETE;
    case SDLK_RETURN:    return PK_ENTER;
    case SDLK_ESCAPE:    return PK_ESCAPE;
    case SDLK_TAB:       return PK_TAB;
    case SDLK_LEFT:      return PK_LEFT;
    case SDLK_RIGHT:     return PK_RIGHT;
    case SDLK_UP:        return PK_UP;
    case SDLK_DOWN:      return PK_DOWN;
    case SDLK_HOME:      return PK_HOME;
    case SDLK_END:       return PK_END;
    case SDLK_F1:        return PK_F1;
    case SDLK_F2:        return PK_F2;
    case SDLK_F3:        return PK_F3;
    case SDLK_F4:        return PK_F4;
    case SDLK_F5:        return PK_F5;
    case SDLK_F11:       return PK_F11;
    case SDLK_LSHIFT:
    case SDLK_RSHIFT:    return PK_SHIFT;
    case SDLK_LALT:
    case SDLK_RALT:      return PK_ALT;
    case SDLK_LCTRL:
    case SDLK_RCTRL:     return PK_CTRL;
    case SDLK_LGUI:
    case SDLK_RGUI:
    case SDLK_MODE:
    case SDLK_CAPSLOCK:
    case SDLK_NUMLOCKCLEAR: return PK_MOD_OTHER;
    default:             return k > 0 && k < 0x80 ? (int)k : PK_OTHER;
    }
}

static unsigned mods_of(SDL_Keymod m) {
    return ((m & SDL_KMOD_CTRL) ? PM_CTRL : 0) | ((m & SDL_KMOD_ALT) ? PM_ALT : 0) | ((m & SDL_KMOD_SHIFT) ? PM_SHIFT : 0);
}

unsigned pc_mods_now(void) {
    return mods_of(SDL_GetModState());
}

bool pc_event_wait(pc_event_t *out, int ms) {
    SDL_Event ev;

    memset(out, 0, sizeof *out);
    if(!SDL_WaitEventTimeout(&ev, ms))
        return false;

    switch(ev.type) {
    case SDL_EVENT_QUIT:
        out->kind = EV_QUIT;
        break;

    case SDL_EVENT_WINDOW_RESIZED:
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: /* same points, other pixels */
        window_sync();
        out->kind = EV_REDRAW;
        break;

    case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
        window_rescale();
        out->kind = EV_REDRAW;
        break;

    case SDL_EVENT_WINDOW_EXPOSED:
        window_present();
        break;

    case SDL_EVENT_WINDOW_FOCUS_LOST:
        out->kind = EV_FOCUS_LOST;
        break;

    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
        out->kind = ev.type == SDL_EVENT_KEY_DOWN ? EV_KEY_DOWN : EV_KEY_UP;
        out->key = raw_key(ev.key.scancode, ev.key.key);
        out->mods = mods_of(ev.key.mod);
        out->repeat = ev.key.repeat;
        break;

    case SDL_EVENT_TEXT_INPUT: /* printable characters come here, with shift and layout applied */
        out->kind = EV_TEXT;
        strncpy(out->text, ev.text.text, sizeof out->text - 1);
        out->mods = pc_mods_now();
        break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if(ev.button.button == SDL_BUTTON_LEFT) {
            out->kind = EV_MOUSE_DOWN;
            window_point(ev.button.x, ev.button.y, &out->x, &out->y);
            out->mods = pc_mods_now();
        }
        break;

    case SDL_EVENT_MOUSE_MOTION:
        out->kind = (ev.motion.state & SDL_BUTTON_LMASK) != 0 ? EV_MOUSE_DRAG : EV_MOUSE_MOVE;
        window_point(ev.motion.x, ev.motion.y, &out->x, &out->y);
        out->mods = pc_mods_now();
        break;

    case SDL_EVENT_WINDOW_MOUSE_LEAVE:
        out->kind = EV_MOUSE_LEAVE;
        break;

    case SDL_EVENT_MOUSE_WHEEL:
        if(ev.wheel.y != 0) {
            out->kind = EV_WHEEL;
            out->wheel = ev.wheel.y > 0 ? 1 : -1;
        }
        break;

    default:
        break;
    }
    return true;
}

pc_window_t pc_window_key(const pc_event_t *ev) {
    bool bigger, smaller, reset;

    if(ev->key == PK_F11) {
        window_fullscreen_toggle();
        return PW_TAKEN;
    }
    if((ev->mods & PM_CTRL) == 0)
        return PW_NOT_MINE;
    bigger = ev->key == '=' || ev->key == '+' || ev->key == PK_KP_PLUS;
    smaller = ev->key == '-' || ev->key == PK_KP_MINUS;
    reset = ev->key == '0' || ev->key == PK_KP_0;
    if(!bigger && !smaller && !reset)
        return PW_NOT_MINE;
    window_zoom(bigger ? 1 : smaller ? -1 : 0);
    return PW_REDRAW;
}

int pc_keys_held(int *out, int max) {
    const bool *down;
    int sc, n = 0;

    SDL_PumpEvents();
    down = SDL_GetKeyboardState(NULL);
    for(sc = 0; sc < SDL_SCANCODE_COUNT && n < max; sc++) {
        if(down[sc])
            out[n++] = raw_key((SDL_Scancode)sc, SDL_GetKeyFromScancode((SDL_Scancode)sc, SDL_KMOD_NONE, false));
    }
    return n;
}

void pc_keys_flush(void) {
    SDL_FlushEvents(SDL_EVENT_KEY_DOWN, SDL_EVENT_TEXT_INPUT);
}

#else
typedef int events_pc_unused; /* nothing here on the calculator */
#endif
