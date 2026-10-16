#ifndef ERROR_H_
#define ERROR_H_

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    E_SUCCESS,

    /*Because of bad programming*/
    E_GENERIC,

    /*trying to add nodes to a NODE_NUMBER for instance*/
    E_AST_NOT_ALLOWED,
    E_AST_OUT_OF_BOUNDS,

    /*Tokenizing and parsing*/
    E_TOK_INVALID,

    E_PARSE_BAD_OPERATOR,
    E_PARSE_UNMATCHED_CLOSE_PAR,
    E_PARSE_BAD_COMMA,

    E_EVAL_NO_MAPPING,

    E_OUT_OF_MEMORY, /*Fluxion: parse() ran out, see pcas_oom below*/

    AMOUNT_ERRORS
} pcas_error_t;

extern const char *error_text[AMOUNT_ERRORS];

/*Fluxion: out of memory. every allocation in the engine goes through these, and
  when one fails they set pcas_oom and pcas_cancelled, so everything unwinds the
  same way a cancel does and leaves a valid tree (see cas.h). the caller clears
  both before the next run. the site that got the NULL still has to not touch it*/
extern bool pcas_oom;
extern bool pcas_cancelled;
void *pcas_malloc(size_t size);
void *pcas_calloc(size_t count, size_t size);
void *pcas_realloc(void *old, size_t size);

#endif