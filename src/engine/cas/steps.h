/*Fluxion: show the work, worked out again like on paper. not in upstream PineappleCAS.*/

#ifndef STEPS_H_
#define STEPS_H_

#include "cas.h"

#define STEP_EQS_MAX 4  /*x_2=0 or x+3=0, a=1, b=6, c=1, and the zeros or pieces of an inequality*/
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
    pcas_ast_t *low; /*-2 < x < 3: low is the -2, x is left, 3 is right. NULL for the usual*/
    char rel, low_rel; /*between left and right, and low and left: < > l (<=) g (>=), 0 is =*/
} step_eq_t;

typedef struct {
    step_eq_t eq[STEP_EQS_MAX];
    int eqs;
    bool list; /*eqs joined with commas*/
    bool as_is; /*kept away from the canonical form, the formula with the numbers just put in*/
    char move;      /*done to both sides on the way to the next line: + _ * / r, n for rules, or 0*/
    pcas_ast_t *by; /*the 5 of _5*/
    bool flip; /*the move turned the sign round: dividing a < by a negative*/
    const char *rules[STEP_RULES_MAX]; /*move n: the rules used, "product rule"*/
} step_t;

typedef struct {
    step_t line[STEPS_MAX];
    int n;
} steps_t;

/*left = right worked out for var, false (and nothing in out) if it's a shape or method it doesn't do*/
bool steps_solve(pcas_ast_t *left, pcas_ast_t *right, Symbol var, Method m, steps_t *out);
/*left = right a layer a line (layers.c): sqrt(x+3) = 5, 2^x = 8, abs(x) = 4, ln(x) = 2. ends on what solve gives, false if it can't*/
bool steps_layers(pcas_ast_t *left, pcas_ast_t *right, Symbol var, steps_t *out);
/*n linear equations (each = 0) in n letters, solved by eliminatoin a step at a time: every line is the whole system. false if a coefficient isn't a number, or it has no one answer*/
bool steps_system(pcas_ast_t **eqs, int n, const Symbol *vars, steps_t *out);
/*expand( a product with a sum in it at a time, then the powers and like terms. false when there's nothing to open*/
bool steps_expand(pcas_ast_t *e, steps_t *out);
/*factor( on a polynomial in one letter: its own factoring lines up to the product, no = 0*/
bool steps_factor(pcas_ast_t *e, steps_t *out);
/*tangent(f, a), critical(f), extrema(f) or inflect(f) worked out in var: the derivative steps, then the equation steps for where it's 0. a is the point for the tangent, NULL for the rest*/
enum { STEPS_TANGENT, STEPS_CRITICAL, STEPS_EXTREMA, STEPS_INFLECT };
bool steps_command(int kind, pcas_ast_t *f, pcas_ast_t *a, Symbol var, steps_t *out);
/*t's lines on the end of s. t's first line is the one s ends on, so only its move comes over. false (t freed) when they don't fit*/
bool steps_append(steps_t *s, steps_t *t);
/*a polynomial past x^2 that factors into lines and quadratics: factor, each factor = 0, each solved (polysteps.c). false if it doesn't*/
bool steps_poly(pcas_ast_t *left, pcas_ast_t *right, Symbol var, steps_t *out);
/*ax+b = cx+d the way steps_solve does it, false when a side isn't a line in var. steps_ineq's linear ones*/
bool steps_linear(pcas_ast_t *left, pcas_ast_t *right, Symbol var, steps_t *out);
/*left rel right, rel is < > l (<=) or g (>=). a line works the sign along, the rest by a sign chart: the zeros, a test point in each gap, the pieces. false if it's all reals, none, or a shape it can't read*/
bool steps_ineq(pcas_ast_t *left, pcas_ast_t *right, char rel, Symbol var, steps_t *out);
/*just the check, for greying out the method menu*/
bool steps_fit(pcas_ast_t *left, pcas_ast_t *right, Symbol var, Method m);
/*d/dx(f) one rule at a time, every d/dx still left gets one per line, then the value plugged in if it's at one. d is the deriv node, false if a rule doesn't fit*/
bool steps_derivative(pcas_ast_t *d, steps_t *out);
/*lim(f) like on paper: plug in, or on 0/0 cancel / l'hopital first. l is the limit node, false if there isn't one*/
bool steps_limit(pcas_ast_t *l, steps_t *out);
/*∫f dx a rule a line like the d/dx ones, ending on + C or F(b) - F(a). false if a piece has no rule*/
bool steps_integral(pcas_ast_t *I, steps_t *out);
/*whichever of the three e is, deriv, limit or integral*/
bool steps_calc(pcas_ast_t *e, steps_t *out);
void steps_free(steps_t *s);
void steps_cut(steps_t *s, int n); /*frees the lines from n on, n lines left*/

/*for limit.c and integrate.c, which work their steps out in their own loops*/
#define STEPS_FOUND_MAX 16
#define SIMP_LIGHT (SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_RATIONAL | SIMP_EVAL)
int steps_find(pcas_ast_t *e, OperatorType op, pcas_ast_t **d, int n); /*the outermost op nodes in e into d, n so far*/
step_t *steps_expr(steps_t *s, pcas_ast_t *e); /*a line that's just e, = the one above. takes e*/
step_t *steps_eq(steps_t *s, pcas_ast_t *left, pcas_ast_t *right); /*left = right, takes both. u = x^2*/
pcas_ast_t *steps_number(mp_rat r); /*3 or 3/2 as written, a fraction is top over bottom*/
pcas_ast_t *sqrt_of(pcas_ast_t *v); /*v^(1/2) as written, takes v*/
void steps_rule(step_t *l, const char *name); /*name on l, move n. once each, two power rules on a line read as one*/
void steps_shown(steps_t *s); /*every tree in the canonical form, at the end. 3X+5 and not 5+3X*/

#endif
