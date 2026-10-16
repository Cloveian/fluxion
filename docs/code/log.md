# Log

## 2026-10-15 - out of memory, bold / italic, E

- running out of memory doesn't crash anymore. every malloc in the engine goes through `pcas_malloc`, a NULL sets `pcas_oom` and the cancel flag, so it all unwinds like a cancel and the app says "out of memory". imath hands out a static number instead of NULL: the cas code makes scratch numbers and feeds them to imath straight away, so that was ~400 crash sites in one go
- found the rest with fault injection, `PCAS_FAIL_ALLOC=n` fails the nth allocation on the PC. first sweep was 12600 runs: 22 crash sites, 420 imath asserts, and one heap overflow that was my own bug (`read_num` bailed before setting `*consumed`, oof). guarded the sites, then made every recursive function return early once the flag's set, which cas.h always claimed they did. had to, some inputs allocate 16000+ times and guarding derefs one at a time was never going to end
- +834 bytes on the calculator. haven't timed the extra checks on it yet
- `[2nd][,]` types E, `2E3` -> 2000
- the form key: `[2nd][enter]` cycles by hand -> expanded -> factored -> decimal, `[alpha][enter]` jumps to decimal and back. it's a mode, it redoes the last answer (runs its input again from the scratch editor, so a half typed line is left alone) and shows in the top bar. decimal is `approx()` plus the layout printing numbers as 0.333333. that's the last four form rows on the scoreboard, 18 of 19 now
- expand collects like terms after every step now. (x+1)^7 took 5 min on the calculator (62 s at 500%) and ^8 ran out of memory, because it built all 2^n raw terms first. on the PC ^8 went from 144 ms to 27 ms (it hardly noticed, the calculator is where it hurts). it's a flag simplify's own expands dont pass, collecting everywhere broke two complex tests
- 2nd / alpha / on tapped while its working count for the next key now, busy_poll feeds them into the modifier state, polling at 40 Hz so a quick tap doesnt fall between two polls. cant check that one on the PC, scripted keys never reach keys_peek
- `[on][clear]` on an empty line clears the screen. the history scrolls up out of view, up brings it back, nothing gets deleted
- names. type `s` `i` `n` and its sin, `p` `i` is pi, letters get cut into the longest names from the left on every edit. the catalog grew a `word` field so its the name table. `value [sto->] ab` stores, `[sto->] ab` deletes, `[vars]` lists them. stored = bold = substituted everywhere (live, loops refused when you store), equations only inside solve, solving for a stored letter is an error. letters stay letters in the editor and the split is a view over the run, so cursor / undo / backspace didnt need touching. typed commands like `solve(` arent picked up yet
- bold and italic in the draw layer (`draw_text_styled`, `box_t.style`). nothing uses them yet and widths aren't synced, so styled text is a pixel or two off. `[2nd][clear]` clears the line like `[on][clear]`
- roadmap written up with the decisions so far: names (multi-letter vars and typeable functions are one thing, bold = has a value), RAM appvars for storage, the by-hand default, cot/sec/csc as real functions

## 2026-10-15 - math popup, catalog, history browsing

- holding 2nd / alpha with another key works on the calculator now. the scan only finds the first key down and those two come first, so holding alpha hid the arrow. they're read on their own like `[on]` now, a tap counts on release
- `[math]` pops up a little grid at the cursor: = ≠ < > ≤ ≥ ! ∞ ° θ, 1-9/0 pick straight away
- catalog on `[2nd][0]` like the TI: everything a-z, the keys for it on the right, what it does at the bottom, alpha + letter jumps. it's built on a new page thing (`ui/page.c`) so a mode menu can use it later
- deleting a template keeps whats in it: backspace at the start of `sin(x+1)` leaves `x+1`, fractions go inline with brackets if they need them
- history: up goes answer, input, answer before... enter pastes, `[on]`+2 copies, left/right goes inside it to select just a bit. answers get turned back into editor stuff from their boxes (whats on screen) so they paste back the same

## 2026-10-14 - select, copy paste, undo, history

- 2nd / alpha / `[on]` can all be on at once now, `[2nd][alpha]` + letter is a capital. capitals are the same variable as lowercase (nspire does that too). alpha lock is double tap alpha now
- `[alpha]` + arrows selects (Shift+arrows on the PC). it goes item by item and grabs the whole template at the edge of a slot. typing replaces it, templates wrap it, so select `x+1` then `÷` is $\frac{x+1}{\square}$
- `[on]` + 1 / 2 / 3 is cut / copy / paste, `[on]` + `[sto→]` undo. undo keeps copies of the whole line (16 steps), typing a run of characters counts as one
- `↑` off the top goes into the history, enter pastes that entry's input back. not the answer yet, that needs the result turned back into editor stuff
- `[2nd]` + left/right is start/end of the line
- font has ≤ ≥ ≠ ∞ ° now for the `[math]` popup, which isn't done
- +2.7 KB compressed. doesn't matter much since it's an app now, no 64 KB program limit. haven't timed undo copies on the calculator

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
- stuff that doesn't cancel still gets split into one fraction per term, that's the common denominator form problem again

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
- calculator build works on toolchain v15 with no changes, but it's 82 KB so it's compressed now (one ~30 KB `.8xp` instead of 3 files)
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
