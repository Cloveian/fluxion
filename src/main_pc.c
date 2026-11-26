#ifndef __TICE__

/* the PC build of Fluxion. same app, just in a window with the keyboard as the
   keypad (see platform/keys_pc.c for which key is which).

     ./build-pc/fluxion-app                          the window
     ./build-pc/fluxion-app --png out.png [--scale 2] [--size 640x480] [--pixels 1] x x^2 + 2 enter ...
        no window: press these keys (names from keys_pc.c, or single characters),
        then save the last screen. --pixels 1 is the bitmap fonts */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "render/draw.h"
#include "ui/app.h"
#include "platform/keys.h"
#include "platform/screen.h"
#include "platform/window_pc.h"

int main(int argc, char **argv)
{
    const char *png = NULL;
    int i = 1, w = 320, h = 240;
    float scale = 2; /* a fraction is fine for the outline text */
    bool pixels = false;

    for(; i + 1 < argc && strncmp(argv[i], "--", 2) == 0; i += 2) {
        if(strcmp(argv[i], "--png") == 0)
            png = argv[i + 1];
        else if(strcmp(argv[i], "--scale") == 0)
            scale = (float)atof(argv[i + 1]);
        else if(strcmp(argv[i], "--pixels") == 0)
            pixels = atoi(argv[i + 1]) != 0;
        else if(strcmp(argv[i], "--size") == 0) /* 640x480, layout pixels */
            sscanf(argv[i + 1], "%dx%d", &w, &h);
    }
#ifdef SCREEN_RESIZABLE
    if(!pixels)
        draw_use_vector();
    window_set_pixels(pixels);
#endif

    if(png != NULL) {
        scale = scale < 1 ? 1 : scale > 8 ? 8 : scale;
        if(pixels) /* bitmap fonts, same as the window */
            scale = (float)(int)(scale + 0.5f);
#ifdef SCREEN_RESIZABLE /* the web page's is the one size */
        screen_set_view((int)((float)w * scale + 0.5f), (int)((float)h * scale + 0.5f), scale);
#endif
        keys_script(&argv[i], argc - i);
        app_run();
        return screen_save_png(png, (int)scale) == 0 ? 0 : 1;
    }

    if(!window_open("Fluxion"))
        return 1;
    app_run();
    window_close();
    return 0;
}

#else
typedef int main_pc_unused; /* the calculator's main is main.c */
#endif
