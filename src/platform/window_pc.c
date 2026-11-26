#ifndef __TICE__

/* PC only. shows screen_pc.c's framebuffer in an SDL3 window. the framebuffer is the window's real pixels, each layout pixel a whole number of them. F11 fullscreen, Ctrl + and - for that number */

#include "window_pc.h"
#include "screen.h"

#include <SDL3/SDL.h>
#include <stdlib.h>

#define SCALE_START 3 /* at density 1, times the density otherwise */
#define SCALE_MAX 12

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static int scale = SCALE_START; /* window pixels to a layout pixel, whole numbers */
static float density = 1; /* window pixels to a point, 1.5 at a 150% display scale */
static int tex_w, tex_h; /* what the texture was made for */

/* a texture for the framebuffer size now */
static void make_texture(void) {
    if(texture != NULL)
        SDL_DestroyTexture(texture);
    screen_pixel_size(&tex_w, &tex_h);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, tex_w, tex_h);
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
}

/* the framebuffer gets the window's pixels, at the scale now */
static void apply_view(void) {
    int px, py;

    SDL_GetWindowSizeInPixels(window, &px, &py);
    screen_set_view(px, py, (float)scale);
}

void window_sync(void) {
    int px, py, w, h;

    if(window == NULL)
        return;
    SDL_GetWindowSizeInPixels(window, &px, &py);
    screen_pixel_size(&w, &h);
    if(px != w || py != h)
        screen_set_view(px, py, (float)scale);
}

void window_point(float wx, float wy, int *x, int *y) {
    float ppu = screen_ppu();

    *x = (int)(wx * density / ppu);
    *y = (int)(wy * density / ppu);
}

/* moved to a screen with another scale, same size in points */
void window_rescale(void) {
    float now;
    int fit;

    if(window == NULL)
        return;
    now = SDL_GetWindowPixelDensity(window);
    if(now <= 0 || now == density)
        return;
    fit = (int)SDL_lroundf((float)scale * now / density);
    scale = fit < 1 ? 1 : fit > SCALE_MAX ? SCALE_MAX : fit;
    density = now;
    apply_view();
}

void window_zoom(int step) {
    if(scale + step < 1 || scale + step > SCALE_MAX)
        return;
    scale += step;
    apply_view();
}

bool window_open(const char *title) {
    if(window != NULL)
        return true;

    if(!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init: %s", SDL_GetError());
        return false;
    }

    /* without the density flag a fractional display scale gets a 1x buffer the compositor stretches */
    if(!SDL_CreateWindowAndRenderer(title, SCREEN_W * scale, SCREEN_H * scale, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY, &window, &renderer)) {
        SDL_Log("window: %s", SDL_GetError());
        SDL_Quit();
        return false;
    }

    density = SDL_GetWindowPixelDensity(window);
    if(density <= 0)
        density = 1;
    if(density != 1) { /* asked for 960 x 720 points, that's more pixels now */
        scale = (int)SDL_lroundf((float)SCALE_START * density);
        SDL_SetWindowSize(window, (int)SDL_lroundf((float)(SCREEN_W * scale) / density), (int)SDL_lroundf((float)(SCREEN_H * scale) / density));
    }
    apply_view(); /* the first frame at the real size, the resize events only fix it up */
    make_texture();
    SDL_StartTextInput(window);
    return true;
}

void window_present(void) {
    int w, h;

    if(window == NULL)
        return;
    screen_pixel_size(&w, &h);
    if(tex_w != w || tex_h != h)
        make_texture();
    SDL_UpdateTexture(texture, NULL, screen_rgb_buffer(), w * 3);
    SDL_RenderTexture(renderer, texture, NULL, NULL);
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
