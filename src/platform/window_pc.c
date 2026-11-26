#ifndef __TICE__

/* PC only. shows screen_pc.c's framebuffer in an SDL3 window, scaled up by a whole number, black around what's left. the framebuffer follows the window size. F11 fullscreen, F10 smooth pixels, Ctrl + and - for the scale */

#include "window_pc.h"
#include "screen.h"

#include <SDL3/SDL.h>
#include <stdlib.h>

#define SCALE_START 3 /* at density 1, times the density otherwise */
#define SCALE_MAX 12

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static int scale = SCALE_START; /* window pixels to a framebuffer pixel, whole numbers */
static float density = 1; /* window pixels to a point, 1.5 at a 150% display scale */
static int tex_w, tex_h, tex_f; /* what the texture was made for */
static unsigned char *rgb;
static bool smooth; /* corners of the pixels rounded off, on from the start for a high density screen. F10 */

/* texture pixels to a framebuffer pixel */
static int smooth_factor(void) {
    return smooth ? screen_smooth_factor(scale) : 1;
}

/* texture and rgb buffer for the size now, f times as big across */
static void make_texture(int f) {
    if(texture != NULL)
        SDL_DestroyTexture(texture);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, SCREEN_W * f, SCREEN_H * f);
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    free(rgb);
    rgb = malloc((size_t)SCREEN_W * (size_t)SCREEN_H * (size_t)(f * f) * 3);
    tex_w = SCREEN_W;
    tex_h = SCREEN_H;
    tex_f = f;
}

void window_sync(void) {
    int px, py;

    if(window == NULL)
        return;
    SDL_GetWindowSizeInPixels(window, &px, &py);
    if(px / scale != SCREEN_W || py / scale != SCREEN_H)
        screen_resize(px / scale, py / scale);
}

void window_point(float wx, float wy, int *x, int *y) {
    *x = (int)(wx * density) / scale;
    *y = (int)(wy * density) / scale;
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
    window_sync();
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

    /* without the density flag a fractional display scale gets a 1x buffer the compositor stretches */
    if(!SDL_CreateWindowAndRenderer(title, SCREEN_W * scale, SCREEN_H * scale, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY, &window, &renderer)) {
        SDL_Log("window: %s", SDL_GetError());
        SDL_Quit();
        return false;
    }

    density = SDL_GetWindowPixelDensity(window);
    if(density <= 0)
        density = 1;
    smooth = density > 1.0f;
    if(density != 1) { /* asked for 960 x 720 points, that's more pixels now */
        scale = (int)SDL_lroundf((float)SCALE_START * density);
        SDL_SetWindowSize(window, (int)SDL_lroundf((float)(SCREEN_W * scale) / density), (int)SDL_lroundf((float)(SCREEN_H * scale) / density));
    }
    make_texture(smooth_factor());
    SDL_StartTextInput(window);
    return true;
}

void window_smooth_toggle(void) {
    smooth = !smooth;
    window_present();
}

void window_present(void) {
    const unsigned char *px = screen_pixels();
    SDL_FRect to = { 0, 0, 0, 0 };
    int i, f = smooth_factor();

    if(window == NULL)
        return;
    if(tex_w != SCREEN_W || tex_h != SCREEN_H || tex_f != f)
        make_texture(f);

    if(f == 1) {
        for(i = 0; i < SCREEN_W * SCREEN_H; i++)
            screen_rgb(px[i], &rgb[i * 3]);
    } else {
        screen_expand(f, rgb);
    }
    SDL_UpdateTexture(texture, NULL, rgb, SCREEN_W * f * 3);

    to.w = (float)(SCREEN_W * scale); /* from the corner, the rest stays black */
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
