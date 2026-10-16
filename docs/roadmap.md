# Feature roadmap

## foundation thingys

- [x] Storage strategy. where persistent state lives, instead of leaning on
  the OS's own variables. going with RAM appvars, a RAM reset clearing them is fine. size gets capped,
  history drops the oldest off the end when its full
- [x] Names. multi-letter vars and typeable functions are the same thing, one table of
  functions / `e` / `i` / stored stuff, letters turn into the longest name. stored = bold =
  not symbolic, always ([design](design-choices/input.md#names))

## on foundations thingies

- [x] Persistent(ish) history. Store history in ram so it survives closing the app
- [x] Variable saving + a page to view/delete.

## engine maths

- [ ] cot / sec / csc. real functions, not lazy: `cot` in means `cot` out, never 1/tan
- [ ] Multi-var solve, pulling one var out, like `solve(y=7x+3,x)` -> $x=\frac{y-3}{7}$. linear already works, anything else gives up (`y=x^2+3`, `a=pi r^2`, `y=2^x`)
- [ ] Systems (ACT Q14). its own function like the nspire's, commas, no `and`
- [ ] Solving trig equations. general solution or an interval, that's a mode setting
- [ ] Holes & asymptotes. zeros of the denominator. a function, answer on a few lines so it's readable.
- [ ] Better "normal" math handling. the everyday cases that don't come out clean yet.

## polish

- [ ] Lock icons. a padlock with the A / C / 2 in negative space, in their own spots so they show at once.
- [ ] Finish bold / italic. the draw layer does it already, italic's unused.
- [x] Clear the screen. `[on][clear]` on an empty line scrolls the history up off the screen, nothing gets deleted
- [x] Modifier taps while its working. they get dropped right now, 2nd / alpha / on pressed during `working...` should stick for the next key
- [x] Soft key menus in math print. the folder lists say `tan^-1`, they should draw the real thing like the line does
- [x] Screen off. auto after sitting idle, and `[2nd][alpha][on]` does it by hand
- [ ] Mode menu. degrees / radians, simplify mode, the trig answer thing. simplify mode picks
  what "simplest" means since its subjective (expanded or factored? roots in the bottom?).
  presets plus a few toggles, "by hand" is the default preset and is really just a toggle
  combo. the form key switches it, the rationalize toggle in Tools moves here
- Optimisation. ongoing, cancel/poll is in and the slow cases are logged.
- [x] Expand combines like terms after every step instead of at the end. (x+1)^7 was 5 min at 100% and ^8 ran out of memory, 2^n raw terms before anything got collected

## later

- [ ] Sets `{}`.
- [ ] Matrices `[]`.
- [ ] Expanding formula-based functions into formulas.
- [ ] Unit conversion.
- [ ] Wizards.
- [ ] Geometry stuff (visuals?). needs breaking into real formulas (area, distance, slope...).
- [ ] Graphs.
- [ ] Steps to solve. show the work, not just the answer. last, once the engine's settled.
