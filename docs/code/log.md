# Log

## 2026-11-18 (evening) - a segfault and some roots that could be smaller

- csolve 3x^4+4=0 (and 5x^4+4) segfaulted, older than anything from today, a crash scan found it. the gcd went gcd(A, -A) -> gcd(-A, A) -> ... until the stack ran out. it stops 32 deep now (real inputs never go past 7), and that alone turned the crash into 1.8M work, ~1.8 *hours* on the calc. so a two term ax^4+b or ax^6+b skips the quartic stuff under csolve and goes straight to nth_roots: 18k, about a minute
- my first version of that just skipped them and the checker caught (x-1)(x^4+2) coming out unknown, cuz the factor got skipped but nothing solved it either. solve_binomial does them in solve_factor now, with a test
- 4^(1/4) is sqrt(2), 1000^(1/6) is sqrt(10), 64^(1/4) is 2^(3/2). eval_pow tries n as a d-th power for each d *dih*viding k, biggest first. first go was inside eval_root_factors and made 8^(2/9) into 4 (its p/q caller wants s\*r^(1/q) back with *its* q), so it's its own helper. sqrt(-3+4i) comes out 1+2i on the way
- 180 random ax^n+b against sympy, 0 wrong 0 unknown
- (later) the p/q path gets it too, after eval_root_factors gives up: 8^(2/9) -> 2^(2/3), 64^(5/12) -> 2^(5/2). one system answer got shorter from it, 600 systems + 4000 rationals otherwise the same
- ln(-2+2i) is ln(8)/2+3ipi/4. sqrt(8) is 2^(3/2) by the time the atan gets built, and gcd(2^(3/2), -2) was 1, upstream only took a gcd off a power against the same base with a plain number power. so 2/(-2+2^(3/2)) never cancelled to the 1/(-1+sqrt(2)) the table has. any power >= 1 gives up its whole base now, nothing else in the harnesses moved and 1800 fuzz sums didn't hang

## 2026-11-18 (afternoon) - slow ones and a sort that wouldn't stop

- okie so csolve 2x^6+3=0 was guessed at ~12 *minutes* on the calc (157k work). the minus on -3/2 never got seen, simplify hands it back as a/b and nth_roots only checked for a number node, so every root started as (-3/2)^(1/6)(cos+isin) and the identities turned it complex six separate times. folding it first + building cos t - isin t for the negative angles: 31k, ~106 s. still slow-ish but its just the simplifier merging 3^(1/6)/2^(1/6) into each root now, ~4.5k a go
- x^6=64 under csolve was 28k work for no reason: factor stopped at (x-2)(x+2)(x^4+4x^2+16), so solve_even took four complex square roots. poly_factor now takes roots in x^2 out of an even one (x^4-5x^2+6 -> (x^2-2)(x^2-3)) and tries an even quartic as (Px^2+Rx+S)(Px^2-Rx+S), which is the only other way one splits. 1.2k now, and factor(x^4+4), factor(x^8-1) n' stuff finally factor all the way. 5 of the 600 systems got denested on the way, sqrt(4+2sqrt(3)) -> 1+sqrt(3), same roots
- abs(-3i) is 3. upstream flipped the first minus it found in a product and dropped the abs, so abs(-3x) was 3x too, which is just wrong for x < 0. numbers come out as their size, i as 1, pi/e/sqrt(2) as they are, letters keep their abs. pcas.tests had abs(Ji) = J baked in, it's abs(J) now
- then the fun one. trying to get -2/sqrt(2) to say -sqrt(2), csolve x^4=-16's roots turned into -sqrt(2)-isqrt(2) and the canonical form (what the app runs to *show* an answer) hung on it. on HEAD too, it just never got that input before. the gcd saw sqrt(2) and 2^(1/2) as *dih*ferent things depending on which side they were on, so compare() said both terms went first and the sort swapped them forever. fixed that, wrote a little fuzz of random radical + i sums, it found 5 more: two more pairs, three terms going round in a circle (-1, -isqrt(44), -ypi/2, rock paper scissors), and compare dividing by a gcd that made things *bigger* until it was 400 terms deep. so the sort only moves a term when compare agrees both ways, the canonical form gives up after 32 passes (pcas.tests needs 15) and compare's gcd bit stops 4 deep (tests never go past 1). 700 more fuzz sums on the fixed build, 0 hangs. none of the 366 test inputs display any different
- the -2/sqrt(2) thing itself didn't go in. a rule in eval_div did it nicely but changed what like terms see mid-solve, 2^(3/2) next to 2sqrt(2), and a root that makes a denominator 0 stopped getting caught. not worth it for looks, reverted
- ln(-1+i) is ln(2)/2+3ipi/4, the atan table has pi/8 and 3pi/8 now
- (later) and it went in at the rationalize step like i guesed, the canonical form cancels the number on top against the one below right after it rationalizes. -2/sqrt(2) shows -sqrt(2), 6/sqrt(3) shows 2sqrt(3), x^4=-16 shows +-sqrt(2)+-isqrt(2). display only this time, so the solver never sees it
- still open: ln(-2+2i) doesn't fold, 2/(2sqrt(2)-2) isnt cancelled before the table. factor((x^2-1)^2) prints ((x-1)(x+1))^2. the three-in-a-circle sums still cost ~250 s on the calc before the cap stops them

## 2026-11-18 (morning) - wrong answers, round whatever

