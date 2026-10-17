#ifndef CAS_H_
#define CAS_H_

#include "../ast.h"
#include "derivative.h"

/*
    Changes ast to a form we work with in the simpilfier.

    Reduces every number node to numerator integer / denominator integer
    Changes all roots to powers.
    Expands (AB)^5 to A^5 * B^5

    Generally speaking, this should be included in every simplify() call
*/
#define SIMP_NORMALIZE                  (1u << 0u)
/*
    Flattens multiplication and addition nodes
    Also changes a multiplication or addition node with only one child
    into just that one child.
*/
#define SIMP_COMMUTATIVE                (1u << 1u)
/*
    Simplifies rational functions.
    A/B/C/D becomes A/(BCD)
*/
#define SIMP_RATIONAL                   (1u << 2u)
/*
    Simply executes eval() on the node which will evaluate all constant
    expressions like 5 + 5 to 10.
*/
#define SIMP_EVAL                       (1u << 3u)
/*
    Change A + A to 2A
    Change A*A to A^2
    Change A_A to 0
*/
#define SIMP_LIKE_TERMS                 (1u << 4u)
/*
    Change derivative nodes to their evaluated derivative
    sin(X) + deriv(X^2,X,2) becomes sin(X) + 4
*/
#define SIMP_DERIV                      (1u << 5u)
/*
    Simplifies inverses like sin(asin(X)) = X
    as well as log identities
*/
#define SIMP_ID_GENERAL                 (1u << 6u)
/*
    Simplifies trig identities like sin(X)^2 + cos(X)^2 = 1

    ANY SIMP_ID WILL AUTOMATICALLY SET THE EVAL FLAG, COMMUTATIVE FLAG, AND THE SIMPLIFY LIKE TERMS FLAG
    ANY SIMP_ID WILL ALSO CALL TO EXPAND() AND FACTOR()
*/
#define SIMP_ID_TRIG                    (1u << 7u)
/*
    Simplifies trig constants like sin(pi/4) = sqrt(2)/2

    ANY SIMP_ID WILL AUTOMATICALLY SET THE EVAL FLAG, COMMUTATIVE FLAG, AND THE SIMPLIFY LIKE TERMS FLAG
    ANY SIMP_ID WILL ALSO CALL TO EXPAND() AND FACTOR()
*/
#define SIMP_ID_TRIG_CONSTANTS          (1u << 8u)
/*
    Simplifies inverse trig constants like atan(sqrt(3)) = pi/3

    ANY SIMP_ID WILL AUTOMATICALLY SET THE EVAL FLAG, COMMUTATIVE FLAG, AND THE SIMPLIFY LIKE TERMS FLAG
    ANY SIMP_ID WILL ALSO CALL TO EXPAND() AND FACTOR()
*/
#define SIMP_ID_TRIG_INV_CONSTANTS      (1u << 9u)
/*
    Simplifies hyperbolic identities like sinh(X)/cosh(X) = tanh(X)

    ANY SIMP_ID WILL AUTOMATICALLY SET THE EVAL FLAG, COMMUTATIVE FLAG, AND THE SIMPLIFY LIKE TERMS FLAG
    ANY SIMP_ID WILL ALSO CALL TO EXPAND() AND FACTOR()
*/
#define SIMP_ID_HYPERBOLIC              (1u << 10u)
/*
    Simplifies complex functins like sin(z) and ln(z)

    ANY SIMP_ID WILL AUTOMATICALLY SET THE EVAL FLAG, COMMUTATIVE FLAG, AND THE SIMPLIFY LIKE TERMS FLAG
    ANY SIMP_ID WILL ALSO CALL TO EXPAND() AND FACTOR()
*/
#define SIMP_ID_COMPLEX                 (1u << 11u)
/*
    Simplify all identities

    ANY SIMP_ID WILL AUTOMATICALLY SET THE EVAL FLAG, COMMUTATIVE FLAG, AND THE SIMPLIFY LIKE TERMS FLAG
    ANY SIMP_ID WILL ALSO CALL TO EXPAND() AND FACTOR()
*/
#define SIMP_ID_ALL                     (SIMP_ID_GENERAL | SIMP_ID_TRIG | SIMP_ID_TRIG_CONSTANTS | SIMP_ID_TRIG_INV_CONSTANTS | SIMP_ID_HYPERBOLIC | SIMP_ID_COMPLEX)

