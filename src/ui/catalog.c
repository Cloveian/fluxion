#include "catalog.h"

#include <stddef.h>

/*
    keys are a proposal (input.md left the mapping for later): each key keeps the
    meaning it has on the TI-84 itself, so nothing needs learning. [on] chords for
    select / copy / paste come later.

    engine syntax reminders: capital single-letter variables, _ is subtract,
    - is negate, $ marks a slot the editor fills in.
*/

#define ITEM(name, label, kind, keys, engine, have, note) \
    static const item_t name = { label, kind, keys, engine, have, note };

/* stuff you type straight from the keypad */

/* numbers */
ITEM(it_digits,   "0-9",   ITEM_CHAR, "[0]-[9]",  "0-9",   HAVE_YES, NULL)
ITEM(it_point,    ".",     ITEM_CHAR, "[.]",      ".",     HAVE_YES, NULL)
ITEM(it_negate,   "(-)",   ITEM_CHAR, "[(-)]",    "-",     HAVE_YES, "negate, not subtract")
ITEM(it_sci,      "E",     ITEM_CHAR, "[2nd][,]", "[E]",   HAVE_YES, "scientific: 2E3")

/* operators */
ITEM(it_plus,     "+",     ITEM_CHAR, "[+]",      "+",     HAVE_YES, NULL)
ITEM(it_minus,    "-",     ITEM_CHAR, "[-]",      "_",     HAVE_YES, NULL)
ITEM(it_times,    "x",     ITEM_CHAR, "[x]",      "*",     HAVE_YES, "shown as nothing, or a dot between numbers")
ITEM(it_divide,   "/",     ITEM_CHAR, "[/]",      "/",     HAVE_YES, "typed inline, shown stacked (Nspire)")
ITEM(it_open,     "(",     ITEM_CHAR, "[(]",      "(",     HAVE_YES, NULL)
ITEM(it_close,    ")",     ITEM_CHAR, "[)]",      ")",     HAVE_YES, NULL)
ITEM(it_comma,    ",",     ITEM_CHAR, "[,]",      ",",     HAVE_YES, "between arguments: logb(x,2)")
ITEM(it_equals,   "=",     ITEM_CHAR, "[2nd][math] =", "=", HAVE_NO, "the engine has no equations; needed for solve")

/* variables and constants */
ITEM(it_x,        "x",     ITEM_CHAR, "[x,t,theta,n]", "X", HAVE_YES, "shown lowercase")
ITEM(it_letters,  "a-z",   ITEM_CHAR, "[alpha] + key", "A-Z", HAVE_YES, "shown lowercase, one letter each")
ITEM(it_pi,       "pi",    ITEM_CHAR, "[2nd][^]",  "pi",   HAVE_YES, NULL)
ITEM(it_e,        "e",     ITEM_CHAR, "[2nd][/]",  "e",    HAVE_YES, "the constant, drawn bold")
ITEM(it_i,        "i",     ITEM_CHAR, "[2nd][.]",  "i",    HAVE_YES, "the constant, drawn bold")
ITEM(it_theta,    "theta", ITEM_CHAR, NULL,        "theta", HAVE_YES, NULL)

/* powers and roots */
ITEM(it_power,    "x^n",   ITEM_TEMPLATE, "[^]",       "($)^($)",   HAVE_YES, "cursor jumps into the exponent")
ITEM(it_square,   "x^2",   ITEM_CHAR,     "[x^2]",     "^2",        HAVE_YES, NULL)
ITEM(it_recip,    "x^-1",  ITEM_CHAR,     "[x^-1]",    "^(-1)",     HAVE_YES, NULL)
ITEM(it_sqrt,     "sqrt",  ITEM_TEMPLATE, "[2nd][x^2]", "sqrt($)",  HAVE_YES, NULL)
ITEM(it_cbrt,     "cbrt",  ITEM_TEMPLATE, NULL,        "($)root($)", HAVE_YES, "3root: ROOT with index 3")
ITEM(it_nroot,    "n-root", ITEM_TEMPLATE, NULL,       "($)root($)", HAVE_YES, "index first")

