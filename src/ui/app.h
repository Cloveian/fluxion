#ifndef FLUXION_APP_H
#define FLUXION_APP_H

#include <stdbool.h>
#include "render/layout.h"

/* the whole app - history, the input line, the soft-key folders. runs until
   [2nd][mode] (Esc / window closed on the PC). same code on both. */

void app_run(void);

/* for the mode page (ui/mode.c) */
const char *app_form_name(void); /* "by hand", "expanded", ... */
void app_next_form(void); /* cycles it and redoes the last answer, like [2nd][enter] */
bool app_rationalize(void);
void app_toggle_rationalize(void);
int app_form_index(void); /* for saving. set is quiet, no redoing the last answer */
void app_set_form_index(int i);

/* for the commands that live in their own file (ui/geom.c), so their answers
   come out the same as solve's. push takes in and out */
void push(box_t *in, box_t *out, const char *message, unsigned long ms);
box_t *value_box(pcas_ast_t *v);
box_t *solutions_box(char var, pcas_ast_t **roots, int n);
#endif
