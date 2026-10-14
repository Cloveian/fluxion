# Log

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
