#ifndef FLUXION_WORK_H
#define FLUXION_WORK_H

/*
    the steps page, show the work for a solve. the methods are the soft keys, and
    the app hands it its keys while it's open, like a page.
*/

#include <stdbool.h>
#include "ui/bind.h"
#include "engine/ast.h"

/* src is the line inside solve( ), it gets cut up. NULL when it opened, or what to say */
const char *work_open(char *src);
/* src is the line inside system( ). linear ones by elimination. NULL when it opened, or what to say */
const char *work_open_system(char *src);
/* the steps for expand( or factor( on e (the page keeps it) */
const char *work_open_expr(bool expand, pcas_ast_t *e);
/* the steps for d/dx(...), a limit or an integral, e is that node and the page keeps it */
const char *work_open_calc(pcas_ast_t *e);
bool work_is_open(void);
void work_close(void);
bool work_act(action_t a); /* false if it means nothing here */
const char *const *work_softkeys(void); /* the five method names, NULL for steps that don't have them */
/* everything under the status bar */
void work_draw(void);

#endif
