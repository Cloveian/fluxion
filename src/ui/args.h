#ifndef FLUXION_ARGS_H
#define FLUXION_ARGS_H

#include <stdbool.h>
#include "engine/ast.h"
#include "engine/error.h"

#define ARGS_LIMIT 17 /* max args_read can take. piecewise( with 8 pieces and its x=v, the most of any command */

/* cut src at the top level commas, in place. empty pieces kept (triangle(3,4,5,,,)
   needs them). how many pieces, 0 for more than max (src half cut up by then) */
int args_split(char *src, char **piece, int max);

/* NULL and err set on a parse error, an empty piece counts as one */
pcas_ast_t *args_parse(const char *s, pcas_error_t *err);

/* args_parse with degrees going in like the main line does it, so sin(30) < x means what it says */
pcas_ast_t *args_parse_deg(const char *s, pcas_error_t *err);

/* split + parse every piece. -1 on a parse error, nothing left allocated */
int args_read(char *src, pcas_ast_t **args, int max, pcas_error_t *err);

/* one letter or theta. shown gets what the answer prints for it, NULL if not needed */
bool args_var(const char *s, Symbol *var, char *shown);

#endif
