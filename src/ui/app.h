#ifndef FLUXION_APP_H
#define FLUXION_APP_H

#include <stdbool.h>

/* the whole app - history, the input line, the soft-key folders. runs until
   [2nd][mode] (Esc / window closed on the PC). same code on both. */

void app_run(void);

/* for the mode page (ui/mode.c) */
const char *app_form_name(void); /* "by hand", "expanded", ... */
void app_next_form(void); /* cycles it and redoes the last answer, like [2nd][enter] */
bool app_rationalize(void);
void app_toggle_rationalize(void);
#endif
