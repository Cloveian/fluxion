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
| 2 | | a bunch of `e` and `log` | only `ln` | the input only needed `ln` |

---
Back to [goals](goals.md) · [screens & operations](design-choices/ui.md)
