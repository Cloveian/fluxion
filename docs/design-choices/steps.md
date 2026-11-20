# Show the work

Steps like you'd write them on paper, for an answer that's already in the history.

## What it is

- the moves a person makes, *not* what the engine did. simplify does milions of tiny rewrites and none of them look like homework, so steps get worked out again by their own code, one paper move at a time, with a light tidy after each one (like terms, number arithmetic, nothing that reorders or spreads fractions)
- on demand: nothing gets stored with the answer. the steps get worked out when i open them and thrown away when i close them, so the 12 KB history save doesn't care
- the last line has to match what solve said, thats what the tests check
- something it doesn't know the shape of says so, no steps beats steps that look wrong

## Looks

one line per equation, and the thing done to both sides on its own line in the middle, centered:

```
3x+5=-3
  -5
 3x=-9
  ÷3
  x=-3
```

needs ± and ÷ in the font (`tools/bdf2c.py`). a move that isn't the same thing done to both sides (factoring, the formula) just gets the next line

### Decision
the lines with the move centered between them, like above

## Getting there

a specific button or a menu option on a highlighted history row. the row's input gets read again, so it works on any answer still in the history

### Decision
from a history row. which key exactly: a "steps" item in a menu for now, a key once i know which one is free

## Methods

the same equation can be done more than one way, so there's a method menu on the steps page, with the ones that don't fit greyed out

| Method | Fits when | Notes |
| ------ | --------- | ----- |
| square root | no x term, 2x^2=18 | |
| factoring | factors over the integers | shown as the ac method, *not* the rational root search the engine does |
| complete the square | always | nicest when a is 1 and b is even |
| quadratic formula | always | a, b and c written in first |
| optimal | always | the first one that fits, in that order |

no real roots: the last steps get a square root of a negative and carry on with i, the app's solve gives the complex roots too (there's no real mode)

### Decision
opens on optimal every time, the others are in the menu

## First cut

linear and quadratics (all of the methods above), the steps page, and a `steps;` test type. after that *dih*rivatives, limits, the rest of calc 1, then the rest of algebra (~25-35 sessions through calc 1, from the 2026-11-19 log)

### Decision
linear + quadratics first

## Derivatives

the engine already does a *dih*rivative one rule at a time (`derivative.c` rewrites d/dx nodes by a table), so the steps are that, a line at a time:

```
d/dx(x^2 sin x)
   product rule
x^2 d/dx(sin x) + sin x d/dx(x^2)
   power rule, trig rule
x^2 cos x + 2x sin x
```

- every d/dx still in the line gets one rule per line, so it goes breadth first and stays short (3-5 lines)
- d/dx of a constant or of ax+b goes without a line, on paper nobody writes d/dx(2x+1) = 2 out
- 3x^2 is the constant multiple rule, not the product rule with a d/dx(3) to throw away
- chain rule gets named next to the rule that left a d/dx of something that isn't just x
- the last line gets like terms collected if that makes it shorter, not enter's full simplify: that one takes ~7 s on the CE for x^2 sin x, and turns (1-ln x)/x^2 into 1/x^2 - ln(x)/x^2
- d/dx at a value (deriv(f,x,3)) has no steps yet
- d/dx(x) on its own is 1 by the power rule (x is x^1), a proposal like the rest

### Proposal
the rule names centered between the lines, every d/dx one rule a line. a default, not decided yet

---
Back to [design choices](index.md)
