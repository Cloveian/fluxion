# Rendering

How math is displayed on the screen (320\*240)

## Identify
I need to decide how expressions get drawn

### Relevant constraints/nice to haves:
#### Must haves
- 2D math: fractions stacked, exponents raised, roots
- Prettier / more readable than PineappleCAS
- Fits the [platform](platform.md) limits (RAM, program size), and redraws fast enough to feel instant at 48 MHz

#### Nice to haves
- LaTeX-looking (unless that is too heavy)
- Custom font

### What has to be decided
- Layout model
- Font, and sizes (smaller exponents?)
- Multiplication: shown ($2\cdot x$) or hidden ($2x$), and when
- Brackets that grow with what's inside
- Wider than the screen: scroll or wrap
- Taller than the screen
- Colours / theme
- Redrawing: double-buffered, only when something changed?

## Brainstorm

### Options:

**A - Box layout straight from the engine tree**
every node is a box (width, height above the line, height below). measure from the bottom up, then draw. what makes stuff look like LaTeX is mostly the spacing and font sizes anyways, so i can just steal those rules:
- spacing: a bit around $+$ / $=$, none for $2x$, tighter in exponents
- exponents and nested fractions get smaller (needs a small font)
- tree -> LaTeX might still be nice later for testing, but not for drawing

**B - LaTeX string -> small TeX-subset renderer**
engine tree -> LaTeX -> a tiny TeX renderer. closest to latex style, but then i'm parsing something i already had as a tree

**C - Linear pretty-print**
one line (`x^2/(x+1)`) with color and spacing. Easiest, but that's basically what PineappleCAS already does. and the main reason it's 'not great'

### What the toolchain has (looked through the CEdev v15 headers/examples)

- graphx's font is 8\*8 and only scales up by whole numbers, so no small font for exponents with it
- graphx can do custom fonts but they're still 8 px tall
- fontlibc does any height and can be embedded in the program (`convfont`), each size costs space tho
- double buffering is in VRAM so it doesn't eat RAM
- `gfx_SetClipRegion` for the scrolling history

## Select

| Criteria                                   | Weight | A: Box layout | B: LaTeX | C: Linear |
| ------------------------------------------ | :----: | :-----------: | :------: | :-------: |
| Looks like real math (beats PineappleCAS)  |   10   |       9       |    9     |     3     |
| Time to build (10 = fast)                  |   8    |       6       |    3     |     9     |
| Fits RAM / program size / speed            |   6    |       8       |    5     |     9     |
| Works with a 2D editor (cursor <-> boxes)    |   7    |       9       |    4     |     5     |
| LaTeX look                                 |   3    |       6       |    8     |     2     |
| **Weighted total**                         |  340   |    **267**    |   196    |    197    |

### Decision
Box thing


---
Back to [design choices](index.md) · [input](input.md) · [screens & operations](ui.md)
