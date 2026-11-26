#ifndef __TICE__

/* PC only. shows screen_pc.c's framebuffer in an SDL3 window, the framebuffer is the window's real pixels. F11 fullscreen, Ctrl + - 0 for the zoom */

#include "window_pc.h"
#include "screen.h"

#include <SDL3/SDL.h>

#define SCALE_START 3 /* real pixels to a layout pixel at density 1 and no zoom */

static const float zooms[] = { 0.5f, 0.67f, 0.8f, 0.9f, 1.0f, 1.1f, 1.25f, 1.5f, 1.75f, 2.0f, 2.5f, 3.0f, 4.0f }; /* Ctrl + and - steps */
#define ZOOM_ONE 4 /* index of the 1.0 */
#define ZOOMS (int)(sizeof zooms / sizeof zooms[0])

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static int zoom = ZOOM_ONE;
static bool pixels; /* the bitmap fonts: whole pixels only or the blocks come out uneven */
static float density = 1; /* window pixels to a point, 1.5 at a 150% display scale */
static int tex_w, tex_h; /* what the texture was made for */

/* real pixels to a layout pixel now */
static float ppu(void) {
    float p = (float)SCALE_START * density * zooms[zoom];

    return pixels ? (p < 1 ? 1 : SDL_roundf(p)) : p;
}

void window_set_pixels(bool on) {
    pixels = on;
}

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
    screen_set_view(px, py, ppu());
}

void window_sync(void) {
    int px, py, w, h;

    if(window == NULL)
        return;
    SDL_GetWindowSizeInPixels(window, &px, &py);
    screen_pixel_size(&w, &h);
    if(px != w || py != h)
        screen_set_view(px, py, ppu());
}

void window_point(float wx, float wy, int *x, int *y) {
    float per = screen_ppu();

    *x = (int)(wx * density / per);
    *y = (int)(wy * density / per);
}

/* moved to a screen with another scale, same size in points */
void window_rescale(void) {
    float now;

    if(window == NULL)
        return;
    now = SDL_GetWindowPixelDensity(window);
    if(now <= 0 || now == density)
        return;
    density = now;
    apply_view();
}

void window_zoom(int step) {
    int to = step == 0 ? ZOOM_ONE : zoom + step;

    if(to < 0 || to >= ZOOMS)
        return;
    zoom = to;
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
    if(!SDL_CreateWindowAndRenderer(title, SCREEN_W * SCALE_START, SCREEN_H * SCALE_START, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY, &window, &renderer)) {
        SDL_Log("window: %s", SDL_GetError());
        SDL_Quit();
        return false;
    }

    density = SDL_GetWindowPixelDensity(window);
    if(density <= 0)
        density = 1;
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