#define SIMP_ALL                        (0xFFFFu)

/*
    Fluxion: cancelling. while it works, the engine calls pcas_poll (if not NULL)
    now and then; the caller can draw a busy sign there, and returns true to stop.
    then pcas_cancelled is set, every loop and entry point returns early, and the
    tree is left valid but unfinished: throw the answer away. the caller clears
    pcas_cancelled before the next run. pcas_check() polls and says whether to stop.
*/
extern bool (*pcas_poll)(void);
extern bool pcas_cancelled;
bool pcas_check(void);

/*Simplifies ast. Returns true if changed*/
bool simplify(pcas_ast_t *e, unsigned short flags);

/*
    Order multiplication and division.
    Change XAZ to AXZ and 1+sin(X)ZA5 to 5AZsin(X)+1
    Rationalize denominators.
    Change powers to roots.
    Combine a^5b^5 to (ab)^5.


*/
/*Order multiplication and division. Change XAZ to AXZ and 1+sin(X)ZA5 to 5AZsin(X)+1*/
#define CANONICAL_SORT                  (1u << 0u)
/*Rationalize denominators*/
#define CANONICAL_RATIONALIZE           (1u << 1u)
/*Change X^(1/2) to sqrt(X)*/
#define CANONICAL_POWERS_TO_ROOTS       (1u << 2u)
/*Combine a^5b^5 to (ab)^5.*/
#define CANONICAL_COMBINE_POWERS        (1u << 3u)
#define CANONICAL_ALL                   (0xFFu)
/*
    Use only when exporting and do not plan to simplify again
    or execute any other function on ast. Messes up all other algorithms.

    Sorting for addition and multiplication is insertion sort, but recursive so kinda expensive
*/
bool simplify_canonical_form(pcas_ast_t *e, unsigned char flags);

/*Factor A + A to A(1 + 1) where at least one part of the
resulting multiplication can be evauated numerically*/
#define FAC_SIMPLE_ADDITION_EVALUATEABLE    (1u << 0u)
/*Factor A^3+AX to A(A^2+X)*/
#define FAC_SIMPLE_ADDITION_NONEVALUATEABLE (1u << 1u)
/*Factor  x^2 - 1 to (x-1)(x+1)*/
#define FAC_POLYNOMIAL                      (1u << 2u)
#define FAC_ALL                             0xFF

bool factor(pcas_ast_t *e, unsigned char flags);

/*Fluxion: in every fraction, factors the top and bottom (FAC_POLYNOMIAL) and
cancels what they share: (X^2_1)/(X_1) = (X+1). call before simplify(), which
splits fractions apart.*/
bool cancel_fractions(pcas_ast_t *e);

/*Fluxion: every sum with a fraction in it becomes one fraction over the lcm of the
denominators, common factors cancelled: 1/X+1 = (X+1)/X. top and bottom expanded, or
factored if factored is true. call after simplify(), which would split it again.*/
bool comdenom(pcas_ast_t *e, bool factored);

/*EVAL_DISTRIBUTE_NUMBERS, EVAL_DISTRIBUTE_MULTIPLICATION, EVAL_DISTRIBUTE_ADDITION, EVAL_EXPAND_POWERS*/

/*Expand 2(A+B) to 2A + 2B or -(A+B) to -1*A + -1*B*/
#define EXP_DISTRIB_NUMBERS        (1u << 0u)
/*Expand any f(x)*g(x)*(A+B) to  f(x)g(x)A + f(x)g(x)B*/
#define EXP_DISTRIB_MULTIPLICATION (1u << 1u)
/*Expand (AB)/2 to (1/2)AB and (A+B)/2 to 1/2A + 1/2B*/
#define EXP_DISTRIB_DIVISION       (1u << 2u)
/*Expand (A+B)(C+D) to AC+AD+BC+BD */
#define EXP_DISTRIB_ADDITION       (1u << 3u)
/*Distribute (AB)^2 to A^2B^2 and (1/A)^2 to (1^2/A^2)*/
#define EXP_DISTRIB_POWERS         (1u << 4u)
/*Expand (A+B)^2 to A^2 + 2AB + B^2 */
#define EXP_EXPAND_POWERS          (1u << 5u)
#define EXP_COLLECT_TERMS          (1u << 6u) /*Fluxion: collects as it goes so (X+1)^n doesn't build 2^n terms first, see _expand*/
#define EXP_ALL                    0xFF

