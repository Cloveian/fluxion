# Screens & operations

What you can do to an expression, and how you get to it

## Identify
I need to decide what the app looks like around the editor

### Relevant constraints/nice to haves:
#### Must haves
- Intuitive, nothing to memorise
- Only what the [engine](platform.md) can actually do: simplify (toggles for basic / trig / hyperbolic / complex identities, evaluating trig and inverse trig constants), evaluate, substitute, expand, derivative. There's also a `factor.c` that isn't exposed as a command
- Copy Paste

#### Nice to haves
- Reading/writing TI variables (`Y1`, `Str1`)
- History that survives quitting

### What has to be decided
- Which operations are in v1, which later
- How you pick one
- Settings: simplify identity toggles, derivative variable
- History: scrolling, reusing an old entry, `Ans`
- Change form: which forms, in what order, which key
- Errors
- Cancelling a long computation (`on`?)
- Leaving the app

## Brainstorm

### Options:

**A - Soft keys**
the five keys under the screen (`y=` ... `graph`) are labelled at the bottom and pick the operation. Always visible, one press. can be like the submenus that pop up when menu is pressed on nspires

**B - Menu list**
`math` key opens a list of operations. Room for more, but one more step every time.

**C - Command line**
type `deriv(x^2, x)` like a TI-89 / PineappleCAS's TI-Basic interface. Powerful, but the opposite of intuitive.

**D - Nspire-style**
same as the Calculator app on my nspire:
- enter = simplify, always
- history scrolls, input on the left, answer on the right. arrow up + enter copies it back down
- everything else is in the 5 keys under the screen, each one is a folder like the nspire's menu. picking something types the command (`expand(▯)`), or you can just type it
- so kinda A + C

Folders, first guess (`*` = not in the engine yet, hybrid work or later):

| Key      | Folder    | Inside                                                           |
| -------- | --------- | ---------------------------------------------------------------- |
| `y=`     | Algebra   | expand, substitute, evaluate, factor\*, solve\*                  |
| `window` | Calculus  | d/dx, ∫\*, limit\*                                               |
| `zoom`   | Functions | trig + inverses, hyperbolic, ln, log, log base, abs, !           |
| `trace`  | Templates | fraction, √, ⁿ√, exponent, abs, piecewise\*                      |
| `graph`  | Tools     | settings (identity toggles, derivative variable), clear history  |

### Change form

the nspire just picks a form and if you want a diffrent one you have to know `expand` / `factor` / `comDenom` / etc. i want to just be able to switch (examples in [simplify cases](../simplify-cases.md))

first idea:
- enter gives what you'd write by hand
- ← → on an answer in history goes through the other forms, with a little label (`factored 2/4`)
- skip forms that come out the same
- only work them out when you actually press ← / → (it's 48 MHz, can't do all of them every enter)

Forms (`*` = needs engine work, `?` = haven't checked if PineappleCAS can):

| Form | Example |
| ---- | ------- |
| by hand (default) | $-\frac{1}{2\sqrt{10\pi}}$ |
| expanded | $x^2 - 1$ |
| factored\* | $(x-1)(x+1)$ |
| common denominator? | $\frac{x+1}{x}$ instead of $1 + \frac{1}{x}$ |
| rationalized? | $\frac{-\sqrt{10\pi}}{20\pi}$ (the nspire's pick) |
| partial fractions\* | $\frac{1}{x-1} - \frac{1}{x}$ |
| decimal? | $-0.0892$ |

#### Decision
~~cycle per answer~~ a mode instead. the key switches the mode (like expanded -> factored), redraws the last answer in it, and everything after comes out that way too. doing the engine part first, haven't picked the key or the order yet

### What the engine gives

- has: simplify (6 identity toggles), evaluate, substitute, expand, derivative. that's it
- doesn't have: solve, integrate, factor (`factor.c` is only used internally). `solve(` and `factor(` are like the first things i'd use on the nspire so i have to add them (hybrid engine) or they're not in v1
- no way to cancel, PineappleCAS's README literally says a hard input can take so long you have to RAM reset. would have to check `[on]` in the engine's loops

## Select

| Criteria                        | Weight | A: Soft keys | B: Menu list | C: Command line | D: Nspire-style |
| ------------------------------- | :----: | :----------: | :----------: | :-------------: | :-------------: |
| Intuitive / nothing to memorise |   10   |      9       |      7       |        3        |        9        |
| Feels like the nspire           |   7    |      7       |      7       |        6        |       10        |
| Time to build (10 = fast)       |   8    |      7       |      8       |        7        |        5        |
| Room for more operations        |   4    |      8       |      9       |       10        |        9        |
| Fewest key presses              |   5    |      8       |      5       |        6        |        8        |
| Need to invent (10 = copy it)   |   5    |      7       |      8       |        8        |        9        |
| **Weighted total**              |  390   |     302      |     284      |       238       |     **321**     |

### Decision
D - Nspire style. I am used to it, and its a pretty good system that i don't have to design the mechanism from scratch.


---
Back to [design choices](index.md) · [rendering](rendering.md) · [input](input.md)