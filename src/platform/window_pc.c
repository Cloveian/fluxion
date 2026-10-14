#ifndef __TICE__

/* PC only. shows screen_pc.c's framebuffer in an SDL3 window, scaled up */

#include "window_pc.h"
#include "screen.h"

#include <SDL3/SDL.h>

#define SCALE 3

int window_show(const char *title) {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    static unsigned char rgb[SCREEN_H * SCREEN_W * 3];
    const unsigned char *px = screen_pixels();
    bool running = true;
    int i;

    if(!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init: %s", SDL_GetError());
        return -1;
    }

    if(!SDL_CreateWindowAndRenderer(title, SCREEN_W * SCALE, SCREEN_H * SCALE, 0, &window, &renderer)) {
        SDL_Log("window: %s", SDL_GetError());
        SDL_Quit();
        return -1;
    }

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING,
                                SCREEN_W, SCREEN_H);
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);

    for(i = 0; i < SCREEN_W * SCREEN_H; i++)
        screen_rgb(px[i], &rgb[i * 3]);
    SDL_UpdateTexture(texture, NULL, rgb, SCREEN_W * 3);

    while(running) {
        SDL_Event ev;

        while(SDL_PollEvent(&ev)) {
            if(ev.type == SDL_EVENT_QUIT)
                running = false;
            else if(ev.type == SDL_EVENT_KEY_DOWN && ev.key.key == SDLK_ESCAPE)
                running = false;
        }

        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

#else
typedef int window_pc_unused; /* nothing here on the calculator */
#endif