bool expand(pcas_ast_t *e, unsigned char flags);

/*Fluxion: (A+B)^n is only multiplied out up to this n*/
#define EXPAND_MAX_POWER 64

/*Fluxion: multiplies out the polynomial parts of e as flat coefficient rows
  (cas/poly.c). expand() calls it first, true if anything changed*/
bool poly_expand(pcas_ast_t *e);
/*Fluxion: top/bot by long division, the quotient only (new tree), *exact says the
  remainder was 0. both have to be polynomials in just var with number coefficients,
  NULL otherwise*/
pcas_ast_t *poly_quotient(pcas_ast_t *top, pcas_ast_t *bot, Symbol var, bool *exact);

/*Evaluates identities that are too basic to put in identities.c*/
#define EVAL_BASIC_IDENTITIES   (1u << 0u)
/*Evaluates addition and multiplication of constants*/
#define EVAL_COMMUTATIVE        (1u << 1u)
/*Evaluates 4/2 to 2 and 15/20 to 3/4*/
#define EVAL_DIVISION           (1u << 2u)
/*Evaluates a^b if a <= 10 and b <= 10*/
#define EVAL_POWERS_SMALL       (1u << 3u)
#define EVAL_POWERS_FULL        (1u << 4u)
/*Evaluates int(5.5) = 5*/
#define EVAL_INT                (1u << 5u)
/*Evaluates absolute value of strictly real constants*/
#define EVAL_ABS                (1u << 6u)
/*Evaluates X! where X <= 10*/
#define EVAL_FACTORIAL_SMALL    (1u << 7u)
#define EVAL_FACTORIAL_FULL     (1u << 8u)
/*Things that can be computed in a reasonable amount of time*/
#define EVAL_EASY               (EVAL_BASIC_IDENTITIES | EVAL_COMMUTATIVE | EVAL_DIVISION | EVAL_INT | EVAL_ABS | EVAL_POWERS_SMALL | EVAL_FACTORIAL_SMALL)
/*Things that take a lot of computation to compute*/
#define EVAL_HARD               (EVAL_POWERS_FULL | EVAL_FACTORIAL_FULL)
#define EVAL_ALL                0xFFFF

/*Evaluates constants such as 5+5 or 6^5. Also implements basic identities
such as 1A = A, A + 0 = A. Returns true if the ast was changed.*/
bool eval(pcas_ast_t *e, unsigned short flags);

/*
    Fluxion: solves e = 0 for var, over the reals (cas/solve.c).
    out is SOLVE_MAX pointers, all NULL. returns how many solutions it put there
    (0 = none), SOLVE_ALL if every value is one, or SOLVE_UNKNOWN if it can't solve
    it. caller frees the solutions.
*/
#define SOLVE_MAX       32
#define SOLVE_ALL       (-1)
#define SOLVE_UNKNOWN   (-2)
int solve(pcas_ast_t *e, Symbol var, pcas_ast_t **out);
/*Fluxion: solve() with the complex roots of quadratics too. the app wants them, asym and friends don't*/
int solve_ex(pcas_ast_t *e, Symbol var, pcas_ast_t **out, bool complex);

/*Fluxion: the trig families solve() gives (pi/6 + 2 pi n) turned into the ones
  from 0 up to hi, which is 2pi, or 360 in degrees. small integers go in for n
  and what lands in range stays, sorted. roots without an n pass through. the
  array is rewritten in place, returns the new count*/
int solve_in_range(pcas_ast_t **roots, int n, pcas_ast_t *hi);
/*Fluxion: e as a decimal, one NUMBER node (a new tree), NULL with a letter left.
what solve and system sort their roots by*/
pcas_ast_t *approx_number(pcas_ast_t *e);
/*Fluxion: is the symbol s anywhere in e*/
bool has_symbol(const pcas_ast_t *e, Symbol s);

/*
    Fluxion: the decimal form (cas/approx.c). every part without variables that
    isn't a number yet (pi, e, roots, ln, trig in radians, ...) becomes a number
    as close as a float (about 7 digits). exact numbers stay exact. follow with simplify() to
    combine them: 2sqrt(2)X -> 2.82843X.
*/
bool approx(pcas_ast_t *e);

