#include "catalog.h"

#include <stddef.h>

/*
    engine syntax reminders: capital single-letter variables, _ is subtract,
    - is negate, $ marks a slot the editor fills in.

    word is what you type to get the item: a run of letters in the editor gets split
    into the longest words from the left (see names.c), so typing s i n gets you sin.
*/

#define ITEM(name, label, word, kind, engine, have, note, help) \
    static const item_t name = { label, word, kind, engine, have, note, help, NULL };

/* stuff you type straight from the keypad */

/* numbers */
ITEM(it_digits,   "0-9",   NULL, ITEM_CHAR, "0-9",   HAVE_YES, NULL,
     "the digits. answers stay exact, so 1/3 stays 1/3")
ITEM(it_point,    ".",     NULL, ITEM_CHAR, ".",     HAVE_YES, NULL,
     "decimal point, for numbers like 2.5")
ITEM(it_negate,   "(-)",   NULL, ITEM_CHAR, "-",     HAVE_YES, "negate, not subtract",
     "negative sign, the (-) key. not the same as subtract")
ITEM(it_sci,      "E",     NULL, ITEM_CHAR, "[E]",   HAVE_YES, "scientific: 2E3",
     "times ten to a power, for very big or small numbers. 2E3 -> 2000")

/* operators */
ITEM(it_plus,     "+",     NULL, ITEM_CHAR, "+",     HAVE_YES, NULL,
     "adds. 2+3 -> 5")
ITEM(it_minus,    "-",     NULL, ITEM_CHAR, "_",     HAVE_YES, NULL,
     "subtracts. 5-3 -> 2. for a negative number use the (-) key instead")
ITEM(it_times,    "x",     NULL, ITEM_CHAR, "*",     HAVE_YES, "shown as nothing, or a dot between numbers",
     "multiplies. 2*x shows as 2x; between two numbers it shows as a dot")
ITEM(it_divide,   "/",     NULL, ITEM_CHAR, "/",     HAVE_YES, "typed inline, shown stacked (Nspire)",
     "divides. you type it in a line and it is drawn as a stacked fraction. 6/4 -> 3/2")
ITEM(it_open,     "(",     NULL, ITEM_CHAR, "(",     HAVE_YES, NULL,
     "opening parenthesis, to group things. 2(x+1)")
ITEM(it_close,    ")",     NULL, ITEM_CHAR, ")",     HAVE_YES, NULL,
     "closing parenthesis, ends a group or a function's input")
ITEM(it_comma,    ",",     NULL, ITEM_CHAR, ",",     HAVE_YES, "between arguments: logb(x,2)",
     "separates the inputs of a function, like solve(2x=6,x)")
ITEM(it_equals,   "=",     NULL, ITEM_CHAR, "=", HAVE_PARTLY, "solve( system( and ellipse( hyperbola( piecewise( implicit(: the engine has no equations, the app splits the line",
     "equals sign, for solve, system, piecewise, implicit and the conics. solve(2x=6,x) -> x = 3")

/* variables and constants */
ITEM(it_x,        "x",     NULL, ITEM_CHAR, "X", HAVE_YES, "shown lowercase",
     "the variable x, it has a key of its own")
ITEM(it_letters,  "a-z",   NULL, ITEM_CHAR, "A-Z", HAVE_YES, "shown lowercase, one letter each",
     "variables, one letter each")
ITEM(it_pi,       "pi",    "pi", ITEM_CHAR, "pi",   HAVE_YES, NULL,
     "the number pi = 3.14159..., kept exact. sin(pi/2) -> 1")
ITEM(it_e,        "e",     "e", ITEM_CHAR, "e",    HAVE_YES, "the constant, drawn bold",
     "the number e = 2.71828..., drawn bold so it isn't the variable e. ln(e) -> 1")
ITEM(it_i,        "i",     "i", ITEM_CHAR, "i",    HAVE_YES, "the constant, drawn bold",
     "the imaginary unit, i^2 = -1. drawn bold so it isn't the variable i")
ITEM(it_theta,    "theta", "theta", ITEM_CHAR, "theta", HAVE_YES, NULL,
     "the variable theta, often used for angles. type theta")

/* powers and roots */
ITEM(it_power,    "x^n",   NULL, ITEM_TEMPLATE, "($)^($)",   HAVE_YES, "cursor jumps into the exponent",
     "raises to a power; the cursor moves into the exponent. 2^5 -> 32")
ITEM(it_square,   "x^2",   NULL, ITEM_CHAR,     "^2",        HAVE_YES, NULL,
     "squares what comes before it. 3^2 -> 9")
ITEM(it_recip,    "x^-1",  NULL, ITEM_CHAR,     "^(-1)",     HAVE_YES, NULL,
     "one over what comes before it. 4^-1 -> 1/4")
