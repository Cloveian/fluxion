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
- 3x^2 is the constant multiple rule, not the product rule with a d/dx(3) to throw away. same for x^2/2 and the quotient rule, a bottom with no x is a constant (log(x) is ln(x)/ln(10) like that)
- chain rule gets named next to the rule that left a d/dx of something that isn't just x
- the tidy after every line collects like terms, not enter's full simplify: that one takes ~7 s on the CE for x^2 sin x, and turns (1-ln x)/x^2 into 1/x^2 - ln(x)/x^2
- d/dx at a value (deriv(f,x,3)) is the same rules at x, then the value plugged in as written and what it comes to: 2x, 2\*3, 6. if there's no x left by then (d/dx(3x) is just 3) there's nothing to plug in
- d/dx(x) on its own is 1 by the power rule (x is x^1), a proposal like the rest

### Proposal
the rule names centered between the lines, every d/dx one rule a line. a default, not decided yet

## Limits

limits are new in the engine too (`limit(f, x, a)`), and the steps come out of the same loop that works the answer out, so they can't *dih*sagree:

```
lim x->0 (1-cos x)/x^2
   0/0, l'Hopital
= lim x->0 sin x / 2x
   0/0, l'Hopital
= lim x->0 cos x / 2
   plug in
= cos(0)/2
= 1/2
```

- plug in first when f is a value at a. "a value" is checked on f as it is: no 0 below, ln(0), 0^0, tan(pi/2), and so on, because a simplify after can eat it (0 e^(1/0) came out 0)
- not a value there: one fraction, then on 0/0 factor and cancel, and only when nothing cancels l'Hopital. 6 tries at most
- a number over 0 has no limit, it stays lim(...) on the screen. one-sided limits aren't in yet
- it's drawn lim with x->a lowered next to it like a log's base, not under it. the layout has no box for under-and-centered
- "l'Hopital" has no accent, the font hasn't got the o with the hat

### Proposal
plug in / one fraction / 0/0 factor and cancel / 0/0 l'Hopital as the rule names, x->a as a subscript. a default, not decided yet

### Proposal: x -> ∞

- ∞ is the one from the [math] popup, and it's only allowed as where a limit heads (`limit(f,x,∞)` or `-∞`). anywhere else it's an error, cuz simplify would take it for a letter and ∞ - ∞ would come out 0
- the answer can be ∞ or -∞, but only when the limit is the whole input. lim(x^2) + 1 stays as it is, same reason
- worked out piece by piece: each part heads to a number, ∞, -∞, or 'no idea'. ∞/∞ goes by the top terms for polynomials and l'Hopital otherwise, ∞ - ∞ by the top term or as one fraction, 0 \* ∞ turned into a fraction, and x in both the base and the power (like (1+1/x)^x) as e^(power \* ln(base))
- -∞ is ∞ with x turned around (x -> -x)
- the steps are just one line for now, "plug in inf" and then the answer. the paper way (*dih*vide top and bottom by x^n, then 1/x -> 0) is what it should show, todo
- roots get a degree too, sqrt(x^2+x) grows like x and sqrt(x) like x^(1/2), so ∞/∞ with roots goes by the top terms like polynomials do (l'Hopital just flips those back and forth forever). sqrt(x^2+x) - x, where the top terms cancel, goes through the conjugate

## Integrals

`integral(f, x)` and `integral(f, x, a, b)`. worked out the way the d/dx steps are: every integral still in the line gets one rule, then the next line. the rules, first one that fits:

- constant rule (∫3 dx = 3x), sum rule, constant multiple (a number in front or a number on the bottom)
- a table: power rule (1/x is ln|x|), exp rule (e^x and 2^x), trig rule (sin, cos, tan, sec^2, sec tan, ...), inverse trig rule (1/(1+x^2), 1/sqrt(1-x^2)), log rule (ln x is x ln x - x). anything ax+b inside gets the /a, so cos(3x) is sin(3x)/3 in one go
- u-substitution: some piece g where f/g' is all g. 2x cos(x^2) has u = x^2. u is U, or W, V ... if the problem already uses U
- no integration by parts or partial fractions yet, x e^x and 1/(x^2-1) stay as integrals

a definite one only gets worked out when f is defined all the way from a to b, so 1/x^2 from -1 to 1 stays an integral instead of coming out -2 (an improper one like 1/sqrt(x) from 0 to 1 stays too, for now).

### Proposal

- the answer has no + C, like the nspire. the steps end on "+ C" and then F + C
- u = x^2 gets its own line between the integral and the one in u, then "back to x" at the end
- a definite one ends on F(b) - F(a) with the numbers put in as written, then what that comes to
- the constant multiple happens before the u-sub, so 2x cos(x^2) goes 2∫x cos(x^2) dx, then 2∫cos(u)/2 du. on paper you'd see du = 2x dx straight away. its right but its a step longer than it needs to be

---
Back to [design choices](index.md)
