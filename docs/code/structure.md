# Structure

## File layout

```
fluxion/
├── makefile
├── icon.png
├── src/
│   ├── main.c  start up, main loop
│   ├── cas/   PineappleCAS engine (vendored, MIT notice kept)
│   ├── imath/  big-number maths, used by the engine
│   ├── render.c    expression tree → 2D math on screen
│   ├── editor.c    input: cursor, templates, keys
│   └── screens.c  menus, history, settings
├── test/    PC build: engine + layout tests with plain gcc
└── docs/
```

Rule: **only `render.c` / `editor.c` / `screens.c` touch the screen or keys.** The engine and the layout maths stay pure so they can be tested on the PC without the emulator.

## Testing

- PC: engine + layout built with `gcc`, takes a few seconds
- CEmu: by hand. `cemu-autotester` comes with CEdev if i ever want scripted key presses

## Gotchas

- **Relative paths to CEmu**: `-s bin/FLUXION.8xp` silently fails when CEmu is already running from another folder. Always pass a full path (VS Code configs use `${workspaceFolder}`).
- **IntelliSense thinks `int` is 32-bit.** On the eZ80 it's 24-bit, trust the compiler's warnings over the editor's.
- **`make debug` after a normal build** says "nothing to be done". `make clean` first.

---
Back to [code](index.md)
