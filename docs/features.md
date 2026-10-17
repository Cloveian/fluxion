# features

everything fluxion does right now, as of 10-17. examples are what you type -> what comes back, most of them straight from `test/fluxion.tests` or the catalog's help text. math is written the way you'd type it (lowercase, `sqrt(`, `^`)

## simplifying and answer forms

- `[enter]` simplifies whatever is on the line, always exact. `1/3` stays `1/3`
- fractions cancel common factors before anything else gets to split them up, like `(x^2-1)/(x-1)` -> `x+1`, `(x^3+x^2-8x-12)/(x-3)` -> $(x+2)^2$
- like terms collect, `(x^2-y^2)-(6x^2+4xy-y^2)` -> $-5x^2-4xy$, `2(3x+5)+2(x+3)` -> `8x+16`
- "by hand" is the default form, it only multiplies out if that makes it smaller. `(x+1)^2-(x-1)^2` -> `4x`, `(x+2)(x-2)` -> $x^2-4$, but `(x+1)^2` stays
- by hand also puts a sum over one fraction, bottom factored and top multiplied out. `2x/3+1/6` -> `(4x+1)/6`, `6x/(x+1)-5/(x+1)` -> `(6x-5)/(x+1)`. kept only if it isn't bigger, and roots stay a sum
- the by hand form won't try to multiply out a 4th power or worse, or anything over ~60 nodes, nobody does that by hand
- four forms (by hand, expanded, factored, decimal). `[2nd][enter]` cycles them, redoes the last answer in the new one and sticks for everything after
- the form shows in the top bar when it isn't by hand
- `[alpha][enter]` gives the last answer as a decimal just once, the next answer is back in whatever form was on (in decimal form it gives the exact one once instead)
- factored form puts it over a common denominator with the top and bottom factored, so `sqrt(2(d-c)/a)` stays like that instead of `sqrt(2d/a-2c/a)` (ACT Q26)
- expanded form: `(x+2)(x-2)` -> $x^2-4$
- decimal form is 6 significant digits like the nspire, trailing zeros dropped, rounded half up. `(5/13)/(5/12)` -> `0.923077`, `-4.9*3^2+30*3+55` -> `100.9`
- decimals from 0.001 up to 999999 print plain, outside that it goes TI style, `7e^15+1` -> `2.28831E7`, `1/1024` -> `9.76563E-4`
- huge exact numbers still get a decimal: a 50 digit integer -> `1E50`
- only the irrational bits become floats, fractions stay exact until the very end so `1/3+2/3` is `1` in decimal too
- rationalize is a mode row. on, `1/sqrt(2)` -> `sqrt(2)/2`; off keeps roots where you wrote them
- no functions you didn't type: `e^(x ln 2)` -> `2^x`, `cot(x)` stays `cot(x)` and never turns into `1/tan(x)`
- a fraction with a negative bottom gets both signs flipped, `17/(-28)` -> `-17/28`
- `expand(` multiplies everything out: `expand((x+1)^2)` -> $x^2+2x+1$, `expand((y+7)^3)` -> $y^3+21y^2+147y+343$
- `expand((x+1)^8)` is instant, the polynomial bits get multiplied out as flat rows of coefficients
- `factor(` writes it as a product (see algebra)
- `eval(` works out the exact value of big powers and factorials, like `eval(2^10)` -> `1024`
- answers that are a list of things (solve, asym, domain, systems, shapes) come back as labelled lines or comma lists, not one blob
- the time an answer took shows next to it when it's 0.1 s or more

## algebra

### factoring

- one letter polynomials over the rationals, every rational root pulled out: `factor(x^3+x^2-8x-12)` -> $(x-3)(x+2)^2$, `factor(x^2-x-30)` -> `(x-6)(x+5)`
- not monic, repeated roots: `2x^2+x-1` -> `(2x-1)(x+1)`, `4x^2-12x+9` -> $(2x-3)^2$, `x^3-3x^2+3x-1` -> $(x-1)^3$
- the number and the powers of x come out front, `2x^2+4x` -> `2x(x+2)`, `x^5-x^3` -> $x^3(x-1)(x+1)$, `-2x^2+8` -> `-2(x-2)(x+2)`
- fraction and decimal coefficients get cleared and the number goes in front: `x^2-1/4` -> `(2x-1)(2x+1)/4`, `0.5x^2-0.5` -> `(x-1)(x+1)/2`
- degree 4 and up: `x^4-5x^2+4` -> `(x-1)(x+1)(x-2)(x+2)`, `x^10-1` -> $(x-1)(x+1)(x^8+x^6+x^4+x^2+1)$
- root search goes up to coefficients of 1000000, so `1000000x^2-1` -> `(1000x-1)(1000x+1)`
- what doesn't factor over the rationals stays like the nspire: `x^2-2`, `x^2+1`, `x^3+8` -> $(x+2)(x^2-2x+4)$
- polynomials inside products, fractions and powers get factored too, `(x^2-1)sin(x)+(x^2-1)cos(x)` -> `(x-1)(x+1)(sin(x)+cos(x))`, `(x^2-1)/(x^2-4)` -> `(x-1)(x+1)/((x-2)(x+2))`
- two or more letters by pattern: `x^2-y^2` -> `(x-y)(x+y)`, `x^2+2xy+y^2` -> $(x+y)^2$, `4x^2-9y^2` -> `(2x-3y)(2x+3y)`, `x^3+y^3` -> $(x+y)(x^2-xy+y^2)$, `2x^2-8y^2` -> `2(x-2y)(x+2y)`. the factors get looked at again so `x^4-y^4` goes all the way
- whole numbers into primes: `factor(360)` -> $2^3 \cdot 3^2 \cdot 5$, `factor(-12)` -> $-2^2 \cdot 3$, `factor(97)` -> `97`