/*Fluxion: r as a decimal with digits significant digits: 1.25, 0.333333, 2.28892E7.
plain from 0.001 up to 10^digits. buf needs digits + 10 chars*/
void approx_format(mp_rat r, int digits, char *buf);

/*Replaces all instances of from ast to to ast in e*/
bool substitute(pcas_ast_t *e, pcas_ast_t *from, pcas_ast_t *to);

/*Returns the greatest common divisor of two expressions.
gcd(X(X+1)^2, AX) = X 
gcd(6AX, 10X) = 2X*/
pcas_ast_t *gcd(pcas_ast_t *a, pcas_ast_t *b);

/*Fluxion: a/g already simplified, when both are a whole number times letters to whole powers,
g's number is positive and it divides. NULL otherwise. compare() uses it to skip a simplify per sorted pair*/
pcas_ast_t *gm_div(pcas_ast_t *a, pcas_ast_t *g);

/*Helper functions*/

/*Cleanup a. Copy a to b and cleanup b.*/
void replace_node(pcas_ast_t *a, pcas_ast_t *b);

/*Returns true if the node has an imaginary node.*/
bool has_imaginary_node(pcas_ast_t *e);

/*Returns true if the node is being multiplied by at least one negative or is already a negative number node.
The node must be completely simplified for this to work, because it does not detect
multiplying by more than one negative to make a positive. */
bool is_negative_for_sure(pcas_ast_t *a);

/*Returns true if changed. Expects completely simplified. Removes the negative in the multiplier or number.*/
bool absolute_val(pcas_ast_t *e);

/*Fluxion: degree mode (cas/degrees.c). gives back the tree to use, which is a new
root when the whole thing was an inverse trig*/
pcas_ast_t *degrees_in(pcas_ast_t *e);
/*Fluxion: after simplify, takes the pi/180 back out of trig arguments that stayed symbolic*/
void degrees_out(pcas_ast_t *e);

/*Fluxion: holes and asymptotes of f in var (cas/asym.c). holes and vert are the
  x values, sorted, horiz is the y of the horizontal one or NULL for none. false
  when a zero of the bottom couldn't be found, then out is empty. slant is the y
  of the slant one (top one degree higher), NULL for none, horiz is NULL then*/
typedef struct {
    pcas_ast_t *holes[SOLVE_MAX], *vert[SOLVE_MAX], *horiz, *slant;
    int nholes, nvert;
} asym_t;
bool asym(pcas_ast_t *f, Symbol var, asym_t *out);
void asym_free(asym_t *a);

/*Fluxion: where f is defined in var, over the reals (cas/domain.c). lower and
  upper are the bounds (NULL for none, open = the bound itself is out), holes are
  the single x values out on top of that, smallest first, tan's as pi/2 + pi n.
  partial when a root or log had something in it that isn't linear, there may be
  more out than this. empty when the bounds cross (or meet with one open), no x at
  all, the bounds stay and the holes go. false when nothing at all is defined
  (1/(x-x)) or cancelled*/
typedef struct {
    pcas_ast_t *holes[SOLVE_MAX];
    int nholes;
    pcas_ast_t *lower, *upper;
    bool lower_open, upper_open;
    bool partial;
    bool empty;
} domain_t;
bool domain(pcas_ast_t *f, Symbol var, domain_t *out);
void domain_free(domain_t *d);

/*Fluxion: systems (cas/system.c). n expressions that are = 0, in the n vars.
  linear ones by elimination, two nonlinear ones in two letters by substitution,
  real only. out[k][i] gets the value of vars[i] in solution set k (all NULL going
  in). returns how many sets (a linear one has 1), sorted by the first var, or 0
  for no solution, SOLVE_ALL when there's infinitely many, SOLVE_UNKNOWN when it
  can't solve those or something gave up*/
#define SYSTEM_MAX 4
#define SYSTEM_SETS 6
int solve_system(pcas_ast_t **eqs, int n, const Symbol *vars, pcas_ast_t *out[][SYSTEM_MAX]);

/*Fluxion: degree of e (simplified and expanded) as a polynomial in var, -1 if it
  isn't one. the leading coefficient goes in *lead if that's not NULL (new tree)*/
int poly_degree(pcas_ast_t *e, Symbol var, pcas_ast_t **lead);

#endif