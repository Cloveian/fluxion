#include "catalog.h"

#include <stddef.h>

/*
    keys are a proposal (input.md left the mapping for later): each key keeps the
    meaning it has on the TI-84 itself, so nothing needs learning. [on] chords for
    select / copy / paste come later.

    engine syntax reminders: capital single-letter variables, _ is subtract,
    - is negate, $ marks a slot the editor fills in.

    word is what you type to get the item: a run of letters in the editor gets split
    into the longest words from the left (see names.c), so typing s i n gets you sin.
*/

#define ITEM(name, label, word, kind, keys, engine, have, note, help) \
    static const item_t name = { label, word, kind, keys, engine, have, note, help };

/* stuff you type straight from the keypad */

/* numbers */
ITEM(it_digits,   "0-9",   NULL, ITEM_CHAR, "[0]-[9]",  "0-9",   HAVE_YES, NULL,
     "the digits. answers stay exact, so 1/3 stays 1/3")
ITEM(it_point,    ".",     NULL, ITEM_CHAR, "[.]",      ".",     HAVE_YES, NULL,
     "decimal point, for numbers like 2.5")
ITEM(it_negate,   "(-)",   NULL, ITEM_CHAR, "[(-)]",    "-",     HAVE_YES, "negate, not subtract",
     "negative sign, the (-) key. not the same as subtract")
ITEM(it_sci,      "E",     NULL, ITEM_CHAR, "[2nd][,]", "[E]",   HAVE_YES, "scientific: 2E3",
     "times ten to a power, for very big or small numbers. 2E3 -> 2000")

/* operators */
ITEM(it_plus,     "+",     NULL, ITEM_CHAR, "[+]",      "+",     HAVE_YES, NULL,
     "adds. 2+3 -> 5")
ITEM(it_minus,    "-",     NULL, ITEM_CHAR, "[-]",      "_",     HAVE_YES, NULL,
     "subtracts. 5-3 -> 2. for a negative number use the (-) key instead")
ITEM(it_times,    "x",     NULL, ITEM_CHAR, "[x]",      "*",     HAVE_YES, "shown as nothing, or a dot between numbers",
     "multiplies. 2*x shows as 2x; between two numbers it shows as a dot")
ITEM(it_divide,   "/",     NULL, ITEM_CHAR, "[/]",      "/",     HAVE_YES, "typed inline, shown stacked (Nspire)",
     "divides. you type it in a line and it is drawn as a stacked fraction. 6/4 -> 3/2")
ITEM(it_open,     "(",     NULL, ITEM_CHAR, "[(]",      "(",     HAVE_YES, NULL,
     "opening parenthesis, to group things. 2(x+1)")
ITEM(it_close,    ")",     NULL, ITEM_CHAR, "[)]",      ")",     HAVE_YES, NULL,
     "closing parenthesis, ends a group or a function's input")
ITEM(it_comma,    ",",     NULL, ITEM_CHAR, "[,]",      ",",     HAVE_YES, "between arguments: logb(x,2)",
     "separates the inputs of a function, like solve(2x=6,x)")
ITEM(it_equals,   "=",     NULL, ITEM_CHAR, "[2nd][math]", "=", HAVE_PARTLY, "only in solve(: the engine has no equations, the app splits the line",
     "equals sign, [2nd][math]. only works inside solve. solve(2x=6,x) -> x = 3")

/* variables and constants */
ITEM(it_x,        "x",     NULL, ITEM_CHAR, "[x,t,theta,n]", "X", HAVE_YES, "shown lowercase",
     "the variable x, on its own key: [x,t,theta,n]")
ITEM(it_letters,  "a-z",   NULL, ITEM_CHAR, "[alpha] + key", "A-Z", HAVE_YES, "shown lowercase, one letter each",
     "variables, one letter each. [alpha] + a key types the letter above it")
ITEM(it_pi,       "pi",    "pi", ITEM_CHAR, "[2nd][^]",  "pi",   HAVE_YES, NULL,
     "the number pi = 3.14159..., kept exact. sin(pi/2) -> 1")
ITEM(it_e,        "e",     "e", ITEM_CHAR, "[2nd][/]",  "e",    HAVE_YES, "the constant, drawn bold",
     "the number e = 2.71828..., drawn bold so it isn't the variable e. ln(e) -> 1")
ITEM(it_i,        "i",     "i", ITEM_CHAR, "[2nd][.]",  "i",    HAVE_YES, "the constant, drawn bold",
     "the imaginary unit, i^2 = -1. drawn bold so it isn't the variable i")
ITEM(it_theta,    "theta", "theta", ITEM_CHAR, NULL,        "theta", HAVE_YES, NULL,
     "the variable theta, often used for angles. type theta, or [alpha][3]")

