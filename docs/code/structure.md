# Structure

## File layout

```
fluxion/
├── makefile    calculator build (CEdev), compressed to one .8xp
├── pc.makefile    PC build of the engine: `make -f pc.makefile test`
├── icon.png
├── src/
│   ├── main.c  start up, main loop
│   ├── engine/   PineappleCAS engine (MIT, see its README for what's changed)
│   │   ├── cas/   simplify, expand, factor, derivative, identities
│   │   ├── imath/  big-number + rational maths
│   │   └── pc/  PC-only command line + test runner (empty on the calculator)
│   ├── render/
│   │   └── layout.c    expression tree -> boxes with sizes + positions (no drawing, runs on PC)
│   ├── editor.c    input: cursor, templates, keys
│   └── screens.c  menus, history, settings
├── test/
│   ├── pcas-tests.txt    PineappleCAS's 256 engine tests
│   ├── identity_masks.c   checks the identity skip is safe
│   └── layout_dump.c   prints the box tree: ./build-pc/layout-dump '(X+1)^2/X'
└── docs/
```

Rule: **only `render.c` / `editor.c` / `screens.c` touch the screen or keys.** The engine and the layout maths stay pure so they can be tested on the PC without the emulator.

## Testing

- PC: engine + layout built with `gcc`, takes a few seconds
- CEmu: by hand. `cemu-autotester` comes with CEdev if i ever want scripted key presses

## Gotchas

- CEmu + relative paths: `-s bin/FLUXION.8xp` silently does nothing if CEmu is already open from another folder. use the full path (VS Code configs use `${workspaceFolder}`)
- IntelliSense thinks `int` is 32-bit, it's 24-bit on the eZ80. trust the compiler not the editor
- `make debug` right after a normal build says "nothing to be done", `make clean` first
- over 64 KB the toolchain splits it into a launcher + appvars (3 files to send), `COMPRESSED = YES` keeps it as one `.8xp`
- engine syntax: capital single letter variables, `_` is subtract, `-` is negate (TI style)

---
Back to [code](index.md)