### fractions

- exact rational arithmetic with big numbers (imath): `(5/13)/(5/12)` -> `12/13`, `(1+4/9)/2+1/9` -> `5/6`
- decimals you type in turn into fractions, `0.75*1+0.25*2` -> `5/4` (`1.25` in decimal)
- common denominators: `1+1/x` -> `(x+1)/x`, `a/b+c/d` -> `(ad+bc)/(bd)`, `1/x+1/y+1/z` -> `(yz+xz+xy)/(xyz)`
- lcm of the bottoms, each factor at its biggest power: `1/(x^2-1)+1/(x-1)^2` -> $2x/((x-1)^2(x+1))$ in factored form
- fractions of fractions flatten, `(1+1/x)/(1-1/x)` -> `(x+1)/(x-1)`, `(1+1/2)/(1-1/2)` -> `3`
- cancelling works anywhere in the expression, even inside a function like `sin((x^2-1)/(x-1))` -> `sin(x+1)`

### roots and exponents

- square (and k-th power) factors come out of roots: `sqrt(35*108)` -> $6\sqrt{105}$ (ACT Q8), `sqrt(72)` -> $6\sqrt{2}$, `54^(1/3)` -> $3\sqrt[3]{2}$, `4root(48)` -> $2\sqrt[4]{3}$
- roots multiply and divide out, `sqrt(12)*sqrt(3)` -> `6`, `sqrt(12)/2` -> `sqrt(3)`, `sqrt(9/4)` -> `3/2`
- like roots add: `sqrt(12)+sqrt(27)` -> $5\sqrt{3}$, `sqrt(18)+sqrt(8)` -> $5\sqrt{2}$
- fractional powers: `27^(2/3)` -> `9`, `12^(3/2)` -> $24\sqrt{3}$, `8^(2/3)` -> `4`
- odd roots of negatives stay real like the nspire in real mode: `(-8)^(1/3)` -> `-2`, `(-32)^(3/5)` -> `-8`, `cbrt(-54)` -> $-3\sqrt[3]{2}$
- even roots of negatives go imaginary, `sqrt(-8)` -> $2\sqrt{2}\,i$, `sqrt(9)+sqrt(-16)` -> `3+4i`
- powers of powers and products: `x^2*x^3` -> $x^5$, `(x^2)^3` -> $x^6$, `x/x` -> `1`
- `x^2`, `x^-1`, `^`, `sqrt`, `cbrt` and n-th root all have keys or templates (see editor)

### logs and exponentials

- `ln`, `log` (base 10), `logb(x, b)` (any base, shown as $\log_b$), `e^x` and `10^x`
- integer logs come out exact: `log(1000)` -> `3`, `logb(8,2)` -> `3`, `ln(e^2)` -> `2`. the others just stay, `log(5)` is still `log(5)`
- log rules: `ln(x)+ln(y)` -> `ln(xy)`, differences into a quotient, `logb(a,a)` -> `1`, `a^(c logb(b,a))` -> $b^c$
- inverse pairs cancel: roots of powers, `sin(asin(x))`, `sinh(asinh(x))` etc

### complex numbers

- `i` is on `[2nd][.]`, bold so it isn't a variable. `i^2` -> `-1`, `1/i` -> `-i`
- whole powers of a complex sum multiply out instead of going polar, `(7+4i)^2` -> `33+56i`, `(1+2i)^3` -> `-11-2i`, `(1+i)^8` -> `16`
- `|3+4i|` -> `5`, `e^(i pi)` -> `-1`
- exponentials, logs, sin, cos and tan of complex numbers have identities (`e^(a+bi)` -> $e^a(\cos b + i\sin b)$ etc)
- solve and roots give complex roots of quadratics (see solving)

### numbers

- `!` factorial: `5!` -> `120`, `10!` -> `3628800`
- `int(` rounds down like the TI's, `int(7/2)` -> `3`, `int(-7/2)` -> `-4`
- `|x|` absolute value: `|-5|` -> `5`
- `gcd(12,18)` -> `6`, `lcm(4,6)` -> `12`. with letters they stay as typed (`gcd(x,6)`)
- scientific `E` on `[2nd][,]`: `2E3` -> `2000`, `1.5E-3` -> `0.0015`
- big integers are exact all the way, `sqrt(2^40)` -> `1048576`

## solving

