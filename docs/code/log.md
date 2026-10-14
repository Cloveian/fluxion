# Log

## 2026-10-14 - engine for the forms

- **Forms are a mode** (decision in [ui](../design-choices/ui.md)): the key changes the mode, the last answer is redrawn in it, and answers after it come out that way. Engine first. The key and the order of the modes are still open, so nothing is wired to the app yet.
- **Common denominator** (`comdenom()`): every sum with a fraction becomes one fraction over the lcm of the denominators, then cancelled. `1/x+1` → `(x+1)/x`, `1/(x−1)+1/(x²−1)` → `(x+2)/(x²−1)`. Expanded top and bottom like the nspire's `comDenom`, or factored like its `factor`: ACT Q26 `√(2d/a − 2c/a)` → `√(2(d−c)/a)`.
- **Decimal** (`approx()` + `approx_format()`): π, e, roots, logs, trig (radians) become numbers, shown to 6 significant digits like the nspire's Float 6. Q11 → `100.9`, Q45 → `1.25`, Q23 → `2.28831E7`. Plain from 0.001 to 999999, scientific outside that (the TI-84's lower bound; haven't checked the nspire's).
- **float, not long double**: the CE's 64-bit `long double` maths is +13 KB compressed, more than is left. `float` is about 7 digits, so the 6th can be off by one after a lot of cancelling (`e^15 − 3269017`). Only the irrational parts are floats, sums and products stay exact. The PC uses float too, so the tests see what the CE will do.
- **Size, once wired**: +3.7 KB (comdenom 0.65, decimal 3.0) → 58.6 KB of 64. Not linked until the app calls them, so `FLUXION.8xp` is still 54.9 KB.
- **Not timed in CEmu.**
- **Found**: `∛(−8)` simplifies to `1+i√3`, the principal complex root. The nspire in Real mode says −2. `√2/2+1/x` comes out `(x+√2)√2/(2x)` in common denominator: rationalizing again (case 1).

## 2026-10-14 - cancelling in fractions

- enter cancels common factors now: $\frac{x^2-1}{x-1} \to x+1$, ACT Q37 gives $(x+2)^2$
- the ACT wants $x^2+4x+4$ for that but $(x+2)^2$ is smaller so it stays, expanded will be a form
- stuff that doesn't cancel still gets split into one fraction per term, that's the common denominator form problem agian

## 2026-10-13 - square factors out of roots

- $\sqrt{35\cdot108} \to 6\sqrt{105}$ (ACT Q8), $\sqrt{72} \to 6\sqrt{2}$, etc
- had to do it in two places because simplify keeps merging $2\sqrt{2}$ back into $2^{3/2}$
- noticed: a fraction in an exponent ($54^{1/3}$) is drawn big and low so it looks like the mixed number $54\frac{1}{3}$ lol

## 2026-10-13 - solve

- `solve(2x+3=7,x)` from the Algebra folder, `=` is `[2nd][math]` (TEST on the TI, `=` is the first thing in it). if you leave out the variable it uses x
- does linear (any coefficients) and polynomials with number coefficients: factor it, then each factor is linear or a quadratic. if there's a cubic left it says it can't solve it instead of giving half an answer
- the parser doesn't know `=`, so the app splits the line itself
- not yet: systems (Q14), inequalities, $ax^2+bx+c$ with letters

## 2026-10-13 - polynomial factoring

- `factor(` works on one variable polynomials like the nspire, pulls out every rational root. ACT Q37 -> $(x+2)^2(x-3)$, Q5 -> $(x+5)(x-6)$
- it was a TODO in PineappleCAS. their gcd factoring actually made Q37 worse so mine runs first ᕦ(¬‿¬)ᕤ
- first version was +10 KB because of big local arrays (the eZ80 is really bad at big stack frames), moved them to the heap and it's ~3 KB
- tests in `test/fluxion-tests.txt` (now `fluxion.tests`)
- $x^2\sin(x)+2x\sin(x)+\sin(x)$ still comes out half factored

## 2026-10-13 - fonts + layout pass

- three font sizes now like TeX (normal / script / scriptscript), from the X11 misc-fixed fonts, `tools/bdf2c.py` converts them. replaced the 8\*8 graphx font
- variables are lowercase like the nspire. $e$ and $i$ are bold so you can tell the constants apart from variables named e and i
- nested fractions get smaller, roots and brackets got some spacing fixes, brackets curve so tall ones stay round
- terms are sorted highest degree first
- `--raw` in the viewer draws exactly what you typed, for layout stuff

## 2026-10-13 - drawing, PC screen

- `src/platform/screen.h` is the only thing that touches the screen. graphx on the calculator, a framebuffer + SDL3 window on the PC
- own 8\*8 font so the PC and calculator draw the exact same pixels (checked, CEmu matches)
- `./build-pc/view 'X^2/2'` opens a window, `--png` saves a picture
- the calculator needs `clibs.8xg` (added to the shipping checklist)
- tried building as an app (.8ek), it builds but you need an installer to put it on, even in CEmu
- nested fractions look bad, same width bars. need the small font

next: the editor

## 2026-10-13 - box layout

- `src/render/layout.c` turns the engine tree into boxes (width, height above/below the line, position). fractions, exponents, roots, brackets that grow, log bases, etc. no graphx in it so it runs on the PC
- spacing stolen from TeX
- the font is a parameter so swapping fonts later doesn't touch the layout
- `./build-pc/layout-dump` prints the box tree
- the engine splits $\frac{x^2-1}{(x+1)^2}$ into two fractions, same form problem as Q26

next: actually drawing it

## 2026-10-13 - PineappleCAS engine in

- copied PineappleCAS's engine into `src/engine/` (everything but its UI). what i change goes in `src/engine/README.md`
- PC build passes all 256 of its tests
- calculator build works on toolchain v15 with no changes, but it's 82 KB so it's compresed now (one ~30 KB `.8xp` instead of 3 files)
- turned off the `missing-field-initializers` warning, there were 146 of them all from the engine
- the $\sqrt{10\pi}$ rationalizing thing is just one flag, `CANONICAL_RATIONALIZE`

next: check it in CEmu, then rendering

## 2026-10-12 - set up, hello world runs

- CE C toolchain v15 in `~/CEdev`, makefile adds it to PATH itself so VS Code just works
- CEmu is `ti-cemu` on here (`cemu` is the Wii U emulator lol). ROM is my own dump, gitignored, never commit it
- OS 5.8.0 so arTIfiCE 2.1, programs through Cesium
- hello world runs
- CEmu send was being annoying, a relative path silently fails if CEmu is already open from another folder. VS Code configs use `${workspaceFolder}` now
- leaning towards PineappleCAS's engine so the time goes into rendering + input, see [platform](../design-choices/platform.md)

---
Back to [code](index.md)