/* functions with their own key */
ITEM(it_sin,      "sin",   ITEM_FUNCTION, "[sin]",       "sin($)",  HAVE_YES, NULL)
ITEM(it_cos,      "cos",   ITEM_FUNCTION, "[cos]",       "cos($)",  HAVE_YES, NULL)
ITEM(it_tan,      "tan",   ITEM_FUNCTION, "[tan]",       "tan($)",  HAVE_YES, NULL)
ITEM(it_asin,     "sin^-1", ITEM_FUNCTION, "[2nd][sin]", "asin($)", HAVE_YES, NULL)
ITEM(it_acos,     "cos^-1", ITEM_FUNCTION, "[2nd][cos]", "acos($)", HAVE_YES, NULL)
ITEM(it_atan,     "tan^-1", ITEM_FUNCTION, "[2nd][tan]", "atan($)", HAVE_YES, NULL)
ITEM(it_ln,       "ln",    ITEM_FUNCTION, "[ln]",        "ln($)",   HAVE_YES, NULL)
ITEM(it_exp,      "e^x",   ITEM_TEMPLATE, "[2nd][ln]",   "e^($)",   HAVE_YES, NULL)
ITEM(it_log,      "log",   ITEM_FUNCTION, "[log]",       "log($)",  HAVE_YES, "base 10")
ITEM(it_pow10,    "10^x",  ITEM_TEMPLATE, "[2nd][log]",  "10^($)",  HAVE_YES, NULL)

/* folder-only (no direct key) */

/* Functions */
ITEM(it_sinh,     "sinh",  ITEM_FUNCTION, NULL, "sinh($)",  HAVE_YES, NULL)
ITEM(it_cosh,     "cosh",  ITEM_FUNCTION, NULL, "cosh($)",  HAVE_YES, NULL)
ITEM(it_tanh,     "tanh",  ITEM_FUNCTION, NULL, "tanh($)",  HAVE_YES, NULL)
ITEM(it_asinh,    "sinh^-1", ITEM_FUNCTION, NULL, "asinh($)", HAVE_YES, NULL)
ITEM(it_acosh,    "cosh^-1", ITEM_FUNCTION, NULL, "acosh($)", HAVE_YES, NULL)
ITEM(it_atanh,    "tanh^-1", ITEM_FUNCTION, NULL, "atanh($)", HAVE_YES, NULL)
ITEM(it_logb,     "log_b", ITEM_TEMPLATE, NULL, "logb($,$)", HAVE_YES, "argument first, then base")
ITEM(it_abs,      "|x|",   ITEM_TEMPLATE, NULL, "abs($)",   HAVE_YES, NULL)
ITEM(it_fact,     "!",     ITEM_CHAR,     NULL, "!",        HAVE_YES, NULL)
ITEM(it_int,      "int",   ITEM_FUNCTION, NULL, "int($)",   HAVE_YES, "floor, like the TI's int(")

/* Templates */
ITEM(it_frac,     "n/d",   ITEM_TEMPLATE, "[alpha][y=]", "($)/($)", HAVE_YES, "empty fraction, cursor in the top")
ITEM(it_parens,   "( )",   ITEM_TEMPLATE, NULL, "($)",       HAVE_YES, NULL)
ITEM(it_piecewise, "piecewise", ITEM_TEMPLATE, NULL, NULL,   HAVE_NO, "no conditions in the engine")

/* Algebra (commands) */
ITEM(it_simplify, "simplify", ITEM_COMMAND, "[enter]", NULL, HAVE_YES,
     "always runs on enter: simplify(SIMP_ALL) + canonical form")
ITEM(it_expand,   "expand", ITEM_COMMAND, NULL, "expand($)", HAVE_YES, "expand() with EXP_ALL")
ITEM(it_subst,    "substitute", ITEM_COMMAND, NULL, "subst($,$,$)", HAVE_PARTLY,
     "substitute() exists, but the parser has no syntax for it: Fluxion handles it")
ITEM(it_evaluate, "evaluate", ITEM_COMMAND, NULL, "eval($)", HAVE_PARTLY,
     "exact only (big powers, factorials); no decimal approximation yet")