ITEM(it_sqrt,     "sqrt",  "sqrt", ITEM_TEMPLATE, "sqrt($)",  HAVE_YES, NULL,
     "square root. sqrt(8) -> 2sqrt(2)")
ITEM(it_cbrt,     "cbrt",  "cbrt", ITEM_TEMPLATE, "($)root($)", HAVE_YES, "3root: ROOT with index 3",
     "cube root. cbrt(27) -> 3")
ITEM(it_nroot,    "n-root", NULL, ITEM_TEMPLATE, "($)root($)", HAVE_YES, "index first",
     "root with any index; type the index first. the 4th root of 16 -> 2")

/* functions with their own key */
ITEM(it_sin,      "sin",   "sin", ITEM_FUNCTION, "sin($)",  HAVE_YES, NULL,
     "sine of an angle, in radians. sin(pi/6) -> 1/2")
ITEM(it_cos,      "cos",   "cos", ITEM_FUNCTION, "cos($)",  HAVE_YES, NULL,
     "cosine of an angle, in radians. cos(pi/3) -> 1/2")
ITEM(it_tan,      "tan",   "tan", ITEM_FUNCTION, "tan($)",  HAVE_YES, NULL,
     "tangent of an angle, in radians. tan(pi/4) -> 1")
ITEM(it_asin,     "sin^-1", "asin", ITEM_FUNCTION, "asin($)", HAVE_YES, NULL,
     "inverse sine: the angle with this sine, in radians. sin^-1(1/2) -> pi/6")
ITEM(it_acos,     "cos^-1", "acos", ITEM_FUNCTION, "acos($)", HAVE_YES, NULL,
     "inverse cosine: the angle with this cosine, in radians. cos^-1(0) -> pi/2")
ITEM(it_atan,     "tan^-1", "atan", ITEM_FUNCTION, "atan($)", HAVE_YES, NULL,
     "inverse tangent: the angle with this tangent, in radians. tan^-1(1) -> pi/4")
ITEM(it_cot,      "cot",   "cot", ITEM_FUNCTION, "cot($)",  HAVE_YES, NULL,
     "cotangent, 1/tan. cot(pi/4) -> 1")
ITEM(it_sec,      "sec",   "sec", ITEM_FUNCTION, "sec($)",  HAVE_YES, NULL,
     "secant, 1/cos. sec(0) -> 1")
ITEM(it_csc,      "csc",   "csc", ITEM_FUNCTION, "csc($)",  HAVE_YES, NULL,
     "cosecant, 1/sin. csc(pi/2) -> 1")
ITEM(it_acot,     "cot^-1", "acot", ITEM_FUNCTION, "acot($)", HAVE_YES, NULL,
     "inverse cotangent, between 0 and pi like the nspire. cot^-1(1) -> pi/4")
ITEM(it_asec,     "sec^-1", "asec", ITEM_FUNCTION, "asec($)", HAVE_YES, NULL,
     "inverse secant, cos^-1(1/x). sec^-1(2) -> pi/3")
ITEM(it_acsc,     "csc^-1", "acsc", ITEM_FUNCTION, "acsc($)", HAVE_YES, NULL,
     "inverse cosecant, sin^-1(1/x). csc^-1(2) -> pi/6")
ITEM(it_ln,       "ln",    "ln", ITEM_FUNCTION, "ln($)",   HAVE_YES, NULL,
     "natural logarithm, base e. ln(e^2) -> 2")
ITEM(it_exp,      "e^x",   NULL, ITEM_TEMPLATE, "e^($)",   HAVE_YES, NULL,
     "e raised to a power, the opposite of ln. e^0 -> 1")
ITEM(it_log,      "log",   "log", ITEM_FUNCTION, "log($)",  HAVE_YES, "base 10",
     "logarithm base 10. log(1000) -> 3")
ITEM(it_pow10,    "10^x",  NULL, ITEM_TEMPLATE, "10^($)",  HAVE_YES, NULL,
     "10 raised to a power, the opposite of log. 10^3 -> 1000")

/* folder-only (no direct key) */

/* Functions */
ITEM(it_sinh,     "sinh",  "sinh", ITEM_FUNCTION, "sinh($)",  HAVE_YES, NULL,
     "hyperbolic sine, (e^x-e^-x)/2. sinh(0) -> 0")
ITEM(it_cosh,     "cosh",  "cosh", ITEM_FUNCTION, "cosh($)",  HAVE_YES, NULL,
     "hyperbolic cosine, (e^x+e^-x)/2. cosh(0) -> 1")
ITEM(it_tanh,     "tanh",  "tanh", ITEM_FUNCTION, "tanh($)",  HAVE_YES, NULL,
     "hyperbolic tangent, sinh(x)/cosh(x). tanh(0) -> 0")
