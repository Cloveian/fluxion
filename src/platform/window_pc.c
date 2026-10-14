#ifndef __TICE__

/* PC only. shows screen_pc.c's framebuffer in an SDL3 window, scaled up */

#include "window_pc.h"
#include "screen.h"

#include <SDL3/SDL.h>

#define SCALE 3

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;

bool window_open(const char *title) {
    if(window != NULL)
        return true;

    if(!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init: %s", SDL_GetError());
        return false;
    }

    if(!SDL_CreateWindowAndRenderer(title, SCREEN_W * SCALE, SCREEN_H * SCALE, 0, &window, &renderer)) {
        SDL_Log("window: %s", SDL_GetError());
        SDL_Quit();
        return false;
    }

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING,
                                SCREEN_W, SCREEN_H);
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    SDL_StartTextInput(window);
    return true;
}

void window_present(void) {
    static unsigned char rgb[SCREEN_H * SCREEN_W * 3];
    const unsigned char *px = screen_pixels();
    int i;

    if(window == NULL)
        return;

    for(i = 0; i < SCREEN_W * SCREEN_H; i++)
        screen_rgb(px[i], &rgb[i * 3]);
    SDL_UpdateTexture(texture, NULL, rgb, SCREEN_W * 3);

    SDL_RenderClear(renderer);
    SDL_RenderTexture(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

void window_close(void) {
    if(window == NULL)
        return;

    SDL_StopTextInput(window);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    window = NULL;
}

int window_show(const char *title) {
    bool running = true;

    if(!window_open(title))
        return -1;
    window_present();

    while(running) {
        SDL_Event ev;

        if(!SDL_WaitEvent(&ev))
            break;
        if(ev.type == SDL_EVENT_QUIT)
            running = false;
        else if(ev.type == SDL_EVENT_KEY_DOWN && ev.key.key == SDLK_ESCAPE)
            running = false;
        else if(ev.type == SDL_EVENT_WINDOW_EXPOSED)
            window_present();
    }

    window_close();
    return 0;
}

#else
typedef int window_pc_unused; /* nothing here on the calculator */
#endif
