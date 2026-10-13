# Input

How math gets typed in

## Identify
I need to decide how the editor works

### Relevant constraints/nice to haves:
#### Must haves
- Mostly from scratch, not the OS's input
- Intuitive (the main complaint about PineappleCAS)
- Feeds the [engine](platform.md)
- Copy paste

#### Nice to haves
- Keys mean what they already mean on a TI (`÷`, `^`, `x,t,θ,n`, `2nd x²` -> √)

### What has to be decided
- Editing model
- Key mapping
- Cursor: moving into and out of fractions / exponents / roots, up/down
- `del` on an empty slot
- Variables: single letters only?
- Functions: `sin` inserts `sin(` with the cursor inside?
- Implicit multiplication ($2x$, $x(x+1)$)
- Max expression size
- Undo
- Editor -> engine: write a string and use PineappleCAS's parser, or build its tree directly
- Fallback if the editor runs late

## Brainstorm

**Gate cuts:**

- TI-OS string input (`os_GetStringInput`): it's the OS's input, the whole point is not using it

### Options:

**A - Structured 2D editor (MathPrint-style, with fixes)**
typing `÷` makes a fraction with empty slots you move between (like MathPrint / NumWorks). probly feels the best but it's also the hardest part. everyone with a TI-84 already knows MathPrint so keep what's good about it:
- keep: what the keys do, empty slots, → to get out of a slot, ↑↓ for top/bottom of a fraction, `2nd enter` for last entry
- fix: getting back *into* an exponent/fraction is annoying, `del` on templates is weird, no undo, slow on long stuff, and no copy paste

**B - Linear text + live 2D preview**
type on one line, the 2D version renders underneath as you type. Much simpler, still looks good.

**C - Nspire-style**
what i'm actually used to (nspire CX II CAS). Somewhere between A and B:
- templates for the stuff that's awkward linear (`^`, `√`, and a template menu for things like integrals / piecewise)
- plain typing for most things, `/` turns into a stacked fraction once it's shown
- shift + arrows to select, ctrl + C / V -> on the TI that's the `[on]` chords
- arrow up into history, enter copies that entry / result into the input line
- less template-juggling on a TI keypad than MathPrint

### What the toolchain/engine has

- `keypadc` reads the raw keypad so i have to do key repeat and 2nd/alpha myself, but `os_GetCSC()` can't do chords so whatever
- `[on]` can be a modifier, `kb_On` is separate from the rest of the keys (Cesium does `[on]` shortcuts too). have to reset it at start tho, it carries over from the last program
- PineappleCAS's `parse()` takes a plain string, so the editor can just make a string
- `export_to_binary()` goes the other way, so i could write answers to `Y1` / `Str1` later

## Select

| Criteria                       | Weight | A: Structured 2D | B: Linear + preview | C: Nspire-style |
| ------------------------------ | :----: | :--------------: | :-----------------: | :-------------: |
| Intuitive / feel               |   10   |        9         |          6          |       10        |
| Time to build (10 = fast)      |   8    |        4         |          8          |        6        |
| Feels like the TI already does |   6    |        9         |          5          |        6        |
| Copy paste fits in             |   6    |        7         |          8          |        9        |
| Same boxes as the renderer     |   5    |        9         |          6          |        8        |
| Need to invent (10 = copy it)  |   5    |        7         |          9          |        8        |
| **Weighted total**             |  400   |       298        |         277         |     **318**     |

### Decision
C - Nspire style, adapted for the difference in keys

---
Back to [design choices](index.md) · [rendering](rendering.md) · [screens & operations](ui.md)