ITEM(it_asinh,    "sinh^-1", "asinh", ITEM_FUNCTION, "asinh($)", HAVE_YES, NULL,
     "inverse hyperbolic sine. sinh^-1(0) -> 0")
ITEM(it_acosh,    "cosh^-1", "acosh", ITEM_FUNCTION, "acosh($)", HAVE_YES, NULL,
     "inverse hyperbolic cosine. cosh^-1(1) -> 0")
ITEM(it_atanh,    "tanh^-1", "atanh", ITEM_FUNCTION, "atanh($)", HAVE_YES, NULL,
     "inverse hyperbolic tangent. tanh^-1(0) -> 0")
ITEM(it_logb,     "log_b", "logb", ITEM_TEMPLATE, "logb($,$)", HAVE_YES, "argument first, then base",
     "logarithm with any base. log_2(8) -> 3")
ITEM(it_abs,      "|x|",   "abs", ITEM_TEMPLATE, "abs($)",   HAVE_YES, NULL,
     "absolute value, the distance from 0. |-5| -> 5")
ITEM(it_percent,  "%",     NULL, ITEM_CHAR,     "%",        HAVE_YES, NULL,
     "percent, over 100. 20%*50 -> 10")
ITEM(it_fact,     "!",     NULL, ITEM_CHAR,     "!",        HAVE_YES, NULL,
     "factorial, multiplies 1 up to n. 5! -> 120")
ITEM(it_int,      "int",   "int", ITEM_FUNCTION, "int($)",   HAVE_YES, "floor, like the TI's int(",
     "rounds down to a whole number, like the TI's int(. int(7/2) -> 3")
ITEM(it_gcd,      "gcd",   "gcd", ITEM_FUNCTION, "gcd($,$)", HAVE_YES, "whole numbers only, anything else stays as typed",
     "greatest common divisor, the biggest whole number that divides both. gcd(12,18) -> 6")
ITEM(it_lcm,      "lcm",   "lcm", ITEM_FUNCTION, "lcm($,$)", HAVE_YES, "whole numbers only, anything else stays as typed",
     "least common multiple, the smallest whole number both divide into. lcm(4,6) -> 12")
ITEM(it_ncr,      "nCr",   NULL, ITEM_FUNCTION, "nCr($,$)", HAVE_YES, "n first, then r. menu only, typed letters come out lowercase",
     "combinations: ways to pick r things out of n when order doesn't matter. nCr(5,2) -> 10")
ITEM(it_npr,      "nPr",   NULL, ITEM_FUNCTION, "nPr($,$)", HAVE_YES, "n first, then r. menu only, typed letters come out lowercase",
     "permutations: ways to line up r things out of n. nPr(5,2) -> 20")
ITEM(it_floor,    "floor", "floor", ITEM_FUNCTION, "floor($)", HAVE_YES, "the same as int(, it shows as int(",
     "rounds down to a whole number, toward minus infinity. floor(-7/2) -> -4")
ITEM(it_ceil,     "ceil",  "ceil", ITEM_FUNCTION, "ceil($)", HAVE_YES, NULL,
     "rounds up to a whole number. ceil(7/2) -> 4")
ITEM(it_round,    "round", "round", ITEM_FUNCTION, "round($)", HAVE_YES, "halves go away from 0 like the TI's",
     "the nearest whole number. round(7/2) -> 4, round(-5/2) -> -3")
ITEM(it_sign,     "sign",  "sign", ITEM_FUNCTION, "sign($)", HAVE_YES, "numbers only, sign(x) stays",
     "-1, 0 or 1 by which side of 0 it's on. sign(-4) -> -1")
ITEM(it_mod,      "mod",   "mod", ITEM_FUNCTION, "mod($,$)", HAVE_YES, "the answer has the sign of the second one",
     "the remainder, a - b floor(a/b). mod(7,3) -> 1, mod(-7,3) -> 2")
ITEM(it_min,      "min",   "min", ITEM_FUNCTION, "min($,$)", HAVE_YES, "numbers only, min(x,2) stays",
     "the smaller of two. min(2,3) -> 2")
ITEM(it_max,      "max",   "max", ITEM_FUNCTION, "max($,$)", HAVE_YES, "numbers only, max(x,2) stays",
     "the bigger of two. max(2,3) -> 3")
ITEM(it_binomial, "binomial", "binomial", ITEM_FUNCTION, "binomial($,$)", HAVE_YES, "the same as nCr, it shows as nCr(",
     "ways to pick r things out of n. binomial(5,2) -> 10")
ITEM(it_conj,     "conj",  "conj", ITEM_FUNCTION, "conj($)", HAVE_YES, "letters count as real",
     "the complex conjugate, i turned into -i. conj(3+4i) -> 3-4i")
