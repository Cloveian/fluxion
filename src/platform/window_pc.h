#ifndef FLUXION_WINDOW_PC_H
#define FLUXION_WINDOW_PC_H

/* PC only. an SDL3 window showing screen_pc.c's framebuffer, scaled up.
   while it's open, screen_show() draws into it and keys_pc.c reads its keyboard. */

#include <stdbool.h>

bool window_open(const char *title);
void window_present(void);
void window_close(void);
void window_fullscreen_toggle(void);
/* rounds the corners of the pixels (scale2x / 3x) at a pixel size of 2 or more. starts on for a high density screen */
void window_smooth_toggle(void);
/* window changed size, the framebuffer gets what fits at this scale */
void window_sync(void);
/* 1 or -1, bigger or smaller pixels */
void window_zoom(int step);
/* a mouse position from SDL (window units) as a framebuffer pixel */
void window_point(float wx, float wy, int *x, int *y);

/* open, show the framebuffer, wait for close (or Esc), close. 0 on success. */
int window_show(const char *title);

#endif
