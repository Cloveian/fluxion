/*Fluxion: show the work, worked out again like on paper. not in upstream PineappleCAS.*/

#ifndef STEPS_H_
#define STEPS_H_

#include "cas.h"

#define STEP_EQS_MAX 2  /*x_2=0 or x+3=0*/
#define STEPS_MAX 24

typedef enum {
    METHOD_OPTIMAL,
    METHOD_ROOT,    /*square root, no x term*/
    METHOD_FACTOR,
    METHOD_SQUARE,  /*complete the square*/
    METHOD_FORMULA,
    METHOD_COUNT
} Method;

/*left = right, or left = (right +- pm)/den when pm isn't NULL: x=+-3 (right NULL), x=_3+-2sqrt(2), the formula*/
typedef struct {
    pcas_ast_t *left, *right, *pm, *den;
} step_eq_t;

typedef struct {
    step_eq_t eq[STEP_EQS_MAX];
    int eqs;
    char move;      /*done to both sides on the way to the next line: + _ * / or 0*/
    pcas_ast_t *by; /*the 5 of _5*/
} step_t;

typedef struct {
    step_t line[STEPS_MAX];
    int n;
} steps_t;

/*left = right worked out for var, false (and nothing in out) if it's a shape or method it doesn't do*/
bool steps_solve(pcas_ast_t *left, pcas_ast_t *right, Symbol var, Method m, steps_t *out);
/*just the check, for greying out the method menu*/
bool steps_fit(pcas_ast_t *left, pcas_ast_t *right, Symbol var, Method m);
void steps_free(steps_t *s);

#endif