ITEM(it_re,       "re",    "re", ITEM_FUNCTION, "re($)", HAVE_YES, "letters count as real",
     "the real part. re(3+4i) -> 3")
ITEM(it_im,       "im",    "im", ITEM_FUNCTION, "im($)", HAVE_YES, "letters count as real",
     "the imaginary part, without the i. im(3+4i) -> 4")
ITEM(it_arg,      "arg",   "arg", ITEM_FUNCTION, "arg($)", HAVE_YES, "radians, -pi to pi",
     "the angle of a complex number. arg(1+i) -> pi/4")
ITEM(it_apart,    "apart",  "apart", ITEM_COMMAND, "apart($)", HAVE_PARTLY,
     "apart(f,x): the whole part, then a fraction over each root of the bottom. rational roots, all different",
     "splits a fraction into simpler ones. apart(1/(x^2-1),x) -> 1/(2x-2)-1/(2x+2)")
ITEM(it_polydiv,  "divide", "divide", ITEM_COMMAND, "divide($)", HAVE_YES,
     "divide(p,q,x): polynomial long division, the quotient and the remainder. numbers for coefficients",
     "p = quotient q + remainder. divide(x^3+1,x+2,x) -> quotient x^2-2x+4, remainder -7")
ITEM(it_amplitude, "amplitude", "amplitude", ITEM_COMMAND, "amplitude($)", HAVE_PARTLY,
     "amplitude(f,x): |A| of A*sin(Bx+C)+D or cos, sqrt(a^2+b^2) for a*sin(u)+b*cos(u). anything else is a message",
     "how far a sine or cosine wave goes from its middle. amplitude(3sin(2x)+1,x) -> 3")

ITEM(it_tangent, "tangent", "tangent", ITEM_COMMAND, "tangent($)", HAVE_YES,
     "tangent(f,a,x): y = f'(a)(x-a)+f(a). the x is optional",
     "the tangent line to f at x = a. tangent(x^2,3) -> y = 6x-9")
ITEM(it_critical, "critical", "critical", ITEM_COMMAND, "critical($)", HAVE_PARTLY,
     "critical(f,x): where f' = 0, real ones. as far as solve gets",
     "where the slope is 0. critical(x^3-3x,x) -> x = -1, 1")
ITEM(it_extrema, "extrema", "extrema", ITEM_COMMAND, "extrema($)", HAVE_PARTLY,
     "extrema(f,x): the points where f' = 0 and changes sign, max or min. a flat spot that doesn't turn isn't one",
     "the highest and lowest turning points. extrema(x^3-3x,x) -> max (-1, 2), min (1, -2)")
ITEM(it_inflect, "inflect", "inflect", ITEM_COMMAND, "inflect($)", HAVE_PARTLY,
     "inflect(f,x): the points where f'' = 0 and changes sign",
     "where the curve changes from bending up to bending down. inflect(x^3,x) -> inflection (0, 0)")
ITEM(it_increasing, "increasing", "increasing", ITEM_COMMAND, "increasing($)", HAVE_PARTLY,
     "increasing(f,x): where f' is over 0 and where it's under, as intervals",
     "where the curve goes up and where it goes down. increasing(x^3-3x,x) -> increasing x < -1 or x > 1, decreasing -1 < x < 1")

ITEM(it_implicit, "implicit", "implicit", ITEM_COMMAND, "implicit($)", HAVE_PARTLY,
     "implicit(equation,y,x): dy/dx as -(dF/dx)/(dF/dy) of left - right. y and x are the defaults",
     "the slope of a curve that isn't a function of x. implicit(x^2+y^2=25) -> dy/dx = -x/y")

/* Templates */
ITEM(it_frac,     "n/d",   NULL, ITEM_TEMPLATE, "($)/($)", HAVE_YES, "empty fraction, cursor in the top",
     "an empty fraction. type the top, then move down to type the bottom")
ITEM(it_parens,   "( )",   NULL, ITEM_TEMPLATE, "($)",       HAVE_YES, NULL,
     "a pair of parentheses with the cursor inside, to group things")
ITEM(it_piecewise, "piecewise", "piecewise", ITEM_COMMAND, "piecewise($)", HAVE_PARTLY,
     "piecewise(f1,c1,f2,c2,...,x=v): done in the app, the engine has no conditions. conditions are < > <= >= != chains in one letter. no brace yet",
     "a different rule for different x. piecewise(x^2,x<0,2x+1,x>=0) shows the pieces, add x=3 at the end -> 7, the first piece whose condition holds. 1<x<=3 chains work")

/* Algebra (commands) */
ITEM(it_simplify, "simplify", NULL, ITEM_COMMAND, NULL, HAVE_YES,
     "always runs on enter: simplify(SIMP_ALL) + canonical form",
     "runs on enter: simplifies whatever is on the line")
