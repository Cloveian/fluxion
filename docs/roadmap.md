# Feature roadmap

## foundation thingys

- [x] Storage strategy.
- [x] Names. multi-letter vars and typeable functions.

## on foundations thingies

- [x] Persistent(ish) history. Store history in ram so it survives closing the app
- [x] Variable saving + a page to view/delete.

## engine meths

- [x] cot / sec / csc.
- [x] Multi-var solve, (pulling one var out)
- [x] Systems solving
- [x] Solving trig equations.
- [x] Holes & asymptotes. zeros of the denominator.
- [x] Better "normal" math handling. (a sweep 10-16, todo.tests has whats left)

## polish

- [x] Lock icons. (for mod keys)
- [x] Clear the screen. `[on][clear]` 
- [x] Modifier taps while meth is processing
- [x] 'Soft' key menus in math print.
- [x] Screen off. auto after sitting idle, and `[2nd][alpha][on]` does it by hand
- [x] Mode menu. degrees / radians, simplify mode... etc.
- [ ] Optimisation. (never gonna check this one off)
- [ ] Use what each platform can do. 32 bit bignum *dih*gits and double decimals off the CE, the pc / web / nspire are way faster than the eZ80 and run the same 16 bit digits and float rn
- [x] Expand combines like terms after every step instead of at the end. (x+1)^7 was 5 min at 100% and ^8 ran out of memory, 2^n raw terms before anything got collected

## calc 1

- [x] Derivatives, one rule a line (product, quotient, chain, trig), at a value too
- [x] Limits. plug in, cancel, l'Hopital, and to ∞ / -∞
- [x] Integrals. table, u-sub, by parts (LIATE), sin / cos powers, partial fractions over rational roots, roots of quadratics (complete the square, trig substitution), with steps
- [x] Tangent lines, critical points, max / min and inflections, implicit *dih*rivatives
- [x] Increasing / decreasing intervals, increasing(f,x)
- [x] One-sided limits, a 4th argument of 1 or -1, limit(abs(x)/x,x,0,1)
- [x] Limits that need the conjugate, sqrt(x^2+x) - x is 1/2
- [x] A limit that doesn't exist says DNE (the two sides *dih*fer, or sin / cos of something growing keeps going round), sec / csc / cot limits and poles from one side (1/sin(x) from the right is ∞), a sin or cos times something going to 0 (x^2 cos(9/x) is 0)
- [x] d/dx of an integral with x in a limit (the fundamental theorem), and a definite integral from a to a is 0
- [x] Steps for limits at inf the paper way (*dih*vide by the top power, the conjugate, l'Hopital for inf/inf)
- [x] More integrals: sqrt(1-x^2) and sqrt(x^2+1) forms, 1/(x^2+bx+c) with irrational roots, e^x sin(x), x atan(x), x^5 e^x, abs(x), sec and csc, tan^n, sec^n, sin^2 cos^2. And x/(x^4+1), atan(x)/x^2, x^2 asin(x), x/sqrt(x+1), sqrt(x)/(1+x), sin(3x)cos(2x), 1/(1+sin(x)), 1/(2+cos(x)) and the rest of the fractions in sin and cos (t = tan(x/2)), tan^3 sec, (sin(x)+cos(x))^2, x/(x^2+2x+5)^2, x^2/(x^2+1)^2, sin(2x)/(1+cos(x)), e^x sin(x) cos(x), x e^x sin(x), cos(ln(x)). x^2/sqrt(x+1) and up, acos(x)^2. Left: atan(x)/x (no elementary answer).
- [x] Integrals answer without the extra constants (ln(2abs(1+x^2))/2)
- [x] Average value, area between curves, the mean value theorem (and Rolle's) and continuity at a point, average(, area(, mvt( and continuous(
- [x] Volume of a solid of revolution, discs, washers and shells about y=c or x=c, with the integral set up above the answer, volume(
- [ ] Newton's method, the limit definition of the *dih*rivative and Riemann sums

## later

- [x] Long answers wrap at the + and - onto lines (layout_wrapped). a wide answer that isn't a sum (a long fraction) is read by stepping into it, the view follows the cursor. still open: scroll without browsing
- [x] Inequalities. solve with < > ≤ ≥, sign chart. no trig ones yet
- [x] Slope, distance and midpoint commands
- [x] Parallel and perpendicular lines
- [ ] Transformations
- [x] Geometry formulas (area, perimeter, volume, angles). circle( box( polygon( etc, angles from triangle and polygon
- [x] Probability (nCr, nPr, and pand / por for independent events)
- [x] GCD, LCM and prime factorisation
- [x] True / false for a bare equation or inequality, 23x=22x is false and says x = 1
- [x] Percent. over 100 like the nspire, from the catalog (the 84 has no % key), ?!▸ on the nspire
- [ ] Defining and calling functions like f(x)
- [x] Domain and range. range(f,x) does pieces between holes and asymptotes, no trig yet
- [x] Factoring with more than one variable (x²−y²)
- [x] Nonlinear systems. two equations, two letters, by substitution
- [x] Cubics with no rational root
- [x] Complex roots in solve
- [x] Slant asymptotes
- [ ] Unit circle display
- [x] Triangle solver
- [x] Amplitude
- [x] Roots to a polynomial and back (roots, fromroots)
- [x] Inverse functions
- [x] Ellipses and hyperbolas, equation to points and back. lined up with the axes only
- [x] Piecewise functions. a command for now, no brace template
- [x] Ghost parens, the fraction grabbing the last term, ans when a line starts with an operator
- [x] Show-the-work steps. quadratics, radicals, exp / log / abs, rational equations, trig equations, inequalities, polynomials that factor, expand and factor, tangent / critical / extrema / inflect / implicit, *dih*rivatives, limits and integrals have them. linear systems, a line with a curve and cubics without a rational root (cardano) have them too. and simplify a phase at a time (cancel, multiply out, over one bottom). a page too long for the screen starts with its working folded under the line it belongs to (a cursor line, right / enter opens it, left closes it), so far for the cardano *dih*scriminant, each quadratic factor of a polynomial and the *dih*rivative rule lines of the tangent / extrema family, more later

## more-later

- [ ] Sets `{}`.
- [ ] Matrices `[]`.
- [ ] Expanding formula-based functions into formulas.
- [ ] Unit conversion.
- [ ] Wizards.
- [ ] Graphs.
