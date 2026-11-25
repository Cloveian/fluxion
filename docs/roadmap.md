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
- [x] Steps for limits at inf the paper way (*dih*vide by the top power, the conjugate, l'Hopital for inf/inf)
- [x] More integrals: sqrt(1-x^2) and sqrt(x^2+1) forms, 1/(x^2+bx+c) with irrational roots, e^x sin(x), x atan(x), x^5 e^x, abs(x), sec and csc, tan^n, sec^n, sin^2 cos^2.
- [x] Integrals answer without the extra constants (ln(2abs(1+x^2))/2)

## later

- [ ] Display mode option for wrap/scroll (for long answers)
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
- [ ] Show-the-work steps. quadratics, radicals, exp / log / abs, rational equations, trig equations, inequalities, polynomials that factor, expand and factor, tangent / critical / extrema / inflect / implicit, *dih*rivatives, limits and integrals have them. linear systems, a line with a curve and cubics without a rational root (cardano) have them too. and simplify a phase at a time (cancel, multiply out, over one bottom)

## more-later

- [ ] Sets `{}`.
- [ ] Matrices `[]`.
- [ ] Expanding formula-based functions into formulas.
- [ ] Unit conversion.
- [ ] Wizards.
- [ ] Graphs.