ITEM(it_expand,   "expand", "expand", ITEM_COMMAND, "expand($)", HAVE_YES, "expand() with EXP_ALL",
     "multiplies everything out. expand((x+1)^2) -> x^2+2x+1")
ITEM(it_subst,    "substitute", "subst", ITEM_COMMAND, "subst($,$,$)", HAVE_PARTLY,
     "substitute() exists, but the parser has no syntax for it: Fluxion handles it",
     "puts a value or expression in place of a variable. not in Fluxion yet")
ITEM(it_evaluate, "evaluate", "eval", ITEM_COMMAND, "eval($)", HAVE_PARTLY,
     "exact only (big powers, factorials); no decimal approximation yet",
     "works out the exact value. eval(2^10) -> 1024. no decimal answers yet")
ITEM(it_factor,   "factor", "factor", ITEM_COMMAND, "factor($)", HAVE_PARTLY,
     "over the rationals, more than one letter ok (x^2-y^2); not irrational roots (factor(x^2-2,x))",
     "writes it as a product. factor(x^2-1) -> (x-1)(x+1). x^2-y^2 works too")
ITEM(it_solve,    "solve",  "solve", ITEM_COMMAND, "solve($)", HAVE_PARTLY,
     "solve(eq,x): linear (any coefficients), polynomials via factor + the quadratic formula + cubics (cos form or cardano), trig families, one letter at a time. < > <= >= go through a sign chart (not trig yet). systems have their own command",
     "solves equations and inequalities. solve(x^2-4=0,x) -> x = -2, 2. solve(x^2-4<0,x) -> -2 < x < 2")
ITEM(it_asym,     "asymptotes", "asym", ITEM_COMMAND, "asym($)", HAVE_PARTLY,
     "asym(f,x): holes, vertical, horizontal and slant asymptotes of a rational function",
     "holes and asymptotes of a fraction. asym((x^2-1)/(x-1),x) -> hole at x = 1. slant ones too")
ITEM(it_domain,   "domain", "domain", ITEM_COMMAND, "domain($)", HAVE_PARTLY,
     "domain(f,x): zeros of bottoms, even roots and logs of ax+b, tan at pi/2+pi n. worse inside a root or log just says (and maybe more)",
     "where it's defined. domain(sqrt(x-2),x) -> x >= 2. a root or log of more than ax+b is only partly done")
ITEM(it_range,    "range",  "range", ITEM_COMMAND, "range($)", HAVE_PARTLY,
     "range(f,x): domain and asym cut it into pieces, each one spans its turning points (f' = 0) and ends. no trig yet",
     "the y values it takes. range(x^2+1,x) -> y >= 1, range(1/x,x) -> y < 0 or y > 0")
ITEM(it_system,   "system", "system", ITEM_COMMAND, "system($)", HAVE_PARTLY,
     "system(eq, eq, ..., x, y): linear systems by elimination, letters in the coefficients ok. two nonlinear ones by substitution",
     "solves equations together. system(2x-y=7,x+y=2,x,y) -> x = 3, y = -1. two curvy ones work too, x^2+y^2=25 and y=x+1")
ITEM(it_slope,    "slope",  "slope", ITEM_COMMAND, "slope($)", HAVE_YES,
     "slope(x1,y1,x2,y2): (y2-y1)/(x2-x1), done in the app, a message when it's vertical",
     "slope between two points. slope(1,2,3,4) -> 1")
ITEM(it_dist,     "distance", "dist", ITEM_COMMAND, "dist($)", HAVE_YES,
     "dist(x1,y1,x2,y2): sqrt((x2-x1)^2+(y2-y1)^2), exact",
     "distance between two points. dist(0,0,3,4) -> 5")
ITEM(it_mid,      "midpoint", "mid", ITEM_COMMAND, "mid($)", HAVE_YES,
     "mid(x1,y1,x2,y2): both averages, shown as a point",
     "the point halfway between two points. mid(0,0,4,2) -> (2, 1)")
ITEM(it_par,      "parallel", "par", ITEM_COMMAND, "par($)", HAVE_YES,
     "par(m,x1,y1): y = mx + b through the point. called par, parallel is one letter too long for a name",
     "the line with slope m through a point. par(2,1,1) -> y = 2x - 1")
ITEM(it_perp,     "perpendicular", "perp", ITEM_COMMAND, "perp($)", HAVE_YES,
     "perp(m,x1,y1): slope -1/m through the point, x = x1 when m is 0",
     "the line at right angles to slope m, through a point. perp(2,1,1) -> y = (-x+3)/2")
ITEM(it_roots,    "roots",  "roots", ITEM_COMMAND, "roots($)", HAVE_PARTLY,
     "roots(p,x): solve(p=0,x) under another name, complex roots on, same limits as solve",
     "where a polynomial is 0, complex ones too. roots(x^2+1,x) -> x = -i, i")
