#ifndef __TICE__

/* PC only. shows screen_pc.c's framebuffer in an SDL3 window, scaled up by a whole number that fits, black around it. resizable, the framebuffer follows the window size. F11 for fullscreen, Ctrl + and - for the scale */

#include "window_pc.h"
#include "screen.h"

#include <SDL3/SDL.h>
#include <stdlib.h>

#define SCALE_START 3
#define SCALE_MAX 6

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static int scale = SCALE_START; /* window pixels to a framebuffer pixel, whole numbers */
static int tex_w, tex_h; /* what the texture was made for */
static unsigned char *rgb;

/* the texture and the rgb buffer for the framebuffer size now */
static void make_texture(void) {
    if(texture != NULL)
        SDL_DestroyTexture(texture);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, SCREEN_W, SCREEN_H);
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    free(rgb);
    rgb = malloc((size_t)SCREEN_W * (size_t)SCREEN_H * 3);
    tex_w = SCREEN_W;
    tex_h = SCREEN_H;
}

void window_sync(void) {
    int px, py;

    if(window == NULL)
        return;
    SDL_GetWindowSizeInPixels(window, &px, &py);
    if(px / scale != SCREEN_W || py / scale != SCREEN_H)
        screen_resize(px / scale, py / scale);
}

void window_zoom(int step) {
    if(scale + step < 1 || scale + step > SCALE_MAX)
        return;
    scale += step;
    window_sync();
}

bool window_open(const char *title) {
    if(window != NULL)
        return true;

    if(!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init: %s", SDL_GetError());
        return false;
    }

    if(!SDL_CreateWindowAndRenderer(title, SCREEN_W * scale, SCREEN_H * scale, SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        SDL_Log("window: %s", SDL_GetError());
        SDL_Quit();
        return false;
    }

    make_texture();
    SDL_StartTextInput(window);
    return true;
}

void window_present(void) {
    const unsigned char *px = screen_pixels();
    SDL_FRect to = { 0, 0, 0, 0 };
    int i;

    if(window == NULL)
        return;
    if(tex_w != SCREEN_W || tex_h != SCREEN_H)
        make_texture();

    for(i = 0; i < SCREEN_W * SCREEN_H; i++)
        screen_rgb(px[i], &rgb[i * 3]);
    SDL_UpdateTexture(texture, NULL, rgb, SCREEN_W * 3);

    to.w = (float)(SCREEN_W * scale); /* whole pixels from the corner, what's left over is black */
    to.h = (float)(SCREEN_H * scale);
    SDL_RenderClear(renderer);
    SDL_RenderTexture(renderer, texture, NULL, &to);
    SDL_RenderPresent(renderer);
}

void window_fullscreen_toggle(void) {
    if(window != NULL)
        SDL_SetWindowFullscreen(window, (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) == 0);
}

void window_close(void) {
    if(window == NULL)
        return;

    SDL_StopTextInput(window);
    SDL_DestroyTexture(texture);
    free(rgb);
    rgb = NULL;
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
        else if(ev.type == SDL_EVENT_KEY_DOWN && ev.key.key == SDLK_F11)
            window_fullscreen_toggle();
        else if(ev.type == SDL_EVENT_WINDOW_RESIZED) {
            window_sync();
            window_present();
        } else if(ev.type == SDL_EVENT_WINDOW_EXPOSED)
            window_present();
    }

    window_close();
    return 0;
}

#else
typedef int window_pc_unused; /* nothing here on the calculator */
#endif
