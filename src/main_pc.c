#ifndef __TICE__

/* the PC build of Fluxion. same app, just in a window with the keyboard as the
   keypad (see platform/keys_pc.c for which key is which).

     ./build-pc/fluxion-app                          the window
     ./build-pc/fluxion-app --png out.png [--scale 2] x x^2 + 2 enter ...
        no window: press these keys (names from keys_pc.c, or single characters),
        then save the last screen */

#include <stdlib.h>
#include <string.h>

#include "ui/app.h"
#include "platform/keys.h"
#include "platform/screen.h"
#include "platform/window_pc.h"

int main(int argc, char **argv)
{
    const char *png = NULL;
    int i = 1, scale = 2;

    for(; i + 1 < argc && strncmp(argv[i], "--", 2) == 0; i += 2) {
        if(strcmp(argv[i], "--png") == 0)
            png = argv[i + 1];
        else if(strcmp(argv[i], "--scale") == 0)
            scale = atoi(argv[i + 1]);
    }

    if(png != NULL) {
        keys_script(&argv[i], argc - i);
        app_run();
        return screen_save_png(png, scale) == 0 ? 0 : 1;
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
