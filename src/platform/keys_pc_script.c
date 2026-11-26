#ifndef __TICE__

/* scripted presses for tests and renders, keys_pc.c asks here before it looks at the keyboard */

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
    if(strncmp(s, "click:", 6) == 0 || strncmp(s, "drag:", 5) == 0 || strncmp(s, "hover:", 6) == 0) { /* click:120,200  drag:200,200, with alpha+ it's a shift-click */
        const char *comma = strchr(s, ',');

        press.key = s[0] == 'c' ? K_POINT_DOWN : s[0] == 'd' ? K_POINT_DRAG : K_POINT_MOVE;
        press.x = (short)atoi(strchr(s, ':') + 1);
        press.y = comma != NULL ? (short)atoi(comma + 1) : 0;
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

#else
typedef int keys_pc_script_unused; /* nothing here on the calculator */
#endif