/* powers and roots */
ITEM(it_power,    "x^n",   NULL, ITEM_TEMPLATE, "[^]",       "($)^($)",   HAVE_YES, "cursor jumps into the exponent",
     "raises to a power; the cursor moves into the exponent. 2^5 -> 32")
ITEM(it_square,   "x^2",   NULL, ITEM_CHAR,     "[x^2]",     "^2",        HAVE_YES, NULL,
     "squares what comes before it. 3^2 -> 9")
ITEM(it_recip,    "x^-1",  NULL, ITEM_CHAR,     "[x^-1]",    "^(-1)",     HAVE_YES, NULL,
     "one over what comes before it. 4^-1 -> 1/4")
ITEM(it_sqrt,     "sqrt",  "sqrt", ITEM_TEMPLATE, "[2nd][x^2]", "sqrt($)",  HAVE_YES, NULL,
     "square root. sqrt(8) -> 2sqrt(2)")
ITEM(it_cbrt,     "cbrt",  "cbrt", ITEM_TEMPLATE, NULL,        "($)root($)", HAVE_YES, "3root: ROOT with index 3",
     "cube root. cbrt(27) -> 3")
ITEM(it_nroot,    "n-root", NULL, ITEM_TEMPLATE, NULL,       "($)root($)", HAVE_YES, "index first",
     "root with any index; type the index first. the 4th root of 16 -> 2")

/* functions with their own key */
ITEM(it_sin,      "sin",   "sin", ITEM_FUNCTION, "[sin]",       "sin($)",  HAVE_YES, NULL,
     "sine of an angle, in radians. sin(pi/6) -> 1/2")
ITEM(it_cos,      "cos",   "cos", ITEM_FUNCTION, "[cos]",       "cos($)",  HAVE_YES, NULL,
     "cosine of an angle, in radians. cos(pi/3) -> 1/2")
ITEM(it_tan,      "tan",   "tan", ITEM_FUNCTION, "[tan]",       "tan($)",  HAVE_YES, NULL,
     "tangent of an angle, in radians. tan(pi/4) -> 1")
ITEM(it_asin,     "sin^-1", "asin", ITEM_FUNCTION, "[2nd][sin]", "asin($)", HAVE_YES, NULL,
     "inverse sine: the angle with this sine, in radians. sin^-1(1/2) -> pi/6")
ITEM(it_acos,     "cos^-1", "acos", ITEM_FUNCTION, "[2nd][cos]", "acos($)", HAVE_YES, NULL,
     "inverse cosine: the angle with this cosine, in radians. cos^-1(0) -> pi/2")
ITEM(it_atan,     "tan^-1", "atan", ITEM_FUNCTION, "[2nd][tan]", "atan($)", HAVE_YES, NULL,
     "inverse tangent: the angle with this tangent, in radians. tan^-1(1) -> pi/4")
ITEM(it_cot,      "cot",   "cot", ITEM_FUNCTION, "[on][tan]",   "cot($)",  HAVE_YES, NULL,
     "cotangent, 1/tan. cot(pi/4) -> 1")
ITEM(it_sec,      "sec",   "sec", ITEM_FUNCTION, "[on][cos]",   "sec($)",  HAVE_YES, NULL,
     "secant, 1/cos. sec(0) -> 1")
ITEM(it_csc,      "csc",   "csc", ITEM_FUNCTION, "[on][sin]",   "csc($)",  HAVE_YES, NULL,
     "cosecant, 1/sin. csc(pi/2) -> 1")
ITEM(it_ln,       "ln",    "ln", ITEM_FUNCTION, "[ln]",        "ln($)",   HAVE_YES, NULL,
     "natural logarithm, base e. ln(e^2) -> 2")
ITEM(it_exp,      "e^x",   NULL, ITEM_TEMPLATE, "[2nd][ln]",   "e^($)",   HAVE_YES, NULL,
     "e raised to a power, the opposite of ln. e^0 -> 1")
ITEM(it_log,      "log",   "log", ITEM_FUNCTION, "[log]",       "log($)",  HAVE_YES, "base 10",
     "logarithm base 10. log(1000) -> 3")
ITEM(it_pow10,    "10^x",  NULL, ITEM_TEMPLATE, "[2nd][log]",  "10^($)",  HAVE_YES, NULL,
     "10 raised to a power, the opposite of log. 10^3 -> 1000")

/* folder-only (no direct key) */

/* Functions */
ITEM(it_sinh,     "sinh",  "sinh", ITEM_FUNCTION, NULL, "sinh($)",  HAVE_YES, NULL,
     "hyperbolic sine, (e^x-e^-x)/2. sinh(0) -> 0")
ITEM(it_cosh,     "cosh",  "cosh", ITEM_FUNCTION, NULL, "cosh($)",  HAVE_YES, NULL,
     "hyperbolic cosine, (e^x+e^-x)/2. cosh(0) -> 1")
