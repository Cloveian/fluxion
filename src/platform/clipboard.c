/* the system clipboard, through SDL on the PC */

#include "clipboard.h"

#include <stddef.h>

#if defined(COMPILE_PC) && !defined(__EMSCRIPTEN__) && !defined(__TICE__) && !defined(_TINSPIRE)

#include <SDL3/SDL.h>
#include <stdlib.h>
#include <string.h>

static char *held; /* stands in for the SDL clipboard in the png runs, no window there */

static char *copy_of(const char *text) {
    size_t n = strlen(text) + 1;
    char *copy = malloc(n);

    if(copy != NULL)
        memcpy(copy, text, n);
    return copy;
}

void clipboard_put(const char *text) {
    if(text == NULL)
        return;
    if(SDL_WasInit(SDL_INIT_VIDEO) != 0) {
        SDL_SetClipboardText(text);
    } else {
        free(held);
        held = copy_of(text);
    }
}

char *clipboard_get(void) {
    char *sdl, *copy;

    if(SDL_WasInit(SDL_INIT_VIDEO) == 0)
        return held != NULL ? copy_of(held) : NULL;
    if(!SDL_HasClipboardText())
        return NULL;
    sdl = SDL_GetClipboardText();
    if(sdl == NULL)
        return NULL;
    copy = copy_of(sdl);
    SDL_free(sdl);
    return copy;
}

#else

void clipboard_put(const char *text) { (void)text; }
char *clipboard_get(void) { return NULL; }

#endif
