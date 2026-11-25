#ifndef FLUXION_WINDOW_PC_H
#define FLUXION_WINDOW_PC_H

/* PC only. an SDL3 window showing screen_pc.c's framebuffer, scaled up.
   while it's open, screen_show() draws into it and keys_pc.c reads its keyboard. */

#include <stdbool.h>

bool window_open(const char *title);
void window_present(void);
void window_close(void);
void window_fullscreen_toggle(void);
/* the window changed size: the framebuffer gets as many pixels as fit at the current scale, screen_resize() runs when that's new */
void window_sync(void);
/* 1 or -1: bigger or smaller pixels, the framebuffer changes to fit */
void window_zoom(int step);

/* open, show the framebuffer, wait for close (or Esc), close. 0 on success. */
int window_show(const char *title);

#endif