- csolve on a cubic with one real root handed back that one root like it was all of them, x^3+x+1=0 was 1 of 3. unknown now, same as any other partial, and solve_factor finally passes complex into solve_cubic, so (x^2-2)^3 written out gets its +-sqrt(2) back. last night's go at that pushed the solve_factor frame past ix-128, this one *shrank* the app
- x(y^3-3y-1)=1 with y^3=3y+1 didn't need the triple angle identity after all. when one equation is a cubic or more in one letter alone, the other one gets *dih*vided down by it first (poly_div carries the other letters through now), so that one turns into -1=0 and it's false. 120 made up systems like it against sympy: 81 right and 5 wrong -> 107 right and 0 wrong, the other 13 have cardano answers too long for the test runner to print
- sqrt(-2+2isqrt(3)) came out sqrt(3)-i, which squares to 2-2isqrt(3), so not even a root of the right thing. upstream's ln(a+bi) used atan(b/a), off by pi whenever a < 0, and (-A)^(X/Y) pulled the -1 out of a complex A too. it's the half angle 2atan(b/(|z|+a)) now unless a is a plain positive number. x^6=64 had 2 wrong roots of its 6 and ln(-1+i) had the wrong angle. the checker found ln(-3i) still broken cuz upstream abs(-3i) is 3i (￢_￢ ) so it's sqrt(a^2+b^2) in there instead, abs itself is still todo
- csolve x^n = c gives all n now (2x^6+3=0 was 2 of 6), r(cos t + isin t) going round 2pi/n. x^5=1 and even plain solve x^5=32 were unknown: factoring turned x^5-32 into (x-2) times a quartic and isolate never got to see it whole. residuals ~1e-15 against sympy
- still open: ln(-1+i) is right but prints 2atan(1/(-1+2^(1/2)))i, no 3pi/8 in the table. abs(-3i). x^4=-16 says -2/2^(1/2) where -2^(1/2) would do. and the slow ones, same as last night

## 2026-11-18 (night) - most of last list

