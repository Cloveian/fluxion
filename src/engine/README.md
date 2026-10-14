# Engine

The CAS engine, from [PineappleCAS](https://github.com/nathanfarlow/PineappleCAS) by Nathan Farlow (MIT, see [LICENSE](LICENSE)).

Copied from commit `65427a6` (2022-11-28), everything in its `src/` except `calc/` (its calculator UI, Fluxion has its own).

- `cas/`: simplify, expand, factor, derivative, eval, identities
- `imath/`: big numbers + rational maths
- `pc/`: PineappleCAS's PC command line + test runner, only built for the PC (`#ifdef COMPILE_PC`)
- the rest: AST, parser, exporter, errors

## Changes from upstream

Everything i change in here goes in this table so it's clear what's PineappleCAS and what's mine.

| Date | File | Change | Why |
| ---- | ---- | ------ | --- |
| 2026-10-13 | `cas/identities.c/.h` | `id_ExecuteTable` simplifies once at the start and after a change, instead of after every identity it tries | it was re-simplifying ~120 times a pass for nothing. `2X+X+X*X` took like 15 s on the calculator |
| 2026-10-13 | `cas/identities.c/.h` | each identity knows which functions it uses (`func_mask()`) and gets skipped if the expression doesn't have them | it was trying every trig identity on stuff with no trig in it |
| 2026-10-13 | `cas/identities.c/.h` | the functions come from the identity's text (`text_func_mask()`) so skipped ones never get parsed | parsing all of them on the first simplify took ~3 s. `test/identity_masks.c` checks this is safe |
| 2026-10-13 | `cas/factor.c` | `FAC_POLYNOMIAL` (`factor_polynomial()`): one variable polynomials factored over the rationals. runs before the pairwise gcd factoring | was a TODO upstream, and the gcd factoring made ACT Q37 worse |
| 2026-10-13 | `cas/solve.c` (new), `cas/cas.h` | `solve(e, var, out)`, e = 0 over the reals. linear, or polynomials with number coefficients (factor + quadratic formula). no partial answers | upstream doesn't have solve. the parser has no `=` so the app passes `a - b` |
| 2026-10-13 | `cas/eval.c` | roots of whole numbers pull out their square (k-th power) factors (`eval_root_factors()`): `sqrt(3780)` -> `6sqrt(105)` | ACT Q8 |
| 2026-10-13 | `cas/simplify.c` | canonical form splits `2^(3/2)` -> `2sqrt(2)` | simplify merges it back otherwise, so it has to happen at the end |
| 2026-10-13 | `pc/tests.c/.h` | `solve` test type | testing solve |
| 2026-10-14 | `cas/factor.c`, `cas/cas.h`, `pc/tests.c/.h` | `cancel_fractions()`: factor the top and bottom of every fraction and cancel. the app runs it before `simplify()` | simplify splits fractions up before anything can cancel (ACT Q37) |
| 2026-10-14 | `cas/factor.c`, `cas/cas.h`, `pc/tests.c/.h`, `pc/main.c` | `comdenom(e, factored)`: sums with fractions become one fraction, top and bottom expanded or factored | common denominator / factored forms, ACT Q26 |
| 2026-10-14 | `cas/approx.c` (new), `cas/cas.h`, `pc/tests.c/.h` | `approx()` turns π, e, roots, logs, trig etc into numbers, `approx_format()` prints n digits | decimal form. `float` because `long double` is +13 KB on the CE |
| 2026-10-14 | `cas/simplify.c`, `cas/cas.h`, `cas/expand.c`, `cas/factor.c`, `cas/identities.c`, `cas/derivative.c`, `cas/eval.c` | cancelling: the long loops call `pcas_poll`, if it says stop everything returns early and leaves a valid (unfinished) tree | so you can stop a slow answer, and the app can animate while it waits |
| 2026-10-14 | `cas/eval.c` | `eval_commutative()` leaves a sum alone if its only constant is one `a/b` | upstream bug, eval and normalize kept undoing each other forever. `1/(2X)+1/X^2` hung |
| 2026-10-14 | `pc/tests.c/.h` | `MAX_TESTS` 256 -> 1024, says when it skips | it silenty stopped at 256 |
| 2026-10-14 | `cas/identities.h/.c` | identity text is `const char *` instead of fixed size arrays | 16 KB of padding in `.data` crashed the Flash App on launch |
| 2026-10-14 | `cas/expand.c` | $(A+B)^n$ only gets multiplied out if $n$ is a number $\le 64$ | `sqrt(2+i)` segfaulted and `(X+1)^(10^9)` tried to make a billion copies (found in the nasty input sweep) |
| 2026-10-14 | `parser.c` | free stuff properly on parse errors | it leaked |
| 2026-10-14 | `cas/approx.c` | `approx_format()` finds the exponent with integer compares instead of a float log | huge numbers overflowed the buffer |
