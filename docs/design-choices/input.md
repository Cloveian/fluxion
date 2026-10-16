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

## Keys

basically like typing on a computer, the cursor is a line and `[on]` is Ctrl. the other keys mean what they mean on the TI-84 (everything is in [`catalog.c`](https://github.com/Cloveian/fluxion/blob/main/src/ui/catalog.c))

### Editing

| Key | Does | Computer equivalent |
| --- | ---- | ------------------- |
| `[clear]` | delete left of the cursor (a template on the left: step into it; an empty template: remove it) | Backspace |
| `[del]` | delete right of the cursor (a whole template if it's one) | Delete |
| `[on]` + `[clear]` | clear the whole line | Ctrl+Backspace |
| `←` `→` | move, into and out of templates slot by slot | arrows |
| `[2nd]` + `←` `→` | start / end of the line | Home / End |
| `↑` `↓` | top / bottom of a fraction, in / out of an exponent. `↑` with nowhere to go goes into the history | |
| `[alpha]` + `←` `→` | select, a whole item at a time. at the edge of a slot it grabs the whole template | Shift+arrows |
| `[on]` + `[1]` `[2]` `[3]` | cut, copy, paste | Ctrl+X C V |
| `[on]` + `[sto→]` | undo (a run of typing is one step) | Ctrl+Z |
| `[enter]` | simplify | |
| `[2nd]` + `[enter]` | next form (by hand, expanded, factored, decimal). redoes the last answer and sticks | |
| `[alpha]` + `[enter]` | decimal form, and back | |
| `[sto→]` | the arrow. `value [sto→] name` stores, `[sto→] name` on its own deletes (see Names below) | |
| `[vars]` | the stored names and their values, enter deletes one | |

with something selected: typing replaces it, `(` `÷` `√` `sin` etc go *around* it, `^` makes it the base

### Modifiers

`[2nd]`, `[alpha]` and `[on]` are separate, any of them can be on at once. tap = next key, double tap = lock, tap again = off. `[2nd][alpha]` + a letter is a capital, same variable as the lowercase one (nspire's case insensitive too). this replaced `[2nd][alpha]` = alpha lock, double tap `[alpha]` for that now

### History

`↑` from the top of the line highlights the last answer, then its input, then the answer before... `[enter]` pastes it at the cursor, `[on]`+`[2]` copies it, `[clear]` goes back. `←` `→` go inside it, then arrows move and `[alpha]` + arrows select just a part

### Popups and pages

- `[math]`: a grid at the cursor with = ≠ < > ≤ ≥ ! ∞ ° θ, arrows + `[enter]` or `1`-`9` `0`
- `[2nd][0]`: the catalog, everything a-z with what it does. `[alpha]` + letter jumps, `[enter]` types it, `[clear]` closes

### Not done yet

- the full key map (every key x plain / 2nd / alpha / 2nd+alpha / on)

### Menus

The five keys under the screen (`y=` `window` `zoom` `trace` `graph`) open the folders: Algebra, Calculus, Functions, Templates, Tools.

- `↑` `↓` move, `←` `→` switch folders, `[enter]` or `1`-`9` picks, same key again closes it
- picking something just types it at the cursor (a character, a template, `sin(▯)`, `expand(▯)`, etc)
- Tools stuff toggles (✓)
- stuff the engine can't do yet is greyed out with a `*`

## Names

multi-letter variables and typing `sin` to get the function turned out to be the same problem, so it's one thing. there's a table of names: the catalog's words (`sin`, `asinh`, `pi`, `e`, `i`, `theta`, `sqrt`, ...) and anything stored. a run of letters gets split into the longest names from the left, redone on every edit, so `sinh` is one function, `xi` is $x \cdot i$ and `pin` is $\pi n$. if `ab` is stored then typing `a` `b` gives you `ab`, want $a \cdot b$? type `a*b`

the letters stay letters in the editor. the split is just how a run gets read (for the engine) and drawn, which is what makes "redone on every edit" free: the cursor, undo and backspace never knew anything happened. a name is drawn as one thing, bold if it has a value, and turns back into letters while the cursor's inside it so you can see where you are. `[vars]` lists what's stored, enter on a row deletes it. typed commands (`solve(` as letters) aren't picked up yet, those still come from the folders (todo)

storing is `expr [sto→] name`, one letter or more. the name after the arrow is taken literally so you can make new ones. can't store to a builtin (`sin`, `e`, ...), and a loop like `a+1 [sto→] a` gets refused when you store it instead of hanging later. a value can be a whole expression or an equation, which keeps both sides since the engine has no `=`

bold means it has a value. `e`, `i`, anything stored, and a bold name always gets substituted, no exceptions. so `solve(2x=6,x)` with x stored is an error ("x is stored") instead of quietly solving for 5. plain = symbolic. deleting it on the vars page makes it plain again

the app does the substituting before it parses, so the engine still only ever sees single letters and doesnt change at all

gotchas:

- `e` and `i` can't be variables anymore
- on the calc every letter is its own `[alpha]`, so `sin` goes `s`, `si` (bold i!), `sin`. just the display catching up
- history keeps how it looked when you typed it, pasting it back re-splits it

### Decision
one name table. stored = bold = substituted everywhere, single letters can be stored too, `e` and `i` turn into names. done 10-15

---
Back to [design choices](index.md) · [rendering](rendering.md) · [screens & operations](ui.md)
