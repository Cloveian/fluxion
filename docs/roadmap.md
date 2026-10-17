# Feature roadmap

## foundation thingys

- [x] Storage strategy.
- [x] Names. multi-letter vars and typeable functions.

## on foundations thingies

- [x] Persistent(ish) history. Store history in ram so it survives closing the app
- [x] Variable saving + a page to view/delete.

## engine meths

- [x] cot / sec / csc.
- [x] Multi-var solve, (pulling one var out)
- [x] Systems solving
- [x] Solving trig equations.
- [x] Holes & asymptotes. zeros of the denominator.
- [x] Better "normal" math handling. (a sweep 10-16, todo.tests has whats left)

## polish

- [x] Lock icons. (for mod keys)
- [x] Clear the screen. `[on][clear]` 
- [x] Modifier taps while meth is processing
- [x] 'Soft' key menus in math print.
- [x] Screen off. auto after sitting idle, and `[2nd][alpha][on]` does it by hand
- [x] Mode menu. degrees / radians, simplify mode... etc.
- [ ] Optimisation. (never gonna check this one off)
- [x] Expand combines like terms after every step instead of at the end. (x+1)^7 was 5 min at 100% and ^8 ran out of memory, 2^n raw terms before anything got collected

## later

- [ ] Display mode option for wrap/scroll (for long answers)
- [x] Inequalities. solve with < > ≤ ≥, sign chart. no trig ones yet
- [x] Slope, distance and midpoint commands
- [x] Parallel and perpendicular lines
- [ ] Transformations
- [ ] Geometry formulas (area, perimeter, volume, angles)
- [x] Probability (nCr, nPr, and pand / por for independent events)
- [x] GCD, LCM and prime factorisation
- [ ] Percent
- [ ] Defining and calling functions like f(x)
- [x] Domain and range. range(f,x) does pieces between holes and asymptotes, no trig yet
- [x] Factoring with more than one variable (x²−y²)
- [x] Nonlinear systems. two equations, two letters, by substitution
- [x] Cubics with no rational root
- [x] Complex roots in solve
- [x] Slant asymptotes
- [ ] Unit circle display
- [x] Triangle solver
- [x] Amplitude
- [x] Roots to a polynomial and back (roots, fromroots)
- [x] Inverse functions
- [x] Ellipses and hyperbolas, equation to points and back. lined up with the axes only
- [x] Piecewise functions. a command for now, no brace template
- [x] Ghost parens, the fraction grabbing the last term, ans when a line starts with an operator
- [ ] Show-the-work steps

## more-later

- [ ] Sets `{}`.
- [ ] Matrices `[]`.
- [ ] Expanding formula-based functions into formulas.
- [ ] Unit conversion.
- [ ] Wizards.
- [ ] Graphs.
- [ ] Steps to solve. show the work..
