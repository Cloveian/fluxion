/*
    the PC keyboard path without a keyboard: pushes made up SDL events at keys_wait() and prints the presses that come out. ./build-pc/keys-pc-test k:lshift u:lshift t:a mods:c k:c

      k:NAME u:NAME   key down / up. NAME is one character, or lshift lalt lctrl esc enter bs del
                      left right up down tab home end f1-f5 kp0-kp9 kpplus kpminus kpmul kpdiv kpdot kpenter
      t:TEXT          typed text
      mods:cas        modifiers held from here on (c ctrl, a alt, s shift), mods: for none
      m:down|move|drag:x,y   the mouse (a button is down for drag), m:leave   wheel:1 wheel:-1
      focus  quit
*/

#ifndef __TICE__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>

#include "platform/keys.h"

static SDL_Keymod mods;

static const struct { const char *name; SDL_Keycode key; SDL_Scancode sc; } named[] = {
    { "lshift", SDLK_LSHIFT, SDL_SCANCODE_LSHIFT }, { "lalt", SDLK_LALT, SDL_SCANCODE_LALT },
    { "lctrl", SDLK_LCTRL, SDL_SCANCODE_LCTRL }, { "esc", SDLK_ESCAPE, SDL_SCANCODE_ESCAPE },
    { "enter", SDLK_RETURN, SDL_SCANCODE_RETURN }, { "bs", SDLK_BACKSPACE, SDL_SCANCODE_BACKSPACE },
    { "del", SDLK_DELETE, SDL_SCANCODE_DELETE }, { "left", SDLK_LEFT, SDL_SCANCODE_LEFT },
    { "right", SDLK_RIGHT, SDL_SCANCODE_RIGHT }, { "up", SDLK_UP, SDL_SCANCODE_UP },
    { "down", SDLK_DOWN, SDL_SCANCODE_DOWN }, { "tab", SDLK_TAB, SDL_SCANCODE_TAB },
    { "home", SDLK_HOME, SDL_SCANCODE_HOME }, { "end", SDLK_END, SDL_SCANCODE_END },
    { "f1", SDLK_F1, SDL_SCANCODE_F1 }, { "f2", SDLK_F2, SDL_SCANCODE_F2 }, { "f3", SDLK_F3, SDL_SCANCODE_F3 },
    { "f4", SDLK_F4, SDL_SCANCODE_F4 }, { "f5", SDLK_F5, SDL_SCANCODE_F5 },
    { "kp0", SDLK_KP_0, SDL_SCANCODE_KP_0 }, { "kp1", SDLK_KP_1, SDL_SCANCODE_KP_1 },
    { "kp2", SDLK_KP_2, SDL_SCANCODE_KP_2 }, { "kp3", SDLK_KP_3, SDL_SCANCODE_KP_3 },
    { "kp4", SDLK_KP_4, SDL_SCANCODE_KP_4 }, { "kp5", SDLK_KP_5, SDL_SCANCODE_KP_5 },
    { "kp6", SDLK_KP_6, SDL_SCANCODE_KP_6 }, { "kp7", SDLK_KP_7, SDL_SCANCODE_KP_7 },
    { "kp8", SDLK_KP_8, SDL_SCANCODE_KP_8 }, { "kp9", SDLK_KP_9, SDL_SCANCODE_KP_9 },
    { "kpplus", SDLK_KP_PLUS, SDL_SCANCODE_KP_PLUS }, { "kpminus", SDLK_KP_MINUS, SDL_SCANCODE_KP_MINUS },
    { "kpmul", SDLK_KP_MULTIPLY, SDL_SCANCODE_KP_MULTIPLY }, { "kpdiv", SDLK_KP_DIVIDE, SDL_SCANCODE_KP_DIVIDE },
    { "kpdot", SDLK_KP_PERIOD, SDL_SCANCODE_KP_PERIOD }, { "kpenter", SDLK_KP_ENTER, SDL_SCANCODE_KP_ENTER },
    { "caps", SDLK_CAPSLOCK, SDL_SCANCODE_CAPSLOCK },
};

