#ifndef FLUXION_CALC1_H
#define FLUXION_CALC1_H

#include <stdbool.h>
#include "engine/ast.h"
#include "render/layout.h"

/* tangent( critical( extrema( inflect( increasing( taylor(, calc 1 on top of the derivative and solve. true if cmd
   was one of these, and then the answer (or a message) is already pushed. src is cut up at the commas */
bool calc1_run(const char *cmd, char *src, box_t *in);

/* d/dvar of f, times over. NULL on oom or cancelled */
pcas_ast_t *derive(pcas_ast_t *f, Symbol var, int times);

/* f'(a) worked out. NULL on oom, or with a message when f or f' is undefined at a */
pcas_ast_t *slope_at(pcas_ast_t *f, Symbol var, pcas_ast_t *a, const char **message);

/* f with v put in for var, worked out */
pcas_ast_t *value_at(pcas_ast_t *f, Symbol var, pcas_ast_t *v);

/* var -> a from one side (-1 or 1), worked out. a limit node when it doesn't come out */
pcas_ast_t *limit_at(pcas_ast_t *f, Symbol var, pcas_ast_t *a, int side);

/* r a thousandth to one side (side is -1 or 1), not worked out */
pcas_ast_t *beside(pcas_ast_t *r, int side);

/* equal to about 9 digits, relative once past 1 */
bool numbers_same(double a, double b);

/* the box under a continuity answer. NULL left, right or at say undefined, verdict is the last line */
box_t *limits_box(pcas_ast_t *left, pcas_ast_t *right, pcas_ast_t *at, const char *verdict);

/* the steps page for tangent(, critical(, extremma( or inflect(, src is what's inside. NULL when it opened, or what to say */
const char *calc1_work(const char *cmd, char *src);

#endif