- the lcm_divide segfault: factoring a bottom hands back 20000^2, which eval leaves alone, and comdenom's lcm turned its plain 20000 into that power, then the next bottom's 20000 found nothing. term_den folds those now and merges the numbers into one. my first go just folded and 1/(2\*20000^2)+1/x quietly came out 1/x, which is worse than a crash honestly. the checker's fuzz set went 14 segfaults -> 0. that system says unknown now, but it has four real pairs, its quartic is (y-100)(y+100)(990099y^2-9901009901). a quadratic in y^2 would do it (todo)
- systems handed out sqrt(-7) as a real answer. 65 of the 600 made up systems (11%!!) had fake pairs, every one of them agrees with sympy on how many real ones there are now. the exponent comes as a 1/2 *tree* half the time, not a number, which is why the first version caught nothing lol
- system answers get big powers worked out (1000^3, 998^2), that also gave back the real pair one system was losing (its like terms couldn't meet). capped at 64 bits cuz uncapped 2^1000000 ran for a minute and 7^150/11^250 turned into 127 digits over 261 on a 320 px screen. and a failed power in eval put half a number in, upstream, fixed
- near tangents get an exact sign when the numbers fit, x^2+y^2=1 with x=1+1/10^7 is false now. past ~10^19 it's the float check again, known corner
- widths come off the bands now, SCREEN_W only lives in areas.c, and the top bar's edges are in one place. 11 screens and the wall byte for byte the same
- still open: x(y^3-3y-1)=1 with y^3=3y+1 keeps three made up pairs (needs the triple angle identity to see the bottom is 0). and products of fractions like (3+2y^2+10x^2)/(100+3y^2+7y)\*(5y+10x^2)/(1/2+10-1) sit in upstream's pairwise gcd factoring for over a minute, main too. the timer and [on]+[mode] cover it for now
- a question for future me: enter leaves 1000^2 and 2^100 alone (upstream only works out a^b with a, b <= 10), eval( does them. the nspire would just say 1000000. probably should do the same with the 64 bit cap, but that's a call to make awake
- (later) that quadratic in y^2 happened: solve does x^4 and x^6 with only even powers as a quadratic / cubic in x^2, the system above gives its four pairs, and 86 of the 600 systems went from unknown to an answer (72 against sympy, 14 by residual). which u are positive goes by descartes and the order the roots come back in, decimals only for a cubic with a double root. the decimal said 3x^4+200000x^2+3 had real roots, then two u tied as floats and a negative one got kept, *bleh*. x^6 solves with real roots are still ~20 s to 2 min on the calc by the budget's guess, nearly all of it simplifying the sqrt of a cos form or a cardano root

## 2026-11-17 (evening) - the branch comes home

- the branch is in. the fix was making the sort not need the gcd at all for sums: every term splits into its letters (powers counted) and whatever else is in it (sin(x), pi, x+1), and the letters decide first, most of them first, then more of the earlier letter. the old gcd-and-divide compare only gets the ties now (same letters) and pairs with no letters on either side. 0 hangs in 5000 random sums where main hung on *13* of them, plain ones like xy^2z^3+y^2z^2/2-xy-3xz/4-1, so main was never safe either. expand((x+y+z+1)^4) in the app is 139k -> 4.3k work and ^5 is 10.8k (main never came back). multivariable sums print in a fixed order now, x^2+2xy+y^2+2x+2y+1 instead of 2xy+y^2+x^2+2x+2y+1. graded order, not sure if the nspire does plain lex, (todo) check on a real one
- first try of that broke things that weren't monomials: 4x^2y^2pi next to x^2y^2 still went the slow way and *that* hung 41 in 1500 (worse than main lol). then sin(x) vs pi recursed forever and segfaulted. then 1-ln(x) came out -ln(x)+1. three rounds, all fixed, 1-ln(x) has no test yet tho
- systems check their pairs by decimal first, y=x^3 with x^2=y+3 was ~2.8 *hours* on the calc and is ~47 min now (still awful, the back substitution is the rest). getting it right took four rounds of the checker finding made up pairs: big values blow up the float tolerance, float noise in a cancelling y, big constants on both sides, and isolate making roots up after a sqrt got substituted in. it ended at: the decimal only ever says yes, and only when everything solved on the way was a polynomial, where the roots are exact anyway
- fraction solves check their roots the light way first, 1/x+1/(x+1)=1 ~25 s -> ~17 s. a power of a negative still gets the full check, (-8)^(1/3) is only -2 in SIMP_ALL
- trig families skip the trig / hyperbolic tables their asin already went through (complex stays, an i peeled off after makes 1/i)
- 5*2^X printed as 52^X, upstream thought a power of 2 was a sqrt
- found and not fixed yet: some systems segfault in lcm_divide (x^2=y^2-9999, xy+10000y^2=100000100), roots of negatives get kept as real pairs, one system loses one of its four real pairs, x(y^3-3y-1)=1 with y^3=3y+1 keeps three pairs when there are none, and 2sqrt(2)1000^3 doesn't get folded in a system answer

## 2026-11-17 (afternoon) - themes, and a clock for the slow ones

- past 5 s the top bar says how long its been going and that [on]+[mode] stops it, counting up. no guess at the total, i looked and theres nothing to guess from: sin(x)=1/2 is 6 nodes and ~7 s on the calc, x^2+y^2=25 with x^2+y^2/4=10 is ~36 s, and the input size says nothing about either. so ~~"this will take ~N s"~~ a counter, unless a real predictor shows up someday
- the scripted PC app takes `wait:6`, `png:busy.png` and `busy:on+mode` now, so a screenshot mid-run and a cancel can be tested without a window
- themes! dark, light, contrast, green, rose, paper. each one is just bg, text and accent, dim is mixed halfway bg -> text and select a quarter bg -> accent, so a new one is three colours to get right instead of five (text on bg 12+, dim on bg 3.1+, text on select 6.6+, the WCAG ratios). the palette was also written out twice before (screen_ti.c and screen_pc.c, each one "same colours as" the other), one table in ui/theme.c now. dark and light moved a tiny bit cuz their dim / select are mixed now too, dark dim 120,120,140 -> 129,129,136, so the wall got rerendered. the old light byte in the save still means dark / light
- trig solve *dih*ging. approx_number ran a full SIMP_SOLVE on stuff that was already all numbers, like terms and all, which goes through expand. SIMP_EVAL instead: ran both side by side on all 295 calls in fluxion.tests, same answer every time, 499k -> 491k work total
- then at_n (plugging n into a family for the one turn answers) didn't need its simplify at all, approx_number folds it and the keepers get the full one anyway. 300 one-turn solves (sin / cos / tan, 10 arguments, 10 right sides, 60 of them didnt parse) against main: 235 of 240 root lists the same and the other 5 *better*, sin(2x-1)=1/2 had (-pi)/12+1/2+pi/2 where it now says 1/2+(5pi)/12. with the SIMP_EVAL one, one turn sin(x)=1/2 went 6514 -> 5335 work (~7.5 -> ~5.3 s on the calc, by the budget's guess). two of the 300 went in as tests

## 2026-11-17 - the branch hangs, so not that one yet

the style checker finaly went over all 18 of last night's commits (it got blocked then). mostly comment trims and log numbers that said more than the runs did, all folded back into their own commits

- perf/grouped-monomials makes simplify hang. (XYZ)^2+6X^2Z^2+X^3 never comes back on the branch (8 s and still going), main does it instantly, and random sums of 2-5 monomials hang 2-3 in 300 (main 0 in 800). same kind of thing as the cardano one: the fast gcd and divide hand compare() *different* trees than the slow path, so the sort isn't one consistent order anymore and just goes round. one of those is fixed on the branch (a bare (yz)^3 over z kept only the y), four aren't. and the "order flip" is really a tie kept in typed order, 55 of 300 sums print in another order than main. so it stays on its branch until gm_gcd / gm_div give exactly the trees the slow path does, or compare() gets one order for monomials. future me problem
- with alpha locked, 1-9 in a menu turned into y z θ t u v o p q and picked nothing. locked alpha leaves them as numbers now, a tapped one still gets you O P Q in Functions
- long folders (Algebra, Functions) drew over the status bar, padlock corner included. they count their rows from under the bar now, one row less and they scroll

## 2026-11-16 - cleanup night, folders in folders

okie so a month off, then a look around before adding anything: what a 'real' cas has that this doesn't (polynomial gcd, real factoring, a normal form, assumptions. gcd(x^4-1, x^6-1) says 1 lol), what nspire support would take (a cheap port, the TI side of platform/ is ~370 lines and ndless r2020 does the CX II on 6.2.0), and where to stop for the CE (time and heap, not flash, PC ms \* 6500 is still the rule). all of that wants the code a bit less copy pasted first

- the agent written command files each had their own comma cutter, six copies of the same 20 lines (polyfn's even said "same as geom's"). one args.c now, args_split / args_parse (+ a degrees one) / args_read / args_var. 272 lines out and 135 in (args.c included), the third program appvar went 12324 -> 11389 bytes. checked by typing 47 commands into the old and the new app and comparing the screenshots, byte for byte the same
- point_box was in geom and conic, lives next to solutions_box in app.c now
- the screen is three bands (areas.c): top bar, body, soft keys. app, page and the soft-key bar each worked out SCREEN_H - BAR_H and top_h() on their own before. 29 ui screens came out byte for byte the same. so a screen without the bar (the nspire, someday) is one number, and any resolution later is mostly SCREEN_W / SCREEN_H turning into variables (and fonts, *bleh*)
- folders can have folders. Algebra was 32 things, which is *exactly* menu_boxes[32], so that was coming either way. it's the everyday nine now, then Function info / Lines / Shapes / Solids / Conics / Probability (a first guess, not settled), and rows past 9 are A, B, C like the nspire, alpha + the letter. the menu takes its keys after the modifiers now so a tapped alpha works in there (the padlock shows up too)
- wrote the nspire cx ii cas menu tree down off a pile of photos ([nspire-cx-ii-menus.md](../design-choices/nspire-cx-ii-menus.md)). 252 things in it, the cas part (number / algebra / calculus) is maybe 90, stats and finance the OS already does
- values_box (polyfn) was solutions_box without the x =, and factor.c and solve.c each had their own little integer gcd. one of each
- step one of the cost estimate: the engine counts pcas_check calls (pcas_work), and `fluxion budget <tests>` prints work, PC time and the CE guess per line. over the 364 lines with 300+ work (the other 62 are under 80 us) it's 0.13 us per unit median, p10 0.056, p90 0.24, so a guess off the work count is about 2x either way for most lines (p10 to p90 is ~4x). the slowest per unit are roots of numbers, sqrt(-8) does ~1040 simplify calls (most out of expand) and ~4000 evals, each one rational maths. that's a speed thing to look at on its own. the systems one at the top, x^2+y^2=25 with x^2+y^2/4=10, is 5.5 ms here so ~36 s on the calc, ugh
- `FLX_WORK=1 ./build-pc/fluxion-app` prints every answer's work and ms. expand((x+y+z+1)^4) in the app is 210k work and 125 ms, ~13 min on the calc (the "11 ms" from october was some other path). first theory, the flat expand gets skipped... nope, all the expand paths are ~6k work and 3 ms. it's the canonical sort after, 6k -> 210k
- the monomial gcd from october ended in a simplify(SIMP_ALL ^ SIMP_ID_ALL), which with like terms on runs expand and factor on stuff like 2XY^2. ran both side by side on every gcd in all three test files (4001 of them) and all it ever did was drop a 1 and unwrap a lone factor, so it does just that now. 210k -> 139k work... and the time didn't move lol
- the real one: the canonical form writes x^2z^2 as (xz)^2, the fast gcd only reads letter^n, so every compare with a (xz)^2 in it took the slow recursive gcd. reading (xz)^2 as x^2z^2 makes that sort 139k -> 4.3k work, ~105 -> 9 ms in the app (~1 min on the calc instead of ~11), and (x+y+z+w+1)^4 1.64M -> 10k. BUT out of 27 inputs one comes out in a *dih*ferent order: 6x^2z^2+4xz^3 used to print 4xz^3+6(xz)^2. tried keeping the old division so only the gcd changes, same flip and slow again. so it's on its own branch, perf/grouped-monomials, until i decide if i care about that order (i probably don't but it's a *change*)
- the web demos picked Algebra things by number (y= 7 was system, now it's inverse), all 15 fixed and checked against the old app screen for screen, and the wall got rerendered (just the Algebra and Functions tiles changed)
- the branch doesn't touch the other slow ones. solve(1/x+1/(x+1)=1) is ~3900 simplify calls, comdenom 36% (term_den factoring every bottom is ~15 of that, the rest spread thin), read_poly 27% (its own SIMP_SOLVEs ~16, read_term ~12). adding whole number coefficient sums up directly was the same tree on every test and saved... 1% lol, so not that. the simplify on lead or the fraction ones maybe, next time
- and the test runner's failure printout that crawled on big trees was ~~the printer~~ that same sort all along, the output was just block buffered so it looked stuck halfway through printing. a backtrace off an alarm said simplify_canonical_form -> compare -> gcd. (x+y+z+1)^5 is 56 terms: over 60 s on main on the PC (days on the calc), 25 ms on the branch. in the app it's over 120 s vs 27 ms, and the 76 screenshots from up top (47 commands + 29 screens) are the same on both. so the branch is the big one and that order flip is probably worth it

## 2026-10-16 (evening) - the big list

got a list of 16 things with a deadline, so five workers on their own branches, one thing each, and me on the engine bugs and merging. all in by 19:10, 407 tests

- editor: `(` doesn't force a `)` anymore, a grey one sits where it would close and export closes it there. the fraction key pulls the term before the cursor up top (5+9 then n/d is 5+9/▯). a line that starts with + - * / ^ gets ans in front. and the · next to a fraction has room now
- `[alpha][enter]` is a decimal for that one answer, the next one is back to normal
- by hand comes out as one fraction, bottom factored, top multiplied out: 6x/(x+1)-5/(x+1) is (6x-5)/(x+1). roots stay a sum for now, comdenom rationalizes them into a mess
- new commands, roots / fromroots, inverse, triangle(a,b,c,ab,bc,ca) (empty for unknowns, both triangles in the ssa case, decimals), amplitude, pand / por, ellipse and hyperbola both ways, piecewise(f1,c1,f2,c2,...,x=v). the editor's name buffer went 8 -> 12 so triangle, amplitude, hyperbola and fromroots fit. system() does two nonlinear equations by substitution, every pair checked back in both
- engine bugs. (7+4i)^2 went polar, 65cos(2atan(4/7))+... and the app ran out of memory drawing it, whole powers of a complex sum multiply out first now. (-8)^(1/3) was 1+isqrt(3), odd roots of a negative stay real like the nspire's real mode (that's what made inverse(x^3+1) complex). and (1+4/9)/2+1/9 was 119/18!! upstream bug from day one, eval read only the tops of a fraction of fractions. a worker bisected it to the import
- the sort's divides got the same direct path as the gcd, (x+y+z+1)^4 is 11 ms now, was 98 this afternoon
- solve(3e^x=12,x) running out of memory i cant reproduce, ln(4) on the PC and 24 KB of heap. probably an old build on the calc, waiting to hear
- the app grew a lot, the program's second appvar went 44 -> 63 KB and the installer is 6 appvars now (345 KB, most of the difference to the program is the relocation table flash apps carry)

later, back on the roadmap

- inequalities. solve(x^2-4<0,x) is -2 < x < 2, by a sign chart: roots of the top and bottom sorted by their decimals, one test point per gap, the ends in or out by the sign and whether it's a hole. no trig ones, the families would need an n in the chart
- range(f,x). no limits in the engine so domain() and asym() cut x into pieces, and each piece spans its turning points (f' = 0) and what its ends do. heading off to infinity is a numeric poke, steps that don't fade = it blows up (ln grows slow but steady, 1/x 10x a poke). 1/x is y < 0 or y > 0, x e^-x is y <= 1/e. trig says no
- circle( rect( square( trap( sphere( cyl( cone( cube( box( polygon(, each one shows every formula for that shape, letters fine. the catalog's All list was already chopping off its last ~10 items at 160 rows, now 224
- solve with x under the fraction bar, (3x-y)/(x+y) = 5/8 is x = 13y/19 (an ACT question). over one denominator, top solved, roots that zero the bottom dropped. found a NULL deref in comdenom on the way when it all cancels. and solve(1/x^2=0) said 1/0, e^x = 0 said ln(0), isolate undoes powers without asking. those get dropped now
- the program (not the app) is 3 appvars now

## 2026-10-16 - it remembers, cot sec csc, mode menu, solve grew up

- it remembers now. the history and the stored names go into RAM appvars on the way out (FLXHIST, FLXNAMES, files under build-pc/store on the PC) and come back at start. rows go to bytes with a tiny format in editor.c, a format byte up front so an old one gets ignored instead of read wrong. history capped at 12 KB, oldest dropped, names 4 KB. a RAM reset loses them and thats the deal
- bold measures as wide as it draws now (one more per glyph, the layout callback takes the style), so stored names stopped having their letters touch. italic hangs its top out by the same amount it leans, still nothing uses it
- cot, sec and csc are real functions now, parser to printer, with derivatives, decimals and a few exact values (cot(pi/4) is 1). `[on]` + sin / cos / tan types them. nothing turns them into 1/tan, which was the whole point
- the mode menu, on `[mode]`: angle (degrees is a pi/180 on every trig argument before it runs, exact, so sin(30) is 1/2 and asin(1/2) is 30), what simplest means (the form, same thing as 2nd+enter), and rationalize, which moved out of Tools. the three of them go in their own little appvar (FLXMODE, 4 bytes lol) after every flip. and sin(x) shows as sin(x) again, the pi/180 gets pulled back out of any trig argument that stayed symbolic after simplify (sum terms one by one, a real pi in there and it stays). the trig answer form waits for trig solving to exist
- solve pulls a letter out of a quadratic now, solve(y=x^2+3, x) gives -sqrt(y-3), sqrt(y-3), and x^2+bx+c=0 gives the formula. built as a tree and simplified, with the bits simplify wont do done by hand first (a square factor out of the root, a common number out of the fraction, flipping a negative lead). y=2^x still gives up, that needs an isolate solver
- and when the variable only turns up once it just peels the layers off: solve(y=2^x, x) is logb(y,2), sqrt(x)=2 is 4, 1/x=2 is 1/2, x^4=16 is -2, 2. five of the old "cant do this" tests became real answers
- log(1000) is 3 now, and logb(8,2). integer logs that work out get evaluated, the others stay
- asym(f,x), holes and asymptotes. one line each (hole x = 1 / vertical x = -2, 2 / horizontal y = 2), only the kinds it has, none if nothing. the answer is a new stacked box kind (BOX_COL) so it cant be browsed into or saved, it just goes with the session. top and bottom come straight off the tree as typed, simplify would cancel the hole out. multiplicity by differentiating until it stops being zero there. no slant asymptotes yet
- system(eq, eq, ..., x, y), linear systems. gauss-jordan over the engine trees, coefficients read off with d/dx (the coefficient of x in a linear thing is just the derivative, and the constant is what you get with every letter set to 0, then one check that the leftovers are 0). letters in the coefficients work, x = b/(a+1). no letters named and it takes the ones it finds. false when there is none, a message when they overlap. nonlinear ones say so, substitution is a todo. the last scoreboard row, 19 of 19
- solve does trig equations now. solve(sin(x)=1/2,x) is pi/6 + 2pi n, 5pi/6 + 2pi n like the nspire (cos gets the +-, tan every pi, sin(x)=0 is just pi n), n being the plain letter n. a "trig answers" row on the mode page switches to one turn, 0 up to 2pi (360 in degrees): the general ones get small integers plugged in for n and whatever lands in range stays, sorted. that works for sin(2x) and shifted arguments for free. in degrees it comes out as 360n + 30, or 30, 150
- a sweep of everyday maths. solve takes fraction coefficients now (x^2=1/4, x^2/2-x/2-15=0: the 1/2 was an a/b tree so it didnt count as a number, and factor couldnt read it, so the bottoms get cleared first), roots sort by their decimal (1/2 after -1/2, sqrt(2) too), -x=3 is -3 not 3/-1. by hand form multiplies out and keeps it only if it got smaller, so (x+1)^2-(x-1)^2 is 4x and (x+2)(x-2) is x^2-4 but (x+1)^2 stays. sec(x)cos(x) is 1 (and csc sin, cot tan). 27^(2/3) is 9 and sqrt(12)+sqrt(27) is 5sqrt(3), those two took some digging in eval and the expression gcd. the todo.tests solve block moved over to the real tests
- degree mode odds and ends. asin(x) shows as asin(x) again instead of 180asin(x)/pi, and acos(x)+3 kept losing its 3: degrees_in wrapped the acos while it was still linked into the sum, so the chain ran off through the wrapper. detaching first fixed it. FLX_DUMP=1 on the PC app prints the tree right before it gets drawn, that is how i found it
- timing the new stuff on the PC (times 6500 is the calculator): asym and system are nothing, but trig solve was 2 ms and the one turn version 5 ms, so 13 and 33 seconds on the real thing. the one turn now works out the period and only plugs in the n that can land in range, light simplify for the tries and the full one for the keepers. about 2.5 ms now, the rest is the full simplify on each family in solve itself, which the light one cant replace (it leaves pi - pi/6 alone)
- timed every test line (a worker did the grind). expanding powers of sums was the whole top of the table, (x+1)^8 at 38 ms and (a+b)^3(b+1)^3 at 133 ms, minutes on the calculator. so expand now does the plain polynomial bits flat first, rows of coefficients with an exponent per letter, multiplied out and built back into a tree once. the expand itself is nothing now, but then it turned out the simplify after it is the real cost, 30 ms for a 9 term sum and 118 for 16, the like terms pass runs a whole gcd for every pair of terms. so (x+1)^8 went from 38 to 16 ms and not to zero yet. a cheap same-letters check before each gcd should fix that, next time. the tree expand is still there for anything with a sin or a sqrt in it. and the by hand form stops trying to multiply out a fourth power or worse, nobody does that by hand
- four features at once, each a worker on its own branch from a spec, and me merging: complex roots in solve (x^2+2x+5=0 is -1-2i, -1+2i, solve_ex with a flag so asym and the rest stay real), slant asymptotes (asym((x^2+1)/x) says slant y = x, polynomial division on the flat rows, none when it divides exactly), and slope, dist, mid, par and perp as commands in ui/geom.c. gcd and friends still cooking
- and three UI ones the same way: [2nd] up/down is brightness (8 steps, starts where the OS was, saved, OS level put back on exit) with a dark / light theme row on the mode page, the catalog is in categories now (Algebra, Calculus, Functions, Templates, Keys, then All in alphabetical order, with header rows the cursor skips), and the modifier words in the top bar became padlocks with the 2 / A / C cut out, open for sticky, closed for locked, each in its own slot so they dont shuffle
- gcd(, lcm(, nCr(, nPr( are real functions, and factor(360) is 2^3*3^2*5. the first version taught simplify to leave such a product alone so the test passed, which made a typed 2^2*3 stop being 12, sent it back, the test runner compares text for that case now and the app skips the simplify after factoring a number. nCr and nPr are menu only for now, typed letters get lowercased on the way in
- round three, same way: cubics with no rational root (the cos form for three real roots, cardano for one, x^3-3x+1 is 2cos(8pi/9), 2cos(4pi/9), 2cos(2pi/9); the complex pair is gated off for now, drawing it is minutes on the calculator), factoring with two letters by pattern (x^2-y^2, (x+y)^2, cubes, a shared number out front), domain(f,x) (bounds from roots and logs, holes from denominators, tan families, empty when they cross), and two speedups that fell out of timing the cubic: eval_root_factors stops trial division once p^k passes what is left (7x on a cubic solve), and a gcd guard for the hang my own pow_value change caused on a fraction inside a cube root. deep sleep via boot_TurnOff is on a branch, untested, the docs for it are nowhere
- the expand thing from earlier, finally. the same-letters check went in and only got a quarter off, so i timed each stage and it was never the like terms pass: 66 of the 71 ms in (x+y+z+1)^4 (a -O0 copy with clock() between the steps) was the canonical form sorting the terms. compare() does a gcd per pair and the general gcd recurses through every factor with a simplify at each level. now two monomials (a whole number times letters to whole powers) get their gcd worked out directly. a scratch copy ran both versions side by side over every test and found one difference, gcd(-x,-1) keeps its sign upstream, matched that. in the normal PC build (x+y+z+1)^4 went 98 -> 29 ms and (x+y+1)^5 32 -> 12. still ~3 min on the calculator for the big one, the sort's divides are the next 20 ms
- domain(sqrt(x-2)+sqrt(2-x),x) says x = 2 instead of x >= 2 and x <= 2 on two lines

## 2026-10-15 - out of memory, bold / italic, E

- running out of memory doesn't crash anymore. every malloc in the engine goes through `pcas_malloc`, a NULL sets `pcas_oom` and the cancel flag, so it all unwinds like a cancel and the app says "out of memory". imath hands out a static number instead of NULL: the cas code makes scratch numbers and feeds them to imath straight away, so that was ~400 crash sites in one go
- found the rest with fault injection, `PCAS_FAIL_ALLOC=n` fails the nth allocation on the PC. first sweep was 12600 runs: 22 crash sites, 420 imath asserts, and one heap overflow that was my own bug (`read_num` bailed before setting `*consumed`, oof). guarded the sites, then made every recursive function return early once the flag's set, which cas.h always claimed they did. had to, some inputs allocate 16000+ times and guarding derefs one at a time was never going to end
- +834 bytes on the calculator. haven't timed the extra checks on it yet
- `[2nd][,]` types E, `2E3` -> 2000
- the form key: `[2nd][enter]` cycles by hand -> expanded -> factored -> decimal, `[alpha][enter]` jumps to decimal and back. it's a mode, it redoes the last answer (runs its input again from the scratch editor, so a half typed line is left alone) and shows in the top bar. decimal is `approx()` plus the layout printing numbers as 0.333333. that's the last four form rows on the scoreboard, 18 of 19 now
- expand collects like terms after every step now. (x+1)^7 took 5 min on the calculator (62 s at 500%) and ^8 ran out of memory, because it built all 2^n raw terms first. on the PC ^8 went from 144 ms to 27 ms (it hardly noticed, the calculator is where it hurts). it's a flag simplify's own expands dont pass, collecting everywhere broke two complex tests
- 2nd / alpha / on tapped while its working count for the next key now, busy_poll feeds them into the modifier state, polling at 40 Hz so a quick tap doesnt fall between two polls. cant check that one on the PC, scripted keys never reach keys_peek
- `[on][clear]` on an empty line clears the screen. the history scrolls up out of view, up brings it back, nothing gets deleted
- names. type `s` `i` `n` and its sin, `p` `i` is pi, letters get cut into the longest names from the left on every edit. the catalog grew a `word` field so its the name table. `value [sto->] ab` stores, `[sto->] ab` deletes, `[vars]` lists them. stored = bold = substituted everywhere (live, loops refused when you store), equations only inside solve, solving for a stored letter is an error. letters stay letters in the editor and the split is a view over the run, so cursor / undo / backspace didnt need touching. typed commands like `solve(` arent picked up yet
- the folder menus draw what each row would type, laid out like the line does it, so its a real sin^-1 and a stacked n/d instead of text. built into a scratch editor when the folder opens, one size under normal math
- the screen goes dark after 2 min of nothing (just the backlight, graphx keeps running) and `[2nd][alpha][on]` does it by hand. the key that wakes it gets eaten like the TI does. the idle clock lives in keys_wait and starts when it's entered, so a long solve isn't idling. the OS's own APD never fires while we're running since we don't use GetKey, so this was needed anyway
- `solve(` typed out works now. on enter the word plus its bracket template get folded into the command item the folder would have inserted, so ed_command and the history see the same thing. a `fold` step in editor-test for it
- bold and italic in the draw layer (`draw_text_styled`, `box_t.style`). nothing uses them yet and widths aren't synced, so styled text is a pixel or two off. `[2nd][clear]` clears the line like `[on][clear]`
- roadmap written up with the decisions so far: names (multi-letter vars and typeable functions are one thing, bold = has a value), RAM appvars for storage, the by-hand default, cot/sec/csc as real functions

## 2026-10-15 - math popup, catalog, history browsing

- holding 2nd / alpha with another key works on the calculator now. the scan only finds the first key down and those two come first, so holding alpha hid the arrow. they're read on their own like `[on]` now, a tap counts on release
- `[math]` pops up a little grid at the cursor: = ≠ < > ≤ ≥ ! ∞ ° θ, 1-9/0 pick straight away
- catalog on `[2nd][0]` like the TI: everything a-z, the keys for it on the right, what it does at the bottom, alpha + letter jumps. it's built on a new page thing (`ui/page.c`) so a mode menu can use it later
- deleting a template keeps whats in it: backspace at the start of `sin(x+1)` leaves `x+1`, fractions go inline with brackets if they need them
- history: up goes answer, input, answer before... enter pastes, `[on]`+2 copies, left/right goes inside it to select just a bit. answers get turned back into editor stuff from their boxes (whats on screen) so they paste back the same

## 2026-10-14 - select, copy paste, undo, history

- 2nd / alpha / `[on]` can all be on at once now, `[2nd][alpha]` + letter is a capital. capitals are the same variable as lowercase (nspire does that too). alpha lock is double tap alpha now
- `[alpha]` + arrows selects (Shift+arrows on the PC). it goes item by item and grabs the whole template at the edge of a slot. typing replaces it, templates wrap it, so select `x+1` then `÷` is $\frac{x+1}{\square}$
- `[on]` + 1 / 2 / 3 is cut / copy / paste, `[on]` + `[sto→]` undo. undo keeps copies of the whole line (16 steps), typing a run of characters counts as one
- `↑` off the top goes into the history, enter pastes that entry's input back. not the answer yet, that needs the result turned back into editor stuff
- `[2nd]` + left/right is start/end of the line
- font has ≤ ≥ ≠ ∞ ° now for the `[math]` popup, which isn't done
- +2.7 KB compressed. doesn't matter much since it's an app now, no 64 KB program limit. haven't timed undo copies on the calculator

## 2026-10-14 - sweep fixes, ready to push

- solve now writes the roots like i wanted in case 3 ٩(•̀ᗜ•́)و : $x = \frac{-3-2\sqrt{6}}{5}, \frac{-3+2\sqrt{6}}{5}$. one $x =$, commas, each root one fraction. $\pm$ version will be a form later
- cancel is `[on]`+`[mode]` (Ctrl+Esc on the PC)
- did a nasty input sweep, basically threw ~300 cursed inputs at every command with ASan on and the heap capped like the calculator's. fixed:
	- layout was exponential in nesting depth because a `MAX` macro called a recursive function twice. 16 nested `sin(` froze it, even just typing them
	- expand segfaulted on stuff like `sqrt(2+i)`
	- parse errors leaked memory
	- the decimal formatter overflowed on huge numbers
- still broken from the sweep (not fixing these yet):
	- running out of memory anywhere crashes, the engine never checks malloc
	- $(x+1)^n$ expands into $2^n$ terms before combining, $(x+1)^7$ already fills the heap
	- solve expands factored equations first, so $(x-1)(x-2)\cdots(x-20)=0$ never finishes
	- sorting is $O(n^2)$ with a gcd every compare, so big sums are slow
	- nested derivatives blow up fast
- README, LICENSE and screenshots for making the repo public
- noticed: functions print first in products ($\cos(x)\,x^2$), and derivatives of roots rationalize when they shouldn't (case 1 again)

## 2026-10-14 - flash app

- keypad on the calculator: key repeat, and a quick double tap of the same number doesn't get eaten while it redraws anymore
- sticky 2nd / alpha / `[on]` (shows as `ctl`): tap once for the next key, double tap to lock, tap again to unlock. [2nd][alpha] is alpha lock
- status bar on top with the modifiers, a spinning dot when it's thinking, and battery
- PC keys work like the calculator now (Shift = alpha, Alt = 2nd, Ctrl = `[on]`)
- `make installer` builds it as a Flash App + CEaShell-Installer (submodule, no changes to it). it runs from flash now so there's way more heap
- took forever to figure out: the app crashed on launch before `main` even ran. turns out a Flash App copies its `.data` into RAM at launch and the identity tables were ~16 KB of it, mostly empty padding in fixed size strings. made them `const char *` and it's fine (╯‵□′)╯︵┴─┴
- CEaShell-Installer doesn't have a licence file, CEaShell is GPL-3.0. ok to build and test with, need to ask RoccoLox before actually shipping it

## 2026-10-14 - no more freezes, cancelling

- found the freeze: $\frac{1}{2x}+\frac{1}{x^2}$ (and anything with a plain number fraction in a sum) looped forever. PineappleCAS's eval turned $\frac{1}{2}$ into a number and said nothing changed, then normalize turned it back, forever. upstream bug. those cases are in `fluxion.tests` now
- there's a `working...` thing with moving dots while the engine runs so it doesn't look frozen
- the engine can be cancelled now, it checks a poll function in all the long loops and bails out. no key for it yet
- haven't checked how often the poll runs on the actual calculator

## 2026-10-14 - more engine tests

- `fluxion.tests` went from 68 to 256 tests, every feature + edge cases
- `todo.tests` is a list of stuff the engine gets wrong right now (not run by make)
- the test runner silently stopped at 256 tests per file?? (・_・ヾ so it said 256/256 when there were more. raised it and it says when it skips now

## 2026-10-14 - engine for the forms

- decided forms are a mode (see [ui](../design-choices/ui.md)). engine first, the key isn't picked yet so nothing's wired up
- common denominator: $\frac{1}{x}+1 \to \frac{x+1}{x}$, and it can do factored too, which fixes ACT Q26
- decimal: $\pi$, $e$, roots etc. become numbers, 6 digits like the nspire. Q11, Q45 and Q23 work now
- had to use `float`, `long double` maths was +13 KB and i don't have that. the last digit can be off by one sometimes, oh well. only the irrational bits are floats, the rest stays exact
- renamed the test files to `.tests` since they're their own format
- noticed: $\sqrt[3]{-8}$ gives the complex root instead of $-2$ like the nspire does in real mode

## 2026-10-14 - cancelling in fractions

- enter cancels common factors now: $\frac{x^2-1}{x-1} \to x+1$, ACT Q37 gives $(x+2)^2$
- the ACT wants $x^2+4x+4$ for that but $(x+2)^2$ is smaller so it stays, expanded will be a form
- stuff that doesn't cancel still gets split into one fraction per term, that's the common denominator form problem again

## 2026-10-13 - square factors out of roots

- $\sqrt{35\cdot108} \to 6\sqrt{105}$ (ACT Q8), $\sqrt{72} \to 6\sqrt{2}$, etc
- had to do it in two places because simplify keeps merging $2\sqrt{2}$ back into $2^{3/2}$
- noticed: a fraction in an exponent ($54^{1/3}$) is drawn big and low so it looks like the mixed number $54\frac{1}{3}$ lol

## 2026-10-13 - solve

- `solve(2x+3=7,x)` from the Algebra folder, `=` is `[2nd][math]` (TEST on the TI, `=` is the first thing in it). if you leave out the variable it uses x
- does linear (any coefficients) and polynomials with number coefficients: factor it, then each factor is linear or a quadratic. if there's a cubic left it says it can't solve it instead of giving half an answer
- the parser doesn't know `=`, so the app splits the line itself
- not yet: systems (Q14), inequalities, $ax^2+bx+c$ with letters

## 2026-10-13 - polynomial factoring

- `factor(` works on one variable polynomials like the nspire, pulls out every rational root. ACT Q37 -> $(x+2)^2(x-3)$, Q5 -> $(x+5)(x-6)$
- it was a TODO in PineappleCAS. their gcd factoring actually made Q37 worse so mine runs first ᕦ(¬‿¬)ᕤ
- first version was +10 KB because of big local arrays (the eZ80 is really bad at big stack frames), moved them to the heap and it's ~3 KB
- tests in `test/fluxion-tests.txt` (now `fluxion.tests`)
- $x^2\sin(x)+2x\sin(x)+\sin(x)$ still comes out half factored

## 2026-10-13 - fonts + layout pass

- three font sizes now like TeX (normal / script / scriptscript), from the X11 misc-fixed fonts, `tools/bdf2c.py` converts them. replaced the 8\*8 graphx font
- variables are lowercase like the nspire. $e$ and $i$ are bold so you can tell the constants apart from variables named e and i
- nested fractions get smaller, roots and brackets got some spacing fixes, brackets curve so tall ones stay round
- terms are sorted highest degree first
- `--raw` in the viewer draws exactly what you typed, for layout stuff

## 2026-10-13 - drawing, PC screen

- `src/platform/screen.h` is the only thing that touches the screen. graphx on the calculator, a framebuffer + SDL3 window on the PC
- own 8\*8 font so the PC and calculator draw the exact same pixels (checked, CEmu matches)
- `./build-pc/view 'X^2/2'` opens a window, `--png` saves a picture
- the calculator needs `clibs.8xg` (added to the shipping checklist)
- tried building as an app (.8ek), it builds but you need an installer to put it on, even in CEmu
- nested fractions look bad, same width bars. need the small font

next: the editor

## 2026-10-13 - box layout

- `src/render/layout.c` turns the engine tree into boxes (width, height above/below the line, position). fractions, exponents, roots, brackets that grow, log bases, etc. no graphx in it so it runs on the PC
- spacing stolen from TeX
- the font is a parameter so swapping fonts later doesn't touch the layout
- `./build-pc/layout-dump` prints the box tree
- the engine splits $\frac{x^2-1}{(x+1)^2}$ into two fractions, same form problem as Q26

next: actually drawing it

## 2026-10-13 - PineappleCAS engine in

- copied PineappleCAS's engine into `src/engine/` (everything but its UI). what i change goes in `src/engine/README.md`
- PC build passes all 256 of its tests
- calculator build works on toolchain v15 with no changes, but it's 82 KB so it's compressed now (one ~30 KB `.8xp` instead of 3 files)
- turned off the `missing-field-initializers` warning, there were 146 of them all from the engine
- the $\sqrt{10\pi}$ rationalizing thing is just one flag, `CANONICAL_RATIONALIZE`

next: check it in CEmu, then rendering

## 2026-10-12 - set up, hello world runs

- CE C toolchain v15 in `~/CEdev`, makefile adds it to PATH itself so VS Code just works
- CEmu is `ti-cemu` on here (`cemu` is the Wii U emulator lol). ROM is my own dump, gitignored, never commit it
- OS 5.8.0 so arTIfiCE 2.1, programs through Cesium
- hello world runs
- CEmu send was being annoying, a relative path silently fails if CEmu is already open from another folder. VS Code configs use `${workspaceFolder}` now
- leaning towards PineappleCAS's engine so the time goes into rendering + input, see [platform](../design-choices/platform.md)

---
Back to [code](index.md)