ITEM(it_tanh,     "tanh",  "tanh", ITEM_FUNCTION, NULL, "tanh($)",  HAVE_YES, NULL,
     "hyperbolic tangent, sinh(x)/cosh(x). tanh(0) -> 0")
ITEM(it_asinh,    "sinh^-1", "asinh", ITEM_FUNCTION, NULL, "asinh($)", HAVE_YES, NULL,
     "inverse hyperbolic sine. sinh^-1(0) -> 0")
ITEM(it_acosh,    "cosh^-1", "acosh", ITEM_FUNCTION, NULL, "acosh($)", HAVE_YES, NULL,
     "inverse hyperbolic cosine. cosh^-1(1) -> 0")
ITEM(it_atanh,    "tanh^-1", "atanh", ITEM_FUNCTION, NULL, "atanh($)", HAVE_YES, NULL,
     "inverse hyperbolic tangent. tanh^-1(0) -> 0")
ITEM(it_logb,     "log_b", "logb", ITEM_TEMPLATE, NULL, "logb($,$)", HAVE_YES, "argument first, then base",
     "logarithm with any base. log_2(8) -> 3")
ITEM(it_abs,      "|x|",   "abs", ITEM_TEMPLATE, NULL, "abs($)",   HAVE_YES, NULL,
     "absolute value, the distance from 0. |-5| -> 5")
ITEM(it_fact,     "!",     NULL, ITEM_CHAR,     NULL, "!",        HAVE_YES, NULL,
     "factorial, multiplies 1 up to n. 5! -> 120")
ITEM(it_int,      "int",   "int", ITEM_FUNCTION, NULL, "int($)",   HAVE_YES, "floor, like the TI's int(",
     "rounds down to a whole number, like the TI's int(. int(7/2) -> 3")

/* Templates */
ITEM(it_frac,     "n/d",   NULL, ITEM_TEMPLATE, "[alpha][y=]", "($)/($)", HAVE_YES, "empty fraction, cursor in the top",
     "an empty fraction. type the top, then move down to type the bottom")
ITEM(it_parens,   "( )",   NULL, ITEM_TEMPLATE, NULL, "($)",       HAVE_YES, NULL,
     "A pair of parentheses with the cursor inside, to group things")
ITEM(it_piecewise, "piecewise", NULL, ITEM_TEMPLATE, NULL, NULL,   HAVE_NO, "no conditions in the engine",
     "A function with a different rule for different values of x. not in Fluxion yet")

/* Algebra (commands) */
ITEM(it_simplify, "simplify", NULL, ITEM_COMMAND, "[enter]", NULL, HAVE_YES,
     "always runs on enter: simplify(SIMP_ALL) + canonical form",
     "runs on [enter]: simplifies whatever is on the line")
ITEM(it_expand,   "expand", "expand", ITEM_COMMAND, NULL, "expand($)", HAVE_YES, "expand() with EXP_ALL",
     "multiplies everything out. expand((x+1)^2) -> x^2+2x+1")
ITEM(it_subst,    "substitute", "subst", ITEM_COMMAND, NULL, "subst($,$,$)", HAVE_PARTLY,
     "substitute() exists, but the parser has no syntax for it: Fluxion handles it",
     "puts a value or expression in place of a variable. not in Fluxion yet")
ITEM(it_evaluate, "evaluate", "eval", ITEM_COMMAND, NULL, "eval($)", HAVE_PARTLY,
     "exact only (big powers, factorials); no decimal approximation yet",
     "works out the exact value. eval(2^10) -> 1024. no decimal answers yet")
ITEM(it_factor,   "factor", "factor", ITEM_COMMAND, NULL, "factor($)", HAVE_PARTLY,
     "one variable over the rationals; not x^2-y^2, not irrational roots (factor(x^2-2,x))",
     "writes it as a product. factor(x^2-1) -> (x-1)(x+1). one variable only")
ITEM(it_solve,    "solve",  "solve", ITEM_COMMAND, NULL, "solve($)", HAVE_PARTLY,
     "solve(eq,x): linear (any coefficients), polynomials via factor + the quadratic formula; no systems",
     "solves linear and polynomial equations. solve(x^2-4=0,x) -> x = -2, 2. no systems")

/* Calculus */
ITEM(it_deriv,    "d/dx",   "deriv", ITEM_TEMPLATE, NULL, "deriv($,X,X)", HAVE_YES, "derivative with respect to x",
     "derivative with respect to x. d/dx(x^3) -> 3x^2")
ITEM(it_deriv_at, "d/dx at", NULL, ITEM_TEMPLATE, NULL, "deriv($,X,$)", HAVE_YES, "derivative evaluated at a value",
     "derivative at one value of x. d/dx(x^2) at x=3 -> 6")
