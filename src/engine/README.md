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
| 2026-10-13 | `cas/identities.c/.h` | `id_ExecuteTable` simplifies once up front and after a change, instead of after every identity tried | ~120 wasted full re-simplifies per table pass. 2X+X+X\*X took 14.6 s on the calculator |
| 2026-10-13 | `cas/identities.c/.h` | Each identity remembers which functions its pattern uses (`funcs`, `func_mask()`), and is skipped if the expression doesn't have them | trig/hyperbolic/… identities were tried on expressions with no trig in them. Safe because function nodes only match exactly in `matches()` |
| 2026-10-13 | `cas/identities.c/.h` | The functions are read from the identity's text first (`text_func_mask()`, longest match like the parser), so skipped identities are never parsed | checking the parsed pattern meant parsing all ~120 identities on the first simplify (~2.9 s on the calculator). `test/identity_masks.c` checks text ⊆ parsed for every identity |
| 2026-10-13 | `cas/factor.c` | `FAC_POLYNOMIAL` implemented (`factor_polynomial()`): one variable, rational coefficients, factored over the rationals by content, lowest power, then rational roots. Runs before the pairwise gcd factoring, and again on what that pulls out | upstream left it as a TODO, and the pairwise gcd made ACT Q37 worse (`X^3+X^2+4(-2X_3)`). Tests in `test/fluxion-tests.txt` |
| 2026-10-13 | `cas/solve.c` (new), `cas/cas.h` | `solve(e, var, out)`: e = 0 over the reals. Linear with any coefficients, polynomials with number coefficients via `factor(FAC_POLYNOMIAL)` + the quadratic formula. Returns the roots smallest first, `SOLVE_ALL`, or `SOLVE_UNKNOWN` (never a partial answer) | solve came up 5 times in 45 ACT questions and upstream has none. The parser has no `=`: the app splits the line and passes `a - b` |
| 2026-10-14 | `cas/factor.c`, `cas/cas.h`, `pc/tests.c/.h` | `cancel_fractions()`: every fraction's top and bottom factored (`FAC_POLYNOMIAL`), shared factors cancelled. The app calls it before `simplify()` on enter. `cancel` test type | ACT Q37: `simplify()` splits a fraction into one per term before anything can cancel |
| 2026-10-14 | `cas/factor.c`, `cas/cas.h`, `pc/tests.c/.h`, `pc/main.c` | `comdenom(e, factored)`: every sum with a fraction becomes one fraction over the lcm of the denominators (factored with `FAC_POLYNOMIAL`, each base at its largest power), then cancelled. Top and bottom expanded, or factored. `comdenom` / `comdenomf` test types and CLI commands | the common-denominator and factored forms. ACT Q26: `sqrt(2D/A_2C/A)` → `sqrt(2(D_C)/A)` |
| 2026-10-14 | `cas/approx.c` (new), `cas/cas.h`, `pc/tests.c/.h` | `approx()`: parts without variables that aren't exact (π, e, roots, logs, trig in radians, …) become numbers, computed in `float` and stored as exact binary fractions. Sums, products and fractions stay exact. `approx_format()`: a number as text with n significant digits, rounded in exact integers (`1.25`, `0.333333`, `2.28831E7`). `decimal` test type | the decimal form, ACT Q11, Q45, Q23. upstream has no floating point at all. `float` because the CE's `long double` maths is +13 KB compressed |
| 2026-10-13 | `cas/eval.c` | Roots of whole numbers that aren't perfect powers take their k-th power factors out (`eval_root_factors()`): `sqrt(3780)` -> `6sqrt(105)`. Trial division up to 1000 | ACT Q8. Also makes solve's `x^2=8` give `2sqrt(2)` |
| 2026-10-13 | `cas/simplify.c` | Canonical form splits a whole number to a power over 1: `2^(3/2)` -> `2sqrt(2)`, `2^(5/3)` → `2(3root4)` | `simplify()` merges `2*2^(1/2)` back into `2^(3/2)` as like terms, so the split can only happen at output |
| 2026-10-13 | `pc/tests.c/.h` | `solve` test type: `solve; equation; var; roots` (or `true` / `false` / `unknown`) | so solve is tested like everything else |