ITEM(it_factor,   "factor", ITEM_COMMAND, NULL, "factor($)", HAVE_PARTLY,
     "one variable over the rationals; not x^2-y^2, not irrational roots (factor(x^2-2,x))")
ITEM(it_solve,    "solve",  ITEM_COMMAND, NULL, "solve($,$)", HAVE_NO,
     "needs =, then: linear first, polynomial via factor")

/* Calculus */
ITEM(it_deriv,    "d/dx",   ITEM_TEMPLATE, NULL, "deriv($,X,X)", HAVE_YES, "derivative with respect to x")
ITEM(it_deriv_at, "d/dx at", ITEM_TEMPLATE, NULL, "deriv($,X,$)", HAVE_YES, "derivative evaluated at a value")
ITEM(it_integral, "integral", ITEM_TEMPLATE, NULL, NULL, HAVE_NO, "table + linearity + u-substitution")
ITEM(it_limit,    "limit",  ITEM_TEMPLATE, NULL, NULL, HAVE_NO, NULL)

/* Tools */
ITEM(it_id_basic, "basic identities", ITEM_SETTING, NULL, NULL, HAVE_YES, "SIMP_ID_GENERAL")
ITEM(it_id_trig,  "trig identities",  ITEM_SETTING, NULL, NULL, HAVE_YES, "SIMP_ID_TRIG")
ITEM(it_id_hyp,   "hyperbolic identities", ITEM_SETTING, NULL, NULL, HAVE_YES, "SIMP_ID_HYPERBOLIC")
ITEM(it_id_cplx,  "complex identities", ITEM_SETTING, NULL, NULL, HAVE_YES, "SIMP_ID_COMPLEX")
ITEM(it_id_tconst, "trig constants",  ITEM_SETTING, NULL, NULL, HAVE_YES, "SIMP_ID_TRIG_CONSTANTS")
ITEM(it_id_iconst, "inverse trig constants", ITEM_SETTING, NULL, NULL, HAVE_YES, "SIMP_ID_TRIG_INV_CONSTANTS")
ITEM(it_rationalize, "rationalize", ITEM_SETTING, NULL, NULL, HAVE_YES,
     "CANONICAL_RATIONALIZE; off by default is still undecided")
ITEM(it_clear,    "clear history", ITEM_SETTING, NULL, NULL, HAVE_YES, NULL)

/* the lists */

const item_t *const catalog[] = {
    &it_digits, &it_point, &it_negate, &it_sci,
    &it_plus, &it_minus, &it_times, &it_divide, &it_open, &it_close, &it_comma, &it_equals,
    &it_x, &it_letters, &it_pi, &it_e, &it_i, &it_theta,
    &it_power, &it_square, &it_recip, &it_sqrt, &it_cbrt, &it_nroot,
    &it_sin, &it_cos, &it_tan, &it_asin, &it_acos, &it_atan, &it_ln, &it_exp, &it_log, &it_pow10,
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
    &it_sin, &it_cos, &it_tan, &it_asin, &it_acos, &it_atan,
    &it_sinh, &it_cosh, &it_tanh, &it_asinh, &it_acosh, &it_atanh,
    &it_ln, &it_log, &it_logb, &it_exp, &it_pow10, &it_abs, &it_fact, &it_int
};
static const item_t *const templates[] = {
    &it_frac, &it_power, &it_sqrt, &it_cbrt, &it_nroot, &it_abs, &it_logb, &it_parens,
    &it_pi, &it_e, &it_i, &it_theta, &it_piecewise
};
static const item_t *const tools[] = {
    &it_id_basic, &it_id_trig, &it_id_hyp, &it_id_cplx, &it_id_tconst, &it_id_iconst,
    &it_rationalize, &it_clear
};

#define FOLDER(key, name, list) { key, name, list, sizeof list / sizeof list[0] }

const folder_t folders[FOLDER_COUNT] = {
    FOLDER("y=",     "Algebra",   algebra),
    FOLDER("window", "Calculus",  calculus),
    FOLDER("zoom",   "Functions", functions),
    FOLDER("trace",  "Templates", templates),
    FOLDER("graph",  "Tools",     tools)
};
