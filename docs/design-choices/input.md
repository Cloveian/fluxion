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

basacally like typing on a computer, the cursor is a line and `[on]` is Ctrl. the other keys mean what they mean on the TI-84 (everything is in [`bind_ce.c`](https://github.com/Cloveian/fluxion/blob/main/src/ui/bind_ce.c))

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
| `[alpha]` + `[enter]` | the last answer as a decimal, just that once. the next answer is back in the normal form | |
| `[alpha]` + `[y=]` or `[x,t,θ,n]` | fraction. y= is where the TI has it, the other one's just right there. the term before the cursor goes on top and you're in the bottom (`5+9` -> `5+` 9 over ▯), back to the last `+` `-` `=` `,` or open `(`, so `2x`, `(a+b)` and `sin(x)` all go up whole. nothing there (or a `*` `/` `(-)` right before it) = empty, cursor on top | |
| `(` | just a bracket. its `)` shows up grey at the end of the slot you're in till you type a real one, and if you never do it closes there anyway, so `2(x+1` is 2(x+1) | |
| `)` | closes the last `(` that's still open. none open: steps out of the bracket / function you're in, like before | |
| `[+]` `[-]` `[*]` `[÷]` `[^]` on an empty line | puts `ans` in front, the last answer (like the TI). `ans` is bold like a stored name and typing it out works too. not `[(-)]`, that's a new number. no answer yet = nothing gets added | |
| `[sto→]` | the arrow. `value [sto→] name` stores, `[sto→] name` on its own deletes (see Names below) | |
| `[vars]` | the stored names and their values, enter deletes one | |
| `[mode]` | angle (radians / degrees), trig answers, what simplest means, rationalize, theme (dark, light, contrast, contrast light, green, rose, paper, custom, with six rows under it for its colours). enter flips a row, left / right steps the theme and the custom ones | |
| `[on]` + `[sin]` `[cos]` `[tan]` | csc, sec, cot. real functions, they stay what you typed | |
| `[2nd]` + `[alpha]` + `[on]` | screen off (the backlight). it goes off by itself after 2 min of nothing too. any key brings it back, and that key gets eaten | |
| `[2nd]` + `↑` `↓` | brightness, 8 steps. saved, and the OS gets its own level back on the way out | |

with something selected: typing replaces it, `(` `÷` `√` `sin` etc go *around* it, `^` makes it the base

### Modifiers

`[2nd]`, `[alpha]` and `[on]` are separate, any of them can be on at once. tap = next key, double tap = lock, tap again = off. `[2nd][alpha]` + a letter is a capital, same variable as the lowercase one (nspire's case insensitive too). this replaced `[2nd][alpha]` = alpha lock, double tap `[alpha]` for that now

### History

`↑` from the top of the line highlights the last answer, then its input, then the answer before... `[enter]` pastes it at the cursor, `[on]`+`[2]` copies it, `[clear]` goes back. `←` `→` go inside it, then arrows move and `[alpha]` + arrows select just a part

### Popups and pages

- `[math]`: a grid at the cursor with = ≠ < > ≤ ≥ ! ∞ ° θ, arrows + `[enter]` or `1`-`9` `0`
- `[2nd][0]`: the catalog, split into algebra, calculus, functions, templates, keys and an all list a-z. the five keys under the screen jump to a section, names in the bar at the bottom like the editor. `[alpha]` + letter jumps, `[enter]` types it, `[clear]` closes

### Not done yet

- ~~the full key map (every key x plain / 2nd / alpha / 2nd+alpha / on)~~ it's a table now, `ui/bind_ce.c`, every key and modifier that does something is a row in it
- ~~the catalog's key column is the CE's keys written out by hand, so the nspire leaves it blank (it said `[2nd][x^2]` for √)~~ it comes out of the tables now. the first time the catalog opens, every row of the keypad's table gets typed on a scratch line and so does every item, and the ones that match share a key. so the nspire's says ctrl+x^2, and θ found its `[alpha][3]` that the hand written ones never had

### Menus

The five keys under the screen (`y=` `window` `zoom` `trace` `graph`) open the folders: Algebra, Calculus, Functions, Templates, Tools.

- `↑` `↓` move, `←` `→` switch folders, `[enter]` or `1`-`9` picks, same key again closes it
- picking something just types it at the cursor (a character, a template, `sin(▯)`, `expand(▯)`, etc)
- Tools stuff toggles (✓)
- stuff the engine can't do yet is greyed out with a `*`

## Names

multi-letter variables and typing `sin` to get the function turned out to be the same problem, so it's one thing. there's a table of names: the catalog's words (`sin`, `asinh`, `pi`, `e`, `i`, `theta`, `sqrt`, ...) and anything stored. a run of letters gets split into the longest names from the left, redone on every edit, so `sinh` is one function, `xi` is $x \cdot i$ and `pin` is $\pi n$. if `ab` is stored then typing `a` `b` gives you `ab`, want $a \cdot b$? type `a*b`

the letters stay letters in the editor. the split is just how a run gets read (for the engine) and drawn, which is what makes "redone on every edit" free: the cursor, undo and backspace never knew anything happened. a name is drawn as one thing, bold if it has a value, and turns back into letters while the cursor's inside it so you can see where you are. `[vars]` lists what's stored, enter on a row deletes it. a command typed out, `solve(2x=6,x)` letters and all, gets folded into the same item the folder inserts when you press enter, as long as the brackets are the whole line

storing is `expr [sto→] name`, one letter or more. the name after the arrow is taken literally so you can make new ones. can't store to a builtin (`sin`, `e`, ...), and a loop like `a+1 [sto→] a` gets refused when you store it instead of hanging later. a value can be a whole expression or an equation, which keeps both sides since the engine has no `=`

bold means it has a value. `e`, `i`, anything stored, and a bold name always gets substituted, no exceptions. so `solve(2x=6,x)` with x stored is an error ("x is stored") instead of quietly solving for 5. plain = symbolic. deleting it on the vars page makes it plain again

the app does the substituting before it parses, so the engine still only ever sees single letters and doesnt change at all

gotchas:

- `e` and `i` can't be variables anymore
- on the calc every letter is its own `[alpha]`, so `sin` goes `s`, `si` (bold i!), `sin`. just the display catching up
- history keeps how it looked when you typed it, pasting it back re-splits it

### Decision
one name table. stored = bold = substituted everywhere, single letters can be stored too, `e` and `i` turn into names. done 10-15

## Nspire keys

the nspire build (`make nspire`, Ndless on the CX and CX II). no keys under the screen, and a whole keyboard of letters.

keys don't stand in for other keys anymore, each keypad has a table of key + modifiers -> what it does (type π, the √ template, undo, quit, open the menu...), `ui/bind_ce.c` and `ui/bind_nspire.c`, and the app only ever sees the what. `platform/keys_*.c` just say which key. so the nspire's keys can do what they do in its own OS instead of whatever the CE key in the same spot does (ctrl + ^ was π, cuz that's `[2nd][^]` on a CE (￢ \_￢ ))

### Decision

- `[menu]` opens the folders, ~~left / right go through all five~~ a list of the five drops down from the top and 1-5 opens one, like the OS's own menus. a folder opens a step to the right of it, esc or ← goes back a level. no soft-key bar on the main screen, the history and input get its 15 px
- keys do what they do in the OS (the CX II guidebook's shortcut list), table below
- letters type straight in, no alpha mode. shift + a letter is a capital (same variable, like now)
- `trig` gets a popup with all 12, sin cos tan csc sec cot and their inverses, picked like the `[math]` one (1-9, 0, then arrows). csc⁻¹, sec⁻¹ and cot⁻¹ are real functions for this, on every build (`[on][2nd]` + sin / cos / tan on the CE)

| Key | Plain | ctrl | shift |
| --- | ----- | ---- | ----- |
| `^` | exponent | ⁿ√ | |
| `x²` | squared | √ | |
| `e^x` `10^x` | e^▯, 10^▯ | ln, log | |
| `(-)` | negative | ans | |
| `enter` | simplify | the decimal (≈) | |
| `del` | backspace | clear the line (an empty one clears the screen) | |
| `esc` | close what's open, a level at a time. stops a long one | undo | |
| `=` | = | the = ≠ < > ≤ ≥ popup | |
| `÷` | ÷ | fraction | |
| `+` `-` | | brightness up / down | integral, d/dx |
| `var` | stored names | sto-> | |
| `trig` | popup with all 12 | | |
| `π▸` | popup with π e i ∞ θ ° | | |
| `?!▸` | popup with ! % ° | | |
| `cat` | catalog | the `[math]` popup | |
| template key | Templates folder | | |
| `menu` | the folders, or a page's keys | same | |
| `doc` | settings | | |
| `home` | quit | screen off | |
| `7` `1` | | line start / end (top / end of page in the OS) | |
| letters | type | X C V cut copy paste, Z undo | capital |
| arrows | move | | select |

### Proposal

- ctrl and shift tapped alone stick like `[2nd]` / `[alpha]` on the CE, double tap locks. the padlocks in the bar say C and S
- `[menu]` on a page with keys under its screen (the catalog's sections, the steps page's methods) drops those five down instead, like the OS's ctrl + menu context menu. ctrl + 1-5 did that before but the OS uses ctrl + digits for moving around
- esc and home both stop a long one, the hint says esc (the OS's own break is holding home)
- the touchpad: theres no arrow switches under it, one click for the whole pad and the finger position, so the dirrection comes from where the finger is when it clicks. a push keeps the arrow it started as till it's let go, so sliding while it's down doesn't turn it
- saves go in `/documents/fluxion/`, no `.tns` on them so the nspire's file browser leaves them out

---
Back to [design choices](index.md) · [rendering](rendering.md) · [screens & operations](ui.md)