- `solve(eq, x)` from the Algebra folder or typed out, `=` is `[2nd][math]`. leave out the variable and it's x. answers written one `x =` with commas: `solve(x^2-x-30=0,x)` -> `x = -5, 6`
- no `=` means `= 0`: `solve(x^3-x,x)` -> `x = -1, 0, 1`
- roots come out sorted by their value, each one a single fraction, number first: `solve(5x^2+6x=3,x)` -> $x = \frac{-3-2\sqrt{6}}{5}, \frac{-3+2\sqrt{6}}{5}$
- `true` when every value works, `false` when none do, like `solve(x+1=x+1,x)` -> `true`, `solve(x+1=x+2,x)` -> `false`
- "can't solve this one yet" instead of half an answer
- linear with any coefficients: `solve((100+60+80+30+x)/5=72,x)` -> `x = 90`, `solve(2(3x+5)+2(x+3)=360,x)` -> `x = 43`
- fraction and decimal coefficients are fine, `solve(x^2=1/4,x)` -> `x = -1/2, 1/2`, `solve(0.5x^2=2,x)` -> `x = -2, 2`
- polynomials by factoring, then the quadratic formula: `solve(x^2-2x-1=0,x)` -> $x = 1-\sqrt{2}, 1+\sqrt{2}$, `solve(x^4-5x^2+4=0,x)` -> `x = -2, -1, 1, 2`
- a repeated root only shows once, `solve(9x^2-6x+1=0,x)` -> `x = 1/3`
- cubics with no rational root, exact: three real roots in cos form, `solve(x^3-3x+1=0,x)` -> $2\cos(8\pi/9), 2\cos(4\pi/9), 2\cos(2\pi/9)$; one real root by cardano, `solve(x^3+x+1=0,x)` -> cube roots of $-108 \pm 12\sqrt{93}$ over 6
- complex roots of quadratics: `solve(x^2+2x+5=0,x)` -> `x = -1-2i, -1+2i`, `solve(x^2=-4,x)` -> `x = -2i, 2i`, `solve(x^4=1,x)` -> `x = -1, 1, -i, i`
- `roots(p, x)` is the same thing under another name, `roots(x^2+1,x)` -> `x = -i, i`
- other letters in the coefficients: `solve(6y=5x-1,x)` -> `x = (6y+1)/5` (ACT Q9), `solve(ax+b=0,x)` -> `x = -b/a`, `solve(ax+bx=c,x)` -> `x = c/(a+b)`
- quadratics in letters by the formula: `solve(x^2+bx+c=0,x)` -> $x = \frac{-b \pm \sqrt{b^2-4c}}{2}$ (as two answers), `solve(y=x^2+3,x)` -> $-\sqrt{y-3}, \sqrt{y-3}$, `solve(a=pi r^2,r)` works too
- when the variable is in there once, it peels the layers off: `solve(y=2^x,x)` -> $\log_2 y$, `solve(sqrt(x)=2,x)` -> `4`, `solve(e^(2x)=5,x)` -> `ln(5)/2`, `solve(ln(x)=2,x)` -> $e^2$, `solve(x^3=y,x)` -> $\sqrt[3]{y}$
- x under a fraction bar: over one denominator, the top solved, roots that zero the bottom dropped. `solve((3x-y)/(x+y)=5/8,x)` -> `x = 13y/19`, `solve(1/x+1/(x+1)=1,x)` works, `solve(x/(x-1)=1/(x-1),x)` -> `false`
- answers that would be undefined get dropped, so `solve(1/x^2=0,x)` and `solve(e^x=0,x)` are `false` and not `1/0` or `ln(0)`
- odd roots stay real, `solve(x^3+1=y,x)` -> $-\sqrt[3]{1-y}$
- solving for a letter that has a stored value is refused with "x is stored, delete it to solve for x" instead of quietly solving for a number
- trig equations, the whole family like the nspire: `solve(sin(x)=1/2,x)` -> $\pi/6 + 2\pi n, 5\pi/6 + 2\pi n$, `solve(cos(x)=0,x)` -> $\pi/2 + \pi n$, `solve(tan(x)=1,x)` -> $\pi/4 + \pi n$, `solve(sin(x)=0,x)` -> $\pi n$
- or just one turn (a mode row), 0 up to 2pi or 360: `solve(cos(2x)=0,x)` -> $\pi/4, 3\pi/4, 5\pi/4, 7\pi/4$. shifted and multiplied arguments work for free
- in degree mode `solve(sin(x)=1/2,x)` gives `30, 150` in one turn, `360n + 30...` in general

### inequalities

- `<`, `>`, `<=`, `>=` from the `[math]` popup, inside `solve(`. `<` then `=` typed as two characters counts as `<=`
- done by a sign chart like on paper: roots of the top and bottom sorted, one test point per gap, ends in or out by the sign and whether it's a hole
- `solve(x^2-4<0,x)` -> `-2 < x < 2`, more than one piece comes back as separate lines
- with no letters left, `3 < 5` style gives `true` or `false`

### systems

