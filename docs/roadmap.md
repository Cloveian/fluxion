# Feature roadmap

## foundation thingys

- [ ] Storage strategy. where persistent state lives, instead of leaning on
  the OS's own variables.
- [ ] Names. multi-letter vars and typeable functions are the same thing, one table of
  functions / `e` / `i` / stored stuff, letters turn into the longest name. stored = bold =
  not symbolic, always ([design](design-choices/input.md#names))

## on foundations thingies

- [ ] Persistent(ish) history. Store history in ram so it survives closing the app
- [ ] Variable saving + a page to view/delete.

## engine maths

- [ ] cot / sec / csc.
- [ ] Multi-var / systems solve (ACT Q14). one variable only today.
- [ ] Solving trig equations.
- [ ] Holes & asymptotes. zeros of the denominator.
- [ ] Better "normal" math handling. the everyday cases that don't come out clean yet.

## polish

- [ ] Lock icons. a padlock with the A / C / 2 in negative space, in their own spots so they show at once.
- [ ] Finish bold / italic. the draw layer does it already, italic's unused.
- [ ] Mode menu.
- Optimisation. ongoing, cancel/poll is in and the slow cases are logged.

## later

- [ ] Sets `{}`.
- [ ] Matrices `[]`.
- [ ] Expanding formula-based functions into formulas.
- [ ] Unit conversion.
- [ ] Wizards.
- [ ] Geometry stuff (visuals?). needs breaking into real formulas (area, distance, slope...).
- [ ] Graphs.
- [ ] Steps to solve. show the work, not just the answer. last, once the engine's settled.