ITEM(it_integral, "integral", NULL, ITEM_TEMPLATE, NULL, NULL, HAVE_NO, "table + linearity + u-substitution",
     "integral, the area under a curve. not in Fluxion yet")
ITEM(it_limit,    "limit",  NULL, ITEM_TEMPLATE, NULL, NULL, HAVE_NO, NULL,
     "the value a function gets close to as x gets close to a number. not in Fluxion yet")

/* Tools */
ITEM(it_id_basic, "basic identities", NULL, ITEM_SETTING, NULL, NULL, HAVE_YES, "SIMP_ID_GENERAL", NULL)
ITEM(it_id_trig,  "trig identities",  NULL, ITEM_SETTING, NULL, NULL, HAVE_YES, "SIMP_ID_TRIG", NULL)
ITEM(it_id_hyp,   "hyperbolic identities", NULL, ITEM_SETTING, NULL, NULL, HAVE_YES, "SIMP_ID_HYPERBOLIC", NULL)
ITEM(it_id_cplx,  "complex identities", NULL, ITEM_SETTING, NULL, NULL, HAVE_YES, "SIMP_ID_COMPLEX", NULL)
ITEM(it_id_tconst, "trig constants",  NULL, ITEM_SETTING, NULL, NULL, HAVE_YES, "SIMP_ID_TRIG_CONSTANTS", NULL)
ITEM(it_id_iconst, "inverse trig constants", NULL, ITEM_SETTING, NULL, NULL, HAVE_YES, "SIMP_ID_TRIG_INV_CONSTANTS", NULL)
ITEM(it_rationalize, "rationalize", NULL, ITEM_SETTING, NULL, NULL, HAVE_YES,
     "CANONICAL_RATIONALIZE; off by default is still undecided", NULL)
ITEM(it_clear,    "clear history", NULL, ITEM_SETTING, NULL, NULL, HAVE_YES, NULL, NULL)

/* the lists */

const item_t *const catalog[] = {
    &it_digits, &it_point, &it_negate, &it_sci,
    &it_plus, &it_minus, &it_times, &it_divide, &it_open, &it_close, &it_comma, &it_equals,
    &it_x, &it_letters, &it_pi, &it_e, &it_i, &it_theta,
    &it_power, &it_square, &it_recip, &it_sqrt, &it_cbrt, &it_nroot,
    &it_sin, &it_cos, &it_tan, &it_asin, &it_acos, &it_atan, &it_cot, &it_sec, &it_csc, &it_ln, &it_exp, &it_log, &it_pow10,
    &it_sinh, &it_cosh, &it_tanh, &it_asinh, &it_acosh, &it_atanh, &it_logb, &it_abs, &it_fact, &it_int,
    &it_frac, &it_parens, &it_piecewise,
    &it_simplify, &it_expand, &it_subst, &it_evaluate, &it_factor, &it_solve,
    &it_deriv, &it_deriv_at, &it_integral, &it_limit,
    &it_id_basic, &it_id_trig, &it_id_hyp, &it_id_cplx, &it_id_tconst, &it_id_iconst, &it_rationalize, &it_clear
};
const unsigned catalog_count = sizeof catalog / sizeof catalog[0];

static const item_t *const algebra[] = {
    &it_expand, &it_factor, &it_solve, &it_subst, &it_evaluate
};
static const item_t *const calculus[] = {
    &it_deriv, &it_deriv_at, &it_integral, &it_limit
};
static const item_t *const functions[] = {
    &it_sin, &it_cos, &it_tan, &it_asin, &it_acos, &it_atan, &it_cot, &it_sec, &it_csc,
    &it_sinh, &it_cosh, &it_tanh, &it_asinh, &it_acosh, &it_atanh,
    &it_ln, &it_log, &it_logb, &it_exp, &it_pow10, &it_abs, &it_fact, &it_int
};
static const item_t *const templates[] = {
    &it_frac, &it_power, &it_sqrt, &it_cbrt, &it_nroot, &it_abs, &it_logb, &it_parens,
    &it_pi, &it_e, &it_i, &it_theta, &it_piecewise
};
static const item_t *const tools[] = {
    &it_id_basic, &it_id_trig, &it_id_hyp, &it_id_cplx, &it_id_tconst, &it_id_iconst,
    &it_clear
};

#define FOLDER(key, name, list) { key, name, list, sizeof list / sizeof list[0] }

const folder_t folders[FOLDER_COUNT] = {
    FOLDER("y=",     "Algebra",   algebra),
    FOLDER("window", "Calculus",  calculus),
    FOLDER("zoom",   "Functions", functions),
    FOLDER("trace",  "Templates", templates),
    FOLDER("graph",  "Tools",     tools)
};