- `system(eq, eq, ..., x, y)`, up to 4 equations in as many letters. no letters named and it takes the ones it finds, a to z
- linear by gauss-jordan: `system(2x-y=7,x+y=2,x,y)` -> `x = 3, y = -1`, `system(x+y+z=6,x-y=0,x+2z=9)` -> `x = 1, y = 1, z = 4`
- fractions stay exact, `system(3x+2y=1/2,x-4y=3)` -> `x = 4/7, y = -17/28`
- letters in the coefficients work too, `system(ax+y=b,x-y=0,x,y)` -> `x = b/(1+a), y = b/(1+a)`
- no solution is `false` (ACT Q14: `2x-y=7, -4x+2y=2`), the same line twice says "infinitely many, they say the same thing"
- two nonlinear equations by substitution, every pair checked back in both, one line per solution with "or" between: `system(x^2+y^2=25,y=x+1)` -> `x = -4, y = -3` or `x = 3, y = 4`, `system(xy=6,x+y=5)` -> `x = 2, y = 3` or `x = 3, y = 2`, `system(y=x^2,y=2x+3)`, `system(1/x+1/y=1,x+y=4)` -> `x = 2, y = 2`
- up to 6 solution sets, so a circle and an ellipse crossing give all 4 points
- a circle and a line that miss, `system(x^2+y^2=1,y=5)` -> `false`

## calculus

- `d/dx(` from the Calculus folder, drawn as a real $\frac{d}{dx}$: `d/dx(x^3)` -> $3x^2$
- product, chain, quotient and power rules, the trig, inverse trig, hyperbolic, log and exponential derivatives: `d/dx(x^2 sin(x))` -> $2x\sin(x) + x^2\cos(x)$
- `cot`, `sec`, `csc` have their own, `d/dx(cot(x))` -> $-\csc^2 x$, `d/dx(sec(x))` -> `tan(x)sec(x)`
- derivative at a value, typed out for now (d/dx at in the Calculus folder types the `deriv(` for you): `deriv(x^2,x,3)` -> `6`, drawn with a $|_{x=3}$
- the d/dx template is with respect to x
- integrals and limits aren't in yet (greyed out in the folder)

## trig

- `sin cos tan` on their keys, `sin^-1 cos^-1 tan^-1` on `[2nd]`, `csc sec cot` on `[on]` + `sin cos tan`. inverses drawn as $\sin^{-1}$
- `sinh cosh tanh` and their inverses from the Functions folder or typed out
- exact values on the unit circle, every multiple of pi/6 and pi/4: `sin(pi/6)` -> `1/2`, `cos(3pi/4)` -> $-\sqrt{2}/2$, `tan(pi/3)` -> $\sqrt{3}$, `cot(pi/4)` -> `1`, `sec(pi/3)` -> `2`, `csc(pi/6)` -> `2`
- inverse trig constants: `asin(1/2)` -> `pi/6`, `acos(0)` -> `pi/2`, `atan(1)` -> `pi/4`, `acos(-1/2)` -> `2pi/3`
- hyperbolic ones at 0 too, `sinh(0)` -> `0`, `cosh(0)` -> `1`, `tanh(0)` -> `0`, and the inverses (`acosh(1)` -> `0`)
- pythagorean: `sin(x)^2+cos(x)^2` -> `1`, `cosh(x)^2-sinh(x)^2` -> `1`
- double angle, `2sin(x)cos(x)` -> `sin(2x)`, `cos(x)^2-sin(x)^2`, `2cos(x)^2-1` and `1-2sin(x)^2` -> `cos(2x)`
- quotients: `sin(x)/cos(x)` -> `tan(x)`, `tan(x)cos(x)` -> `sin(x)`, `sinh(x)/cosh(x)` -> `tanh(x)`
- `sec(x)cos(x)`, `csc(x)sin(x)` and `cot(x)tan(x)` are all `1`
- cofunctions like `sin(pi/2-x)` -> `cos(x)`
- periods and symmetry: `sin(x+2pi)` -> `sin(x)`, `tan(x+pi)` -> `tan(x)`, `cos(-x)` -> `cos(x)`, `sin(-x)` -> `-sin(x)`
- trig of inverse trig, `cos(asin(x))` -> $\sqrt{1-x^2}$, `sin(atan(x))` -> $x/\sqrt{1+x^2}$
- degree mode, exact: `sin(30)` -> `1/2`, `asin(1/2)` -> `30`. `sin(x)` still shows as `sin(x)`, not `sin(pi x/180)`
- trig equations, see solving
- each group of identities can be switched off in Tools: basic, trig, hyperbolic, complex, trig constants, inverse trig constants

## functions

