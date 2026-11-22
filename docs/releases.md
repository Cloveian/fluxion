# Releases

## v0.1.0

Calculus shows up, the Nspire shows up, and a pile of wrong answers stops being wrong ٩(>ᴗ<)و

### New toys

- **Integrals.** `integral(f,x)` and `integral(f,x,a,b)`: constants, sums, constant multiples, a table of the usual ones, and u-substitution (`cos(3x)` is `sin(3x)/3` in one go). A definite one checks the domain first, so `1/x^2` from -1 to 1 stays an integral instead of coming out -2.
- **Limits.** `limit(f,x,a)` at a number, at ∞ and -∞, and from one side. Cancels, then l'Hôpital when nothing cancels. `sin(x)/x` is 1, `(1+1/x)^x` is e, as it should be.
- **Show the work.** Tools -> show steps works on the highlighted answer, for `solve(`, `d/dx(`, limits and integrals. Quadratics have a method menu on the soft keys (square root, factoring, completing the square, the formula, or optimal, which picks one). Derivatives show the rule on every line (product rule, power rule, ...).
- **Absolute value equations.** `abs(4x+4)=3` has two answers and solve knows both.
- **Percent.** `25%` is 1/4 and `20%*50` is 10. It's in the catalog, since the 84 has no % key.
- **Themes.** Dark, light, contrast, green, rose and paper, in the mode menu.
- **A clock for slow answers.** Past 5 seconds the top bar counts up and says `[on]+[mode]` stops it, so a slow one doesn't look like a frozen calculator.
- **TI-Nspire CX / CX II.** Fluxion runs on the Nspire through Ndless, and its keys do what the Nspire's own OS does (ctrl + x² is √, esc backs out a level, `[menu]` drops the folders down).
- **Folders in folders.** Algebra is the everyday nine, then Function info / Lines / Shapes / Solids / Conics / Probability. Past 9, rows are `[alpha]` + A, B, C.
- `cot⁻¹`, `sec⁻¹` and `csc⁻¹` are real functions, and the catalog works out its own key hints.
- The [web page](https://cloveian.github.io/fluxion/) has a card with a demo for each of these, and a switch between the TI-84 Plus CE and the TI-Nspire CX II, each playing its own keys.

### Glow up

- **Solve** throws out fake roots from squaring (`sqrt(x)=x-2`), and checks every root against what you typed, not what simplify turned it into.
- **Factoring** gets more: even quartics (`x^4+4`, `x^8-1`), grouping (`x^2+xy-x-y` is `(x-1)(x+y)`), and common factors in sums with more than one letter.
- **Cancelling** works with more letters (`(a^2-b^2)/(a-b)` is `a+b`) and through nested fractions.
- **Roots** are smaller: `4^(1/4)` is `sqrt(2)`, two term bottoms get rationalized (`7/(6-2sqrt(2))`), and `-2/sqrt(2)` shows as `-sqrt(2)`.
- **Complex:** `x^n = c` gives all n roots, and `ln` and `sqrt` of complex numbers have the right angle now.
- **Faster and steadier.** Sums of several letters print in a fixed order (`x^2+2xy+y^2+2x+2y+1`) and no longer hang the sort. `expand((x+y+z+1)^4)` takes about 30 times less work, and `^5` finishes.

### Unbroke

- `sqrt(x)=-1`, `e^x=-1` and `asin(x)=4` each answered with an x. They have none, no matter how nicely you ask.
- `abs(-3x)` came out `3x`, which is wrong for negative x.
- Systems handed back made up pairs (`sqrt(-7)` as a real answer). Nothing in a 600 system test set *dih*sagrees with sympy about how many real pairs there are now.
- Segfaults and crashes: running out of memory inside limits and steps, a stack overflow in `3x^4+4=0`, and a crash in systems with big numbers.
- `cbrt(` never parsed, `5*2^x` printed as `52^x`, and `factor((x^2-1)^2)` printed `((x-1)(x+1))^2`.
- Wrong answers that a sympy fuzz found in solve and limits (limits landing on `asin(2)` or `ln(-2)`, solutions that didn't check out).

## v0.0.1

The first release: the app (`Fluxion-app.b84`) and the program (`Fluxion-program.b84`) for the TI-84 Plus CE. Everyone's got to start somewhere.

- Exact simplify with stacked fractions, roots and cancelling, with the answer in what you would write 'by hand', expanded, factored or decimal form.
- `factor(` and `expand(`, also with more than one letter.
- `solve(` for polynomials, cubics, equations with letters, trig, complex roots and inequalities. Systems, linear and 2x2 nonlinear.
- `d/dx(`, identities, complex numbers.
- Domain, range, holes and asymptotes, inverse, amplitude, piecewise, and roots to a polynomial and back.
- Geometry (triangle solver, shape formulas, lines, ellipses, hyperbolas), gcd / lcm / prime factors, nCr / nPr and probability.
- A 2D editor with templates, ghost brackets and selection, history, stored variables, degrees and radians, and a light theme.