ITEM(it_fromroots,  "from roots", "fromroots", ITEM_COMMAND, "fromroots($)", HAVE_YES,
     "fromroots(r1,r2,...,x): (x-r1)(x-r2)... expanded. a letter at the end is the variable",
     "the polynomial with those roots. fromroots(2,-3) -> x^2+x-6")
ITEM(it_inverse,  "inverse", "inverse", ITEM_COMMAND, "inverse($)", HAVE_PARTLY,
     "inverse(f,x): solves y = f(x) for x over the reals and puts x back for y. as far as solve gets",
     "the inverse function. inverse(2x+3) -> (x-3)/2, inverse(x^2) -> -sqrt(x), sqrt(x)")
ITEM(it_triangle, "triangle", "triangle", ITEM_COMMAND, "triangle($)", HAVE_YES,
     "triangle(a,b,c,ab,bc,ca): sss, sas, asa, aas and ssa (both when there are two), floats in the app, angle mode for the angles",
     "solves a triangle from 3 of its sides and angles, empty or 0 for the rest. ab is the angle between a and b. triangle(3,4,5,,,) -> ab = 90 (in degrees)")
ITEM(it_circle,   "circle", "circle", ITEM_COMMAND, "circle($)", HAVE_YES,
     "circle(r): area, circumference, diameter",
     "area and the rest from the radius. circle(3) -> area = 9pi")
ITEM(it_rect,     "rectangle", "rect", ITEM_COMMAND, "rect($)", HAVE_YES,
     "rect(l,w): area, perimeter, diagonal",
     "a rectangle, length then width. rect(3,4) -> area = 12, diagonal = 5")
ITEM(it_squareshape, "square", "square", ITEM_COMMAND, "square($)", HAVE_YES,
     "square(s): area, perimeter, diagonal",
     "rect with both sides the same. square(2) -> area = 4, diagonal = 2sqrt(2)")
ITEM(it_trap,     "trapezoid", "trap", ITEM_COMMAND, "trap($)", HAVE_YES,
     "trap(a,b,h): area, a and b the parallel sides",
     "trapezoid area, the two parallel sides then the height. trap(3,5,2) -> area = 8")
ITEM(it_sphere,   "sphere", "sphere", ITEM_COMMAND, "sphere($)", HAVE_YES,
     "sphere(r): volume, surface area",
     "volume and surface from the radius. sphere(3) -> volume = 36pi")
ITEM(it_cyl,      "cylinder", "cyl", ITEM_COMMAND, "cyl($)", HAVE_YES,
     "cyl(r,h): volume, surface area, side area",
     "cylinder, radius then height. side area is without the two ends. cyl(2,5) -> volume = 20pi, surface area = 28pi")
ITEM(it_cone,     "cone", "cone", ITEM_COMMAND, "cone($)", HAVE_YES,
     "cone(r,h): volume, slant, surface area",
     "a right cone, radius then height. cone(3,4) -> volume = 12pi, slant = 5")
ITEM(it_cube,     "cube", "cube", ITEM_COMMAND, "cube($)", HAVE_YES,
     "cube(s): volume, surface area, diagonal",
     "diagonal is corner to corner through the middle. cube(2) -> volume = 8, diagonal = 2sqrt(3)")
ITEM(it_box,      "box", "box", ITEM_COMMAND, "box($)", HAVE_YES,
     "box(l,w,h): volume, surface area, diagonal",
     "a rectangular box, l w h. box(2,3,6) -> volume = 36, diagonal = 7")
ITEM(it_polygon,  "polygon", "polygon", ITEM_COMMAND, "polygon($)", HAVE_YES,
     "polygon(n): angle sum, interior and exterior angle (regular), diagonals. angle mode for the angles",
     "n sides. the angles are for a regular one. polygon(6) -> angle sum = 720, interior = 120, diagonals = 9")
ITEM(it_pand,     "P(A and B)", "pand", ITEM_COMMAND, "pand($)", HAVE_YES,
     "pand(pa,pb): pa*pb, exact. assumes independent events",
     "chance both happen, for independent events. pand(1/2,1/3) -> 1/6")
ITEM(it_por,      "P(A or B)", "por", ITEM_COMMAND, "por($)", HAVE_YES,
     "por(pa,pb): pa+pb-pa*pb, exact. assumes independent events",
     "chance at least one happens, for independent events. por(1/2,1/3) -> 2/3")
ITEM(it_ellipse,  "ellipse", "ellipse", ITEM_COMMAND, "ellipse($)", HAVE_PARTLY,
     "ellipse(eq): center, vertices, co-vertices, foci by completing the square. ellipse(h,k,vx,vy,fx,fy): the equation, b^2 = a^2 - c^2. no tilted ones (no xy)",
     "ellipse(4x^2+9y^2=36) -> center, vertices, co-vertices, foci. ellipse(h,k,vx,vy,fx,fy), center + a vertex + a focus -> the equation. lined up with the axes only")
