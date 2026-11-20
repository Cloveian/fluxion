/*Fluxion: show the work, worked out again like on paper. not in upstream PineappleCAS.*/

#ifndef STEPS_H_
#define STEPS_H_

#include "cas.h"

#define STEP_EQS_MAX 3  /*x_2=0 or x+3=0, and a=1, b=6, c=1*/
#define STEPS_MAX 24

typedef enum {
    METHOD_OPTIMAL,
    METHOD_ROOT,    /*square root, no x term*/
    METHOD_FACTOR,
    METHOD_SQUARE,  /*complete the square*/
    METHOD_FORMULA,
    METHOD_COUNT
} Method;

#define STEP_RULES_MAX 4

/*left = right, or left = (right +- pm)/den when pm isn't NULL: x=+-3 (right NULL), x=_3+-2sqrt(2), the formula.
just left with neither: an expression, = the line above (*dih*rivative and limit steps)*/
typedef struct {
    pcas_ast_t *left, *right, *pm, *den;
} step_eq_t;

typedef struct {
    step_eq_t eq[STEP_EQS_MAX];
    int eqs;
    bool list; /*eqs joined with commas*/
    bool as_is; /*kept away from the canonical form, the formula with the numbers just put in*/
    char move;      /*done to both sides on the way to the next line: + _ * / r, n for rules, or 0*/
    pcas_ast_t *by; /*the 5 of _5*/
    const char *rules[STEP_RULES_MAX]; /*move n: the rules used, "product rule"*/
} step_t;

typedef struct {
    step_t line[STEPS_MAX];
    int n;
} steps_t;

/*left = right worked out for var, false (and nothing in out) if it's a shape or method it doesn't do*/
bool steps_solve(pcas_ast_t *left, pcas_ast_t *right, Symbol var, Method m, steps_t *out);
/*just the check, for greying out the method menu*/
bool steps_fit(pcas_ast_t *left, pcas_ast_t *right, Symbol var, Method m);
/*d/dx(f) one rule at a time, every d/dx still left gets one per line, then the value plugged in if it's at one. d is the deriv node, false if a rule doesn't fit*/
bool steps_derivative(pcas_ast_t *d, steps_t *out);
/*lim(f) like on paper: plug in, or on 0/0 cancel / l'hopital first. l is the limit node, false if there isn't one*/
bool steps_limit(pcas_ast_t *l, steps_t *out);
void steps_free(steps_t *s);

/*for limit.c, which works its steps out in its own loop*/
step_t *steps_expr(steps_t *s, pcas_ast_t *e); /*a line that's just e, = the one above. takes e*/
void steps_rule(step_t *l, const char *name); /*name on l, move n. once each, two power rules on a line read as one*/
void steps_shown(steps_t *s); /*every tree in the canonical form, at the end. 3X+5 and not 5+3X*/

#endif
