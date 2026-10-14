# Simplify cases

Real cases where the nspire's answer wasn't what i wanted. This is the spec for "better than the nspire" (and the test list later).

## Rules so far
- only rationalize if it actually helps (the bottom actually ends up rational, or it gets smaller)
- don't put in functions i didn't type. $\ln$ in -> $\ln$ out, and turn internal stuff back ($e^{x\ln 2} \to 2^x$)
- default is whatever's smallest on screen, other forms (factored, expanded, etc) one key away

## Cases

| # | Input | Nspire gave | Want | Why |
| - | ----- | ----------- | ---- | --- |
| 1 | | $\frac{-\sqrt{10\pi}}{20\pi}$ | $-\frac{1}{2\sqrt{10\pi}}$ | rationalized for no reason, π is still on the bottom |
| 2 | | a bunch of $e$ and $\log$ | only $\ln$ | the input only needed $\ln$ |
| 3 | `solve(5x²+6x=3,x)` | $x=\frac{-(2\sqrt6+3)}{5}$ or $x=\frac{2\sqrt6-3}{5}$ | $x=\frac{-3-2\sqrt6}{5}, \frac{-3+2\sqrt6}{5}$ (and $\frac{-3\pm2\sqrt6}{5}$ as a form) | one fraction each, written the same way round, one $x =$ and commas, not "or". **Done** 10-14 ($\pm$ not yet) |

## Real problems (ACT practice test, 2026 booklet, maths section)

45 questions, a CAS can actually help with 16 of them, the rest are word problems / geometry / stats. inputs are in the engine's syntax (`_` is subtract, `-` is negate, capital letters). "Engine today" is plain PineappleCAS before i changed anything.

| Q | Do | Input | ACT answer | Engine today | Status |
| - | -- | ----- | ---------- | ------------ | ------ |
| 13 | simplify | `(5/13)/(5/12)` | $\frac{12}{13}$ | `12/13` | ok |
| 15 | expand | `(Y+7)^3` | $y^3+21y^2+147y+343$ | same | ok |
| 17 | simplify | `(X^2_Y^2)_(6X^2+4XY_Y^2)` | $-5x^2-4xy$ | `-4XY_5X^2` | ok (term order is the renderer's job) |
| 18 | simplify | `sqrt(9)+sqrt(-16)` | $3+4i$ | `4i+3` | ok |
| 19 | substitute | `7+14(N_1)`, N = 25 | $343$ | `343` | ok |
| 24 | simplify | `40pi/180` | $\frac{2\pi}{9}$ | `(2pi)/9` | ok |
| 43 | simplify | `pi*9^2*2` | $162\pi$ | `162pi` | ok |
| 11 | substitute | `-4.9T^2+30T+55`, T = 3 | $100.9$ | `1009/10` | **engine done** 10-14: $100.9$, needs the form key |
| 45 | simplify | `0.75*1+0.25*2` | $1.25$ | `5/4` | **engine done** 10-14: $1.25$, needs the form key |
| 23 | substitute | `7e^(3T)+1`, T = 5 | $\approx 2\times10^7$ | `7e^15+1` | **engine done** 10-14: `2.28831E7`, needs the form key |
| 26 | solve for b, then | `sqrt(2(D_C)/A)` | $\sqrt{\frac{2(d-c)}{a}}$ | `sqrt(2D/A_2C/A)` | **engine done** 10-14: $\sqrt{\frac{2(d-c)}{a}}$ (common denominator, factored), needs the form key |
| 8 | simplify | `sqrt(35*108)` | $6\sqrt{105}$ | `sqrt(3780)` | **fixed** 10-13: $6\sqrt{105}$ |
| 5 | factor | `X^2_X_30` | $(x-6)(x+5)$ | `X(X_1)_30` | **fixed** 10-13: $(x+5)(x-6)$ |
| 37 | factor | `X^3+X^2_8X_12` | $(x-3)(x+2)^2$, zeros $-2, 3$ | `X^3+X^2+4(-2X_3)` | **fixed** 10-13: $(x+2)^2(x-3)$. GCD factoring had made it *worse* |
| 37 | simplify | `(X^3+X^2_8X_12)/(X_3)` | $x^2+4x+4$ | four separate fractions | **fixed** 10-14: $(x+2)^2$ (smallest; expanded is a form) |
| 1 | solve | `(100+60+80+30+X)/5=72` | $x=90$ | – | **fixed** 10-13: $x = 90$ |
| 9 | solve for X | `6Y=5X_1` | $\frac{6y+1}{5}$ | – | **fixed** 10-13: $x = \frac{6y+1}{5}$ |
| 14 | solve system | `2X_Y=7`, `-4X+2Y=2` | no solution | – | **solve** |
| 33 | solve | `2(3X+5)+2(X+3)=360` | $x=43$ | `8X+16` for the left side | **fixed** 10-13: $x = 43$ |

as of 10-14, 14 of the 19 rows work now. left: the 4 form ones and systems (Q14)

later on 10-14 the engine does the form ones too, they just need the form key. so only systems left (Q14)

when i first went through it:
- 7 of the 16 just worked already (the plain simplify / expand / substitute ones)
- 4 had the right answer but showed it the wrong way (decimal, common denominator), same thing as case 1
- 4 needed actual engine work, 3 of those were polynomial factoring
- solve came up 5 times and the engine didn't have it at all, so that's going up the list. most of them are linear in one variable which isnt that bad

---
Back to [goals](goals.md) · [screens & operations](design-choices/ui.md)