- `domain(f, x)`: zeros of bottoms, even roots and logs of something linear, tan/sec/cot/csc families, asin/acos/atanh/acosh ranges. `domain(sqrt(x-2),x)` -> `x >= 2`, `domain(ln(x),x)` -> `x > 0`, `domain(sqrt(4-x)/(x+1),x)` -> `x <= 4` except `x = -1`
- the tightest bound wins, holes past a bound go, a hole on a closed bound opens it. `domain(sqrt(x-2)+sqrt(5-x),x)` -> `x >= 2`, `x <= 5`
- one point or nothing: `domain(sqrt(x-2)+sqrt(2-x),x)` -> `x = 2`, `domain(sqrt(x-5)+sqrt(2-x),x)` -> `no real x`
- `domain(tan(x),x)` -> except $x = \pi/2 + \pi n$
- a root or log of something it can't read gets `(and maybe more)` instead of lying, like `domain(sqrt(x^2-4),x)`
- `range(f, x)`: domain and asym cut it into pieces, each spans its turning points (f' = 0) and what its ends head towards. `range(x^2+1,x)` -> `y >= 1`, `range(1/x,x)` -> `y < 0` or `y > 0`, `range(x e^-x,x)` -> `y <= 1/e`, `range(3,x)` -> `y = 3`
- `asym(f, x)`: holes, vertical, horizontal and slant asymptotes of a fraction, a line for each kind it has, `none` if nothing. `asym((x^2-1)/(x-1),x)` -> `hole x = 1`, `asym((2x^2+1)/(x^2-4),x)` -> `vertical x = -2, 2`, `horizontal y = 2`
- slant ones come from polynomial division, `asym((x^2+1)/x,x)` -> `vertical x = 0`, `slant y = x`. an exact division is a hole, not an asymptote: `asym((x^2+x)/x,x)` -> `hole x = 0`
- top and bottom are read as typed, so the hole doesn't get simplified away first
- `inverse(f, x)` solves y = f(x) for x and swaps back. `inverse(2x+3)` -> `(x-3)/2`, `inverse(x^2)` -> `-sqrt(x), sqrt(x)`, `inverse(x^3+1)` -> real cube root
- `amplitude(f, x)`: |A| of `A sin(Bx+C)+D` or cos, and $\sqrt{a^2+b^2}$ for `a sin(u)+b cos(u)`. `amplitude(3sin(2x)+1,x)` -> `3`. anything else is "can't tell the amplitude of that one"
- `piecewise(f1, c1, f2, c2, ..., x=v)`: no `x=v` and it just shows the pieces as rows (`x^2, if x < 0`). with one, the first condition that holds gets worked out exactly: `piecewise(x^2,x<0,2x+1,x>=0,x=3)` -> `7`
- piecewise conditions can be chains in one letter, like `1<x<=3`, `x != 0`, `x = 0`
- `fromroots(r1, r2, ..., x)`: the polynomial with those roots, expanded. `fromroots(2,-3)` -> $x^2+x-6$, a letter at the end is the variable
- `roots(p, x)` is under solving

## geometry and lines

- `slope(x1,y1,x2,y2)`: `slope(1,2,3,4)` -> `1`, vertical says "undefined, it's vertical"
- `dist(x1,y1,x2,y2)`, exact: `dist(0,0,3,4)` -> `5`, letters fine
- `mid(x1,y1,x2,y2)` as a point: `mid(0,0,4,2)` -> `(2, 1)`
- `par(m,x1,y1)` is the line with slope m through a point, `par(2,1,1)` -> `y = 2x - 1`
- `perp(m,x1,y1)`: slope -1/m through the point, `perp(2,1,1)` -> `y = (-x+3)/2`, and `x = x1` when m is 0
- `triangle(a,b,c,ab,bc,ca)`: any 3 of the sides and angles, empty or 0 for the rest (`ab` is the angle between a and b). sss, sas, asa, aas and ssa, decimals, then the area. `triangle(3,4,5,,,)` -> the three angles and `area = 6` (`ab = 90` in degrees, `1.5708` in radians)
- the ambiguous ssa case gives both triangles side by side with an "or"
- it says why when nothing fits: "those sides don't close up, one's too long", "no triangle fits, that side's too short", "the angles add up to pi or more"
- shape formulas, every one for that shape at once, exact, letters fine (`circle(r)` gives the formulas themselves):
    - `circle(r)`: area, circumference, diameter. `circle(3)` -> `area = 9pi`
    - `rect(l,w)`: area, perimeter, diagonal. `rect(3,4)` -> `area = 12`, `diagonal = 5`
    - `square(s)`: `square(2)` -> `area = 4`, `diagonal = 2sqrt(2)`
    - `trap(a,b,h)` is just area, `trap(3,5,2)` -> `area = 8`
    - `sphere(r)`: volume, surface area. `sphere(3)` -> `volume = 36pi`
    - `cyl(r,h)`: volume, surface area, side area. `cyl(2,5)` -> `volume = 20pi`, `surface area = 28pi`
    - `cone(r,h)`: volume, slant, surface area. `cone(3,4)` -> `volume = 12pi`, `slant = 5`
    - `cube(s)`: volume, surface area, diagonal. `cube(2)` -> `diagonal = 2sqrt(3)`
    - `box(l,w,h)`: `box(2,3,6)` -> `volume = 36`, `diagonal = 7`
    - `polygon(n)`: angle sum, interior and exterior angle (regular), diagonals, in the angle mode. `polygon(6)` -> `720`, `120`, `60`, `9` in degrees, `4pi`, `2pi/3`, `pi/3`, `9` in radians

## conics

- `ellipse(eq)`: center, vertices, co-vertices and foci by completing the square. `ellipse(4x^2+9y^2=36)` -> center `(0, 0)`, vertices `(-3, 0), (3, 0)`, co-vertices `(0, -2), (0, 2)`, foci $(-\sqrt{5}, 0), (\sqrt{5}, 0)$
- `hyperbola(eq)`: center, vertices, foci and the asymptotes $y = k \pm (b/a)(x-h)$. `hyperbola(x^2/9-y^2/16=1)`
- both work the other way round too, `ellipse(h,k,vx,vy,fx,fy)` / `hyperbola(h,k,vx,vy,fx,fy)`, center + a vertex + a focus -> the equation
- a circle passed to ellipse puts both foci on the center
- it says what you gave it instead: "x^2 and y^2 have opposite signs, a hyperbola", "that's just one point", "that's two lines crossing", "no points, squares can't add up to less than 0"
- lined up with the axes only, an `xy` term says so

## probability and combinatorics

- `nCr(5,2)` -> `10`, `nPr(5,2)` -> `20`, `nCr(50,2)` -> `1225`. from the menu (typed letters come out lowercase)
- `pand(pa,pb)`: both happen, independent events. `pand(1/2,1/3)` -> `1/6`
- `por(pa,pb)` is at least one, independent too. `por(1/2,1/3)` -> `2/3`
- probabilities outside 0 to 1 get "probabilities go from 0 to 1", letters are fine
- `!`, `gcd(`, `lcm(` and `factor(` on a whole number (primes), see numbers

## the 2D editor and input

- it's nspire style, mostly plain typing with templates for the 2D bits, and the cursor is a line like on a computer
- templates: fraction, exponent, square root, n-th root (index first), cube root, `|x|`, `log_b`, brackets, and every function with its brackets (`sin(▯)`, `expand(▯)`)
- `/` is typed inline and drawn as a stacked fraction once it's an answer, `6/4` -> `3/2`
- `[alpha][y=]` (or `[alpha][x,t,theta,n]`) is a fraction that grabs the term before the cursor: `5+9` then fraction is `5+` 9 over an empty bottom, cursor in the bottom. it goes back to the last `+ - = ,` or open `(`, so `2x`, `(a+b)` and `sin(x)` go up whole. nothing there and it's an empty fraction with the cursor on top
- `^` puts the cursor in the exponent, `[x^2]` and `[x^-1]` make the exponent and step back out
- arrows go into and out of templates slot by slot. up/down: top and bottom of a fraction, root index and radicand, into and out of an exponent. right after an exponent, up goes back into it
- `[2nd]` + left/right jumps to the start / end of the line
- `[clear]` is backspace: a template on the left gets stepped into, at the start of a template it's removed and what was in it stays (`sin(x+1)` -> `x+1`), a fraction goes inline with brackets if it needs them
- `[del]` deletes to the right, a whole template keeping its insides the same way
- ghost brackets: a typed `(` gets a grey `)` at the end of the slot until you type one, and if you never do it closes there anyway. `2(x+1` is `2(x+1)`
- `)` closes the last open `(`, with none open it steps out of the template you're in
- implicit multiplication like `2x`, `x(x+1)`, `2sqrt(3)`. multiplying shows as nothing, or a dot between two numbers (`2*3`)
- `[(-)]` is negate and `[-]` is subtract, spaced differently like on the TI
- an operator (`+ - * / ^`) first on an empty line puts `ans` in front, the last answer. not `(-)`, that's a new number
- `ans` is drawn bold like a stored name, typing a-n-s works too. an error or "stored" doesn't wipe it
- names: a run of letters is read as the longest names from the left, redone on every edit. `s i n` is sin, `sinh` is one function, `pin` is pi n, `xi` is x i. a stored `ab` makes `a b` read as ab, `a*b` gets the product
- a name is drawn as one thing and turns back into letters while the cursor is inside it
- commands typed out letter by letter (`solve(2x=6,x)`) are folded into the same command the menu would type, as long as their brackets are the whole line
- `[2nd][alpha]` + letter is a capital, same variable as the lowercase one (nspire's case insensitive too)
- `theta` typed out or `[alpha][3]`
- `[math]` pops up a grid at the cursor: `= != < > <= >= ! infinity degree theta`, arrows + enter or 1-9 and 0 straight away
- `=` is `[2nd][math]` too, where TEST is on the TI
- empty slots are drawn as dotted boxes, and enter with one still empty says "fill in the empty boxes first"
- a long line scrolls sideways to keep the cursor in view
- with something selected, typing replaces it, `(`, `/`, `sqrt`, `sin` etc go around it (select `x+1` then `/` -> `(x+1)/▯`), `^` makes it the base

## history, copy/paste, undo

- history on top (input left, answer right), input line at the bottom, newest at the bottom like the nspire
- up off the top of the line highlights the last answer, then its input, then the answer before...
- `[enter]` on a highlighted entry pastes it at the cursor, `[on][2]` copies it, `[clear]` goes back to the line
- left/right goes inside an entry, then arrows move and `[alpha]` + arrows select just a part to paste or copy
- answers get turned back into editor items from what's on screen, so they paste back the same as they look
- `[alpha]` + left/right selects, a whole item at a time, and the whole template at the edge of a slot
- left/right with a selection drops it on that side, like any text box
- `[on]` + `[1] [2] [3]` is cut, copy, paste (they're next to each other like X C V)
- `[on]` + `[sto->]` undoes, 16 steps back, a run of typing is one step. running a line can be undone too, the line comes back
- `[on][clear]` clears the line (`[2nd][clear]` too). on an empty line it clears the screen instead. the history just scrolls out of view, nothing's deleted and up brings it back
- 16 entries in a session, the oldest scroll off
- the history survives quitting: saved to an appvar on the way out (`FLXHIST`, 12 KB max, oldest dropped first), back on the next start
- "clear history" in Tools empties it
- stacked answers (asym, domain, systems, shapes) can't be browsed into or saved, they go with the session

## menus, catalog, keys and modifiers

- the five keys under the screen open folders, names along the bottom like the nspire: `[y=]` Algebra, `[window]` Calculus, `[zoom]` Functions, `[trace]` Templates, `[graph]` Tools
- up/down move, left/right switch folders, enter or 1-9 picks, the same key again or `[clear]` closes
- each row is drawn as what it would type, a real $\sin^{-1}$ and a stacked n/d, one size under normal math
- stuff that isn't in yet is greyed out with a `*`, Tools rows have `[x]` / `[ ]` and stay open so you can flip several
- the catalog on `[2nd][0]` like the TI: Algebra, Calculus, Functions, Templates, Keys (the ones with no folder), then All a-z. headers the cursor skips
- the catalog shows the keys for each item on the right and what it does at the bottom, word wrapped. `[alpha]` + letter jumps (again to walk through them), the soft keys jump to a section, left/right a page at a time, enter types it
- keys mean what they mean on the TI: `[2nd][^]` pi, `[2nd][/]` e, `[2nd][.]` i, `[2nd][x^2]` sqrt, `[2nd][ln]` e^, `[2nd][log]` 10^, `[2nd]` + sin cos tan for the inverses, `[2nd][,]` E, `[2nd][mode]` quit
- `[alpha]` + a key types the letter printed above it, x has its own key
- `[2nd]`, `[alpha]` and `[on]` are separate modifiers and any of them can be on at once
- tap = for the next key, double tap = lock, tap again = off. a double tap is two taps within 0.4 s with nothing in between
- holding one down with the key works too (holding alpha and pressing an arrow selects). on the calculator they're read on their own so holding one doesn't hide the key
- the top bar has padlocks. the letter on its own (2, A, C) when it's on for one key, a closed padlock with the letter punched out when it's locked. each has its own slot so they don't shuffle
- modifier taps while the engine is working still count for the next key
- `[on]` is Ctrl, `[on][mode]` cancels a slow answer
- arrows, `[clear]` and `[del]` repeat while held, 250 ms then 35 a second
- a quick double tap of the same key doesn't get eaten while it redraws, with 20 ms of debounce after a press
- the key that started the program isn't taken as the first press

## modes and settings

- `[mode]` opens the mode page, enter flips a row, the help for each row at the bottom
- angle: radians / degrees. degrees puts a pi/180 on every trig argument before it runs, so it stays exact
- trig answers: general (pi/6 + 2pi n) or one turn (0 to 2pi, or 0 to 360)
- simplest: by hand / expanded / factored / decimal, the same as `[2nd][enter]`
- rationalize: on / off
- theme is dark or light, light is easier outside
- `[2nd]` up/down is brightness, 8 steps, from anywhere (even with an answer highlighted). starts where the OS was on the first run, and the OS's own level is put back on the way out
- settings are saved after every flip (`FLXMODE` appvar), so they stick even if the app dies
- the Tools folder has the six identity groups on or off, and clear history

## variables and storage

- `value [sto->] name` stores, one letter or up to 8: `2x+1 [sto->] ab`. `[sto->] name` on its own deletes it
- a stored value is a whole expression, or an equation (both sides kept, only usable inside solve and system)
- stored = bold = substituted everywhere, live: change what one uses and it follows
- `e` and `i` are names too, bold, so they're never variables
- loops get refused when you store, not later: `a+1 [sto->] a` -> "a would loop"
- can't store to a builtin, you get "sin is built in, pick another"
- `ans` in a stored value becomes the answer itself, so it doesn't move with the next answer
- `[vars]` lists the stored names and their values, enter deletes one
- 32 names at once, saved in an appvar (`FLXNAMES`) and back on the next start
- history, names and settings live in RAM appvars, so they survive quitting but not a RAM reset. on the PC they're files in `build-pc/store/`

## display and rendering

- a box layout straight from the engine's tree with TeX's spacing: stacked fractions, raised exponents, roots with an index, growing brackets, $\log_b$ subscripts, $\frac{d}{dx}$
- 12 bitmap fonts converted from X11 misc-fixed (public domain) by `tools/bdf2c.py`, proportional so 1 and i are narrow. normal math is 9x15
- 4 sizes like TeX, exponents and nested fractions get smaller, and the sizes drop if something nests deeper
- brackets curve so tall ones stay round, the root sign meets its bar, a 2 px gap before a root index so `2 cbrt(x)` doesn't read as `2^3 sqrt(x)`
- `ln|x|` instead of `ln(|x|)`, the bars already do the brackets' job
- terms sorted highest degree first like a textbook, alphabetical within a degree: `x^2` before `xy` before `y^2`
- variables are lowercase like the nspire, `e` and `i` and stored names bold
- a dot between factors only when it'd read as one number otherwise (`2*3`, `3*(1/2)`)
- solve answers read as a pair, number first: `(-3 - 2sqrt(6))/5, (-3 + 2sqrt(6))/5`
- labelled answers in a column (`vertical x = -2, 2`, `center (0, 0)`), "or" between solution sets
- status bar on top: modifier padlocks, a dot going round while it works (so a slow simplify doesn't look frozen), the form name, and a battery in 4 bars (read every 10 s)
- the five folder names along the bottom, the open one highlighted
- messages show at the right above the line, errors in the history in place of an answer
- double buffered on the calculator, the busy dot draws straight on the screen so nothing flashes
- the PC draws the exact same pixels as the calculator (checked in CEmu)
- layout is linear in nesting depth now, 16 nested `sin(` used to freeze it

## on the calculator: speed, memory, robustness

- runs as a Flash App (way more heap than a program) or a compressed program from Cesium
- `[on][mode]` stops a slow answer, the engine checks for it in every long loop and leaves a valid half done tree that's thrown away ("cancelled")
- the engine polls the keys 40 times a second while it works and animates 10 times a second
- out of memory says "out of memory" instead of crashing. every malloc goes through one wrapper and a NULL unwinds like a cancel
- found with fault injection (`PCAS_FAIL_ALLOC=n` fails the nth allocation on the PC), sweeps of up to 55192 runs, every crash and hang site fixed
- a nasty input sweep, ~300 cursed inputs under ASan with the heap capped like the calculator's. huge exponents are capped (`(x+1)^(10^9)` doesn't try), `sqrt(2+i)` doesn't crash
- the screen goes dark after 2 min of no keys (just the backlight), `[2nd][alpha][on]` does it by hand. any key wakes it and that key gets eaten like the TI's own APD. a long solve doesn't count as idle
- identities that can't apply are skipped without even being parsed (by which functions the expression has), the first simplify used to take 3 s
- expand collects like terms as it goes and does polynomials as flat coefficient rows: `(x+1)^7` was 5 min and `^8` ran out of memory, now instant
- the term sort does gcds of monomials directly, `expand((x+y+z+1)^4)` went 98 -> 11 ms on the PC
- one-turn trig solving only tries the n that can land in range
- upstream bugs fixed: `1/(2x)+1/x^2` hung forever, `(1+4/9)/2+1/9` gave `119/18`
- the identity tables are `const` so they don't eat 16 KB of RAM at launch
- `float` maths for decimals (long double was +13 KB)

## building, PC tools, installer

- `make`: `bin/FLUXION.8xp`, compressed into one program, with the CE C toolchain v15. needs `clibs.8xg` on the calculator
- `make installer`: Fluxion as a Flash App plus CEaShell-Installer (a pinned, unmodified submodule): `FLUXINST.8xp` and the app split into `FLUXAPP` appvars. send them all, run `FLUXINST` once, it shows up under apps
- `make -f pc.makefile`: the engine with PineappleCAS's command line, `./build-pc/fluxion simplify '2X+X'`, also `factor`, `expand`, `derivative`, `gcd`
- `make -f pc.makefile test`: PineappleCAS's 256 tests, Fluxion's 415 (factor, solve, csolve, cancel, comdenom, decimal, asym, domain, system, one turn...), and the identity mask check
- `test/todo.tests` has what the engine still gets wrong, with what it gives now
- `./build-pc/fluxion-app`: the whole app in an SDL3 window. Shift is `[alpha]`, Alt `[2nd]`, Ctrl `[on]` (tapped alone), Shift+letter a capital, Shift+arrows select, F1-F5 the soft keys, `~` negate, `=` is `[2nd][math]`, Backspace `[clear]`, Esc quits, numpad works
- `fluxion-app --png out.png --scale 2 KEY KEY ...`: scripted key presses (with `on+`, `2nd+`, `alpha+` for held ones) then a PNG, for screenshots and tests
- `./build-pc/view 'X^2/2'` draws expressions and their answers like the calculator, `--raw` for no simplify, `--ladder` to compare fonts
- `./build-pc/layout-dump` prints the box tree
- `./build-pc/editor-test t:2 frac t:1 ...`: editor steps -> what the engine gets + a picture
- `FLX_DUMP=1` prints the tree about to be drawn
- `tools/bdf2c.py` makes the fonts
- `preview.py` serves the docs locally (mkdocs), design docs in `docs/` with the decision matrices
- everything except `src/platform/` is the same code on the PC and the calculator

## not yet

- integrals and limits (greyed out in Calculus)
- substitute, the engine has it but there's no syntax for it in the app
- `eval(` is exact only, no decimal approximation (the decimal form does that)
- commands can't be nested or mixed with other stuff, `expand(...)` has to be the whole line
- `=` only works in solve, system, ellipse, hyperbola and piecewise
- a d/dx template for other letters, or one with a slot for the value (d/dx at goes in typed out)
- inequalities and range with trig in them
- factoring over the irrationals (`x^2-2`) or into quadratics with no rational root (`x^6-1` stops at $x^4+x^2+1$)
- tilted conics (an `xy` term)
- a brace template for piecewise
- complex roots of cubics (written, but gated off, they take minutes to draw on the calculator)
- systems past 4 equations, nonlinear past two
- slow answers on the calculator for big expansions, `(x+y+z+1)^4` is still minutes
- dividing complex numbers, `(1+i)/(1-i)` stays a fraction instead of `i`
- `sqrt(x^2)` is `x`, not `|x|`
- from the roadmap: wrap / scroll for long answers, transformations, percent, defining f(x), unit circle display, show the work steps, sets, matrices, unit conversion, graphs
- the full key map (every key times plain / 2nd / alpha / 2nd+alpha / on)
