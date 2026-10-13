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
