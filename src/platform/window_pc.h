#ifndef FLUXION_WINDOW_PC_H
#define FLUXION_WINDOW_PC_H

/* PC only. an SDL3 window showing screen_pc.c's framebuffer, scaled up.
   while it's open, screen_show() draws into it and keys_pc.c reads its keyboard. */

#include <stdbool.h>

bool window_open(const char *title);
void window_present(void);
void window_close(void);

/* open, show the framebuffer, wait for close (or Esc), close. 0 on success. */
int window_show(const char *title);

#endif