ITEM(it_hyperbola, "hyperbola", "hyperbola", ITEM_COMMAND, "hyperbola($)", HAVE_PARTLY,
     "hyperbola(eq): center, vertices, foci, asymptotes y = k +- (b/a)(x-h). hyperbola(h,k,vx,vy,fx,fy): the equation, b^2 = c^2 - a^2. no tilted ones (no xy)",
     "hyperbola(x^2/9-y^2/16=1) -> center, vertices, foci, asymptotes. hyperbola(h,k,vx,vy,fx,fy), center + a vertex + a focus -> the equation. lined up with the axes only")

/* Calculus */
ITEM(it_deriv,    "d/dx",   "deriv", ITEM_TEMPLATE, "deriv($,X,X)", HAVE_YES, "derivative with respect to x",
     "derivative with respect to x. d/dx(x^3) -> 3x^2")
ITEM(it_deriv_at, "d/dx at", NULL, ITEM_TEMPLATE, "deriv($,X,$)", HAVE_YES, "derivative evaluated at a value",
     "derivative at one value of x, typed out for now: function, x, value. deriv(x^2,x,3) -> 6")
ITEM(it_integral, "integral", "integral", ITEM_TEMPLATE, "integral($,X)", HAVE_YES, "table + linearity + u-substitution",
     "the area under a curve, typed out for now: function, x, then from and to for a number. integral(x^2,x,0,3) -> 9")
ITEM(it_limit,    "limit",  "limit", ITEM_TEMPLATE, "limit($,X,$)", HAVE_YES, "straight in, on 0/0 cancel then l'Hopital",
     "the value a function gets close to as x gets close to a number, typed out for now: function, x, value. limit(sin(x)/x,x,0) -> 1")

/* Tools */
ITEM(it_id_basic, "basic identities", NULL, ITEM_SETTING, NULL, HAVE_YES, "SIMP_ID_GENERAL", NULL)
ITEM(it_id_trig,  "trig identities",  NULL, ITEM_SETTING, NULL, HAVE_YES, "SIMP_ID_TRIG", NULL)
ITEM(it_id_hyp,   "hyperbolic identities", NULL, ITEM_SETTING, NULL, HAVE_YES, "SIMP_ID_HYPERBOLIC", NULL)
ITEM(it_id_cplx,  "complex identities", NULL, ITEM_SETTING, NULL, HAVE_YES, "SIMP_ID_COMPLEX", NULL)
ITEM(it_id_tconst, "trig constants",  NULL, ITEM_SETTING, NULL, HAVE_YES, "SIMP_ID_TRIG_CONSTANTS", NULL)
ITEM(it_id_iconst, "inverse trig constants", NULL, ITEM_SETTING, NULL, HAVE_YES, "SIMP_ID_TRIG_INV_CONSTANTS", NULL)
ITEM(it_clear,    "clear history", NULL, ITEM_SETTING, NULL, HAVE_YES, NULL, NULL)
ITEM(it_steps,    "show steps", NULL, ITEM_SETTING, NULL, HAVE_YES, NULL, NULL)

/* the lists */

const item_t *const catalog[] = {
    &it_digits, &it_point, &it_negate, &it_sci,
    &it_plus, &it_minus, &it_times, &it_divide, &it_open, &it_close, &it_comma, &it_equals,
    &it_x, &it_letters, &it_pi, &it_e, &it_i, &it_theta,
    &it_power, &it_square, &it_recip, &it_sqrt, &it_cbrt, &it_nroot,
    &it_sin, &it_cos, &it_tan, &it_asin, &it_acos, &it_atan, &it_cot, &it_sec, &it_csc, &it_acot, &it_asec, &it_acsc, &it_ln, &it_exp, &it_log, &it_pow10,
    &it_sinh, &it_cosh, &it_tanh, &it_asinh, &it_acosh, &it_atanh, &it_logb, &it_abs, &it_percent, &it_fact, &it_int,
    &it_gcd, &it_lcm, &it_ncr, &it_npr, &it_amplitude,
    &it_apart, &it_polydiv, &it_floor, &it_ceil, &it_round, &it_sign, &it_mod, &it_min, &it_max, &it_binomial, &it_conj, &it_re, &it_im, &it_arg,
    &it_frac, &it_parens, &it_piecewise,
    &it_simplify, &it_expand, &it_subst, &it_evaluate, &it_factor, &it_solve, &it_asym, &it_domain, &it_range, &it_system,
    &it_slope, &it_dist, &it_mid, &it_par, &it_perp, &it_roots, &it_fromroots, &it_inverse,
    &it_triangle, &it_circle, &it_rect, &it_squareshape, &it_trap, &it_sphere, &it_cyl, &it_cone,
    &it_cube, &it_box, &it_polygon, &it_pand, &it_por,
    &it_ellipse, &it_hyperbola,
    &it_deriv, &it_deriv_at, &it_integral, &it_limit, &it_tangent, &it_critical, &it_extrema, &it_inflect, &it_implicit, &it_increasing,
    &it_id_basic, &it_id_trig, &it_id_hyp, &it_id_cplx, &it_id_tconst, &it_id_iconst, &it_clear, &it_steps
};
const unsigned catalog_count = sizeof catalog / sizeof catalog[0];