static void push_key(const char *name, bool down) {
    SDL_Event ev;
    unsigned i;

    memset(&ev, 0, sizeof ev);
    ev.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    ev.key.down = down;
    ev.key.mod = mods;
    if(name[1] == '\0') {
        ev.key.key = (SDL_Keycode)name[0];
        ev.key.scancode = SDL_SCANCODE_UNKNOWN;
    } else {
        for(i = 0; i < sizeof named / sizeof named[0]; i++) {
            if(strcmp(named[i].name, name) == 0) {
                ev.key.key = named[i].key;
                ev.key.scancode = named[i].sc;
                break;
            }
        }
        if(ev.key.key == 0) {
            fprintf(stderr, "no key called %s\n", name);
            exit(2);
        }
    }
    SDL_PushEvent(&ev);
}

static void push_mouse(const char *what) {
    SDL_Event ev;
    int x = 0, y = 0;
    const char *at = strchr(what, ':');

    memset(&ev, 0, sizeof ev);
    if(at != NULL)
        sscanf(at + 1, "%d,%d", &x, &y);
    if(strncmp(what, "down", 4) == 0) {
        ev.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
        ev.button.button = SDL_BUTTON_LEFT;
        ev.button.x = (float)x;
        ev.button.y = (float)y;
    } else if(strncmp(what, "leave", 5) == 0) {
        ev.type = SDL_EVENT_WINDOW_MOUSE_LEAVE;
    } else {
        ev.type = SDL_EVENT_MOUSE_MOTION;
        ev.motion.state = what[0] == 'd' ? SDL_BUTTON_LMASK : 0;
        ev.motion.x = (float)x;
        ev.motion.y = (float)y;
    }
    SDL_PushEvent(&ev);
}

int main(int argc, char **argv) {
    int i;
    SDL_Event ev;

    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    if(!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    for(i = 1; i < argc; i++) {
        const char *s = argv[i];

        if(strncmp(s, "k:", 2) == 0) {
            push_key(s + 2, true);
        } else if(strncmp(s, "u:", 2) == 0) {
            push_key(s + 2, false);
        } else if(strncmp(s, "t:", 2) == 0) {
            memset(&ev, 0, sizeof ev);
            ev.type = SDL_EVENT_TEXT_INPUT;
            ev.text.text = s + 2;
            SDL_PushEvent(&ev);
        } else if(strncmp(s, "mods:", 5) == 0) {
            mods = SDL_KMOD_NONE;
            if(strchr(s + 5, 'c') != NULL)
                mods |= SDL_KMOD_LCTRL;
            if(strchr(s + 5, 'a') != NULL)
                mods |= SDL_KMOD_LALT;
            if(strchr(s + 5, 's') != NULL)
                mods |= SDL_KMOD_LSHIFT;
            SDL_SetModState(mods);
        } else if(strncmp(s, "m:", 2) == 0) {
            push_mouse(s + 2);
        } else if(strncmp(s, "wheel:", 6) == 0) {
            memset(&ev, 0, sizeof ev);
            ev.type = SDL_EVENT_MOUSE_WHEEL;
            ev.wheel.y = (float)atoi(s + 6);
            SDL_PushEvent(&ev);
        } else if(strcmp(s, "focus") == 0) {
            memset(&ev, 0, sizeof ev);
            ev.type = SDL_EVENT_WINDOW_FOCUS_LOST;
            SDL_PushEvent(&ev);
        } else if(strcmp(s, "quit") == 0) {
            memset(&ev, 0, sizeof ev);
            ev.type = SDL_EVENT_QUIT;
            SDL_PushEvent(&ev);
        } else {
            fprintf(stderr, "what's %s\n", s);
            return 2;
        }
    }
    for(;;) {
        keypress_t p = keys_wait();

        printf("key %d held %u at %d,%d\n", (int)p.key, p.held, p.x, p.y);
        if(p.key == K_QUIT)
            break;
    }
    SDL_Quit();
    return 0;
}

#else
int main(void) { return 0; }
#endif
