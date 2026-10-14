# Log

## 2026-10-14 - sweep fixes, ready to push

- solve now writes the roots like i wanted in case 3 ٩(•̀ᗜ•́)و : $x = \frac{-3-2\sqrt{6}}{5}, \frac{-3+2\sqrt{6}}{5}$. one $x =$, commas, each root one fraction. $\pm$ version will be a form later
- cancel is `[on]`+`[mode]` (Ctrl+Esc on the PC)
- did a nasty input sweep, basically threw ~300 cursed inputs at every command with ASan on and the heap capped like the calculator's. fixed:
	- layout was exponential in nesting depth because a `MAX` macro called a recursive function twice. 16 nested `sin(` froze it, even just typing them
	- expand segfaulted on stuff like `sqrt(2+i)`
	- parse errors leaked memory
	- the decimal formatter overflowed on huge numbers
- still broken from the sweep (not fixing these yet):
	- running out of memory anywhere crashes, the engine never checks malloc
	- $(x+1)^n$ expands into $2^n$ terms before combining, $(x+1)^7$ already fills the heap
	- solve expands factored equations first, so $(x-1)(x-2)\cdots(x-20)=0$ never finishes
	- sorting is $O(n^2)$ with a gcd every compare, so big sums are slow
	- nested derivatives blow up fast
- README, LICENSE and screenshots for making the repo public
- noticed: functions print first in products ($\cos(x)\,x^2$), and derivatives of roots rationalize when they shouldn't (case 1 again)

## 2026-10-14 - flash app

- keypad on the calculator: key repeat, and a quick double tap of the same number doesn't get eaten while it redraws anymore
- sticky 2nd / alpha / `[on]` (shows as `ctl`): tap once for the next key, double tap to lock, tap again to unlock. [2nd][alpha] is alpha lock
- status bar on top with the modifiers, a spinning dot when it's thinking, and battery
- PC keys work like the calculator now (Shift = alpha, Alt = 2nd, Ctrl = `[on]`)
- `make installer` builds it as a Flash App + CEaShell-Installer (submodule, no changes to it). it runs from flash now so there's way more heap
- took forever to figure out: the app crashed on launch before `main` even ran. turns out a Flash App copies its `.data` into RAM at launch and the identity tables were ~16 KB of it, mostly empty padding in fixed size strings. made them `const char *` and it's fine (╯‵□′)╯︵┴─┴
- CEaShell-Installer doesn't have a licence file, CEaShell is GPL-3.0. ok to build and test with, need to ask RoccoLox before actually shipping it

## 2026-10-14 - no more freezes, cancelling

- found the freeze: $\frac{1}{2x}+\frac{1}{x^2}$ (and anything with a plain number fraction in a sum) looped forever. PineappleCAS's eval turned $\frac{1}{2}$ into a number and said nothing changed, then normalize turned it back, forever. upstream bug. those cases are in `fluxion.tests` now
- there's a `working...` thing with moving dots while the engine runs so it doesn't look frozen
- the engine can be cancelled now, it checks a poll function in all the long loops and bails out. no key for it yet
- haven't checked how often the poll runs on the actual calculator

## 2026-10-14 - more engine tests

- `fluxion.tests` went from 68 to 256 tests, every feature + edge cases
- `todo.tests` is a list of stuff the engine gets wrong right now (not run by make)
- the test runner silently stopped at 256 tests per file?? (・_・ヾ so it said 256/256 when there were more. raised it and it says when it skips now

## 2026-10-14 - engine for the forms

- decided forms are a mode (see [ui](../design-choices/ui.md)). engine first, the key isn't picked yet so nothing's wired up
- common denominator: $\frac{1}{x}+1 \to \frac{x+1}{x}$, and it can do factored too, which fixes ACT Q26
- decimal: $\pi$, $e$, roots etc. become numbers, 6 digits like the nspire. Q11, Q45 and Q23 work now
- had to use `float`, `long double` maths was +13 KB and i don't have that. the last digit can be off by one sometimes, oh well. only the irrational bits are floats, the rest stays exact
- renamed the test files to `.tests` since they're their own format
- noticed: $\sqrt[3]{-8}$ gives the complex root instead of $-2$ like the nspire does in real mode

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