#define FOLDER(key, name, list) { key, name, list, sizeof list / sizeof list[0] }
#define SUBFOLDER(item, folder, label, list) \
    static const folder_t folder = FOLDER(NULL, label, list); \
    static const item_t item = { label, NULL, ITEM_FOLDER, NULL, HAVE_YES, NULL, NULL, &folder };

static const item_t *const fn_info[] = { &it_asym, &it_domain, &it_range };
static const item_t *const lines[] = { &it_slope, &it_dist, &it_mid, &it_par, &it_perp };
static const item_t *const shapes[] = { &it_triangle, &it_circle, &it_rect, &it_squareshape, &it_trap, &it_polygon };
static const item_t *const solids[] = { &it_sphere, &it_cyl, &it_cone, &it_cube, &it_box };
static const item_t *const conics[] = { &it_ellipse, &it_hyperbola };
static const item_t *const prob[] = { &it_pand, &it_por };

SUBFOLDER(it_fn_info, fn_info_folder, "Function info", fn_info)
SUBFOLDER(it_lines, lines_folder, "Lines", lines)
SUBFOLDER(it_shapes, shapes_folder, "Shapes", shapes)
SUBFOLDER(it_solids, solids_folder, "Solids", solids)
SUBFOLDER(it_conics, conics_folder, "Conics", conics)
SUBFOLDER(it_prob, prob_folder, "Probability", prob)

static const item_t *const algebra[] = {
    &it_expand, &it_factor, &it_solve, &it_system, &it_roots, &it_fromroots, &it_inverse, &it_apart, &it_polydiv, &it_subst, &it_evaluate,
    &it_fn_info, &it_lines, &it_shapes, &it_solids, &it_conics, &it_prob
};
static const item_t *const calculus[] = {
    &it_deriv, &it_deriv_at, &it_integral, &it_limit, &it_tangent, &it_critical, &it_extrema, &it_inflect, &it_implicit, &it_increasing
};
static const item_t *const functions[] = {
    &it_sin, &it_cos, &it_tan, &it_asin, &it_acos, &it_atan, &it_cot, &it_sec, &it_csc, &it_acot, &it_asec, &it_acsc,
    &it_sinh, &it_cosh, &it_tanh, &it_asinh, &it_acosh, &it_atanh,
    &it_ln, &it_log, &it_logb, &it_exp, &it_pow10, &it_abs, &it_fact, &it_int,
    &it_gcd, &it_lcm, &it_ncr, &it_npr, &it_amplitude,
    &it_floor, &it_ceil, &it_round, &it_sign, &it_mod, &it_min, &it_max, &it_binomial, &it_conj, &it_re, &it_im, &it_arg
};
static const item_t *const templates[] = {
    &it_frac, &it_power, &it_sqrt, &it_cbrt, &it_nroot, &it_abs, &it_logb, &it_parens,
    &it_pi, &it_e, &it_i, &it_theta, &it_piecewise
};
static const item_t *const tools[] = {
    &it_steps, &it_id_basic, &it_id_trig, &it_id_hyp, &it_id_cplx, &it_id_tconst, &it_id_iconst,
    &it_clear
};

const folder_t folders[FOLDER_COUNT] = {
    FOLDER("y=",     "Algebra",   algebra),
    FOLDER("window", "Calculus",  calculus),
    FOLDER("zoom",   "Functions", functions),
    FOLDER("trace",  "Templates", templates),
    FOLDER("graph",  "Tools",     tools)
};

/* the nspire's [menu]: the five as one numbered list, each opening like a subfolder */
#define TOP(n) { folders[n].name, NULL, ITEM_FOLDER, NULL, HAVE_YES, NULL, NULL, &folders[n] }
static const item_t top_items[FOLDER_COUNT] = { TOP(0), TOP(1), TOP(2), TOP(3), TOP(4) };
static const item_t *const top_list[FOLDER_COUNT] = { &top_items[0], &top_items[1], &top_items[2], &top_items[3], &top_items[4] };
const folder_t menu_top_folder = FOLDER(NULL, "Menu", top_list);
