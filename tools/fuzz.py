# random expressions through build-pc/fluxion, checked numerically against sympy
# usage: python3 tools/fuzz.py [count] [seed] [pos|any] [simplify|expand|factor|derivative|integral|solve|limit|critical|system] [-v]   (pos = letters only get positive test points)
import random, re, subprocess, sys
import sympy as sp
from sympy.parsing.sympy_parser import parse_expr, standard_transformations, implicit_multiplication_application, convert_xor

count = int(sys.argv[1]) if len(sys.argv) > 1 else 300
seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
mode = sys.argv[3] if len(sys.argv) > 3 else "any"
cmd = sys.argv[4] if len(sys.argv) > 4 and sys.argv[4] != "-v" else "simplify"
random.seed(seed)

X, Y = sp.symbols("X Y")
tr = standard_transformations + (implicit_multiplication_application, convert_xor)
rootf = lambda n, v: v ** (1 / sp.sympify(n))
Nn = sp.Symbol("N", integer=True)
names = {"N": Nn, "rootf": rootf, "i": sp.I, "X": X, "Y": Y, "e": sp.E, "ln": sp.log, "abs": sp.Abs, "sign": sp.sign, "sqrt": sp.sqrt, "asin": sp.asin, "acos": sp.acos, "atan": sp.atan, "sec": sp.sec, "csc": sp.csc, "cot": sp.cot, "logb": lambda v, b: sp.log(v, b)}

def leaf():
    return random.choice(["X", "X", "Y", "2", "3", "5", "1/2", "3/4", "e", "pi"])

def gen(d):
    if d == 0:
        return leaf()
    k = random.random()
    a, b = gen(d - 1), gen(d - 1)
    if k < 0.2: return f"({a})+({b})"
    if k < 0.35: return f"({a})_({b})"
    if k < 0.55: return f"({a})*({b})"
    if k < 0.65: return f"({a})/({b})"
    if k < 0.72: return f"({a})^{random.choice(['2', '3', '(1/2)', '(1/3)', '(-1)'])}"
    if k < 0.76: return f"sqrt(({a})*({b}))"
    if k < 0.78: return f"sqrt(-({a}))"
    if k < 0.8: return f"sqrt(({a})/({b}))"
    if k < 0.81: return f"sqrt(({a})^2)"
    if k < 0.82: return f"(({a})^2)^(1/2)"
    if k < 0.84: return f"sqrt({a})"
    if k < 0.88: return f"ln({a})"
    if k < 0.91: return f"abs({a})"
    if k < 0.93: return f"sin({a})"
    if k < 0.94: return f"tan({a})"
    if k < 0.96: return f"e^({a})"
    if k < 0.98: return f"({a})^({b})"
    return f"cos({a})"

FUNCS = ["sinh", "cosh", "tanh", "sqrt", "ln", "abs", "sign", "asin", "acos", "atan", "logb", "sec", "csc", "cot", "sin", "cos", "tan"]
TOK = re.compile("(?:" + "|".join(FUNCS) + r"|root|pi|e|i|X|Y|N)|\d+(?:\.\d+)?|[-+*/^()_,]")
ATOM = r"(?:[\w.]+(?:\((?:[^()]|\([^()]*\))*\))?|\((?:[^()]|\([^()]*\))*\))"

def to_sympy(s):
    # outputs run letters together (Xpi, 3rootX, pie), so tokenize first
    toks = TOK.findall(s)
    if "".join(toks) != s:
        raise ValueError("untokenized")
    out = ""
    for t in toks:
        if t in FUNCS: out += t
        elif t == "_": out += "-"
        elif t == "root": out += " root "
        else: out += " " + t + " "
    out = re.sub("(" + "|".join(FUNCS) + r")\s*", r"\1", out)
    out = re.sub("(" + ATOM + r")\s*root\s*(" + ATOM + ")", r"rootf(\1,\2)", out)
    e = parse_expr(out, local_dict=names, transformations=tr)
    # odd roots of negatives are real here (3root(-8) is -2), sympy's x**(1/3) is the complex one
    return e.replace(lambda t: t.is_Pow and t.exp.is_Rational and t.exp.q > 1 and t.exp.q % 2 == 1,
                     lambda t: sp.real_root(t.base, t.exp.q) ** t.exp.p)

def gen_int():
    # the integrals a calc 1 class has: x^k times something, trig powers, 1/(quadratic), sums of those
    k, a, b = random.randint(1, 3), random.randint(1, 4), random.randint(-3, 3) or 1
    return random.choice([
        f"X^{k}*e^X", f"X^{k}*sin(X)", f"X^{k}*cos({a}X)", "X*ln(X)", f"X^{k}*ln(X)", "atan(X)", f"{a}X*e^({b}X)",
        f"sin(X)^{k + 1}", f"cos(X)^{k + 1}", f"sin({a}X)^2", f"cos({a}X)^3", f"sin(X)^2*cos(X)", "sin(X)*cos(X)",
        f"1/(X^2_{a * a})", f"1/(X^2+{a + b + 4}X+{a + 1})", f"1/(X^2+{a * a})", f"X/(X^2+{a})", f"({a}X+{b})/(X^2_1)", f"1/(X^3_X)",
        f"sqrt({a}X+{b + 4})", f"{a}X*sqrt(X^2+{a})", f"e^({a}X)*{b}", f"{a}/(X+{b + 4})", f"X^{k}+{a}X+{b}", f"tan(X)", f"sec(X)^2",
        f"X^{k + 2}*e^X", f"e^({a}X)*sin({abs(b)}X)", f"e^({a}X)*cos({abs(b)}X)", f"X*atan({a}X)", f"abs({a}X+{b})", f"ln(X)^{k + 1}", f"X*ln(X)^2",
        f"1/(X^2+{a}X+{a + 2})", f"1/(X^2+X_{a})", f"(2X+{b})/(X^2+X+{a})", f"X^2/(X^2+{a})", f"X^3/(X^2+{a})", f"sqrt({a + 3}_X^2)",
        f"sqrt(X^2+{a})", f"1/sqrt(X^2+{a})", f"1/sqrt({a + 3}_X^2)", f"tan(X)^{k + 1}", f"sec(X)^3", f"sin(X)^2*cos(X)^2", f"asin(X)", f"sec(X)", f"cot(X)^{k + 1}", f"csc(X)^3", f"sec(X)^{k + 2}", f"sin({a}X)^2*cos({a}X)^2", f"sin(X)^2*cos(X)^4", f"tan({a}X)^2",
        f"1/sqrt(X^2+{a * 2}X+{a * a + 4})", f"sqrt(X^2+{a * 2}X+{a * a + 3})", f"sqrt(X^2_{a + 1})/X", f"X^2/sqrt({a + 1}_X^2)", f"1/(X^2*sqrt(X^2+{a}))",
        f"X^2/sqrt(X^2_{a + 1})", f"sqrt({a + 1}_X^2)/X^2", f"1/(X^2+{a})^(3/2)", f"1/(X^2_{a + 1})^(3/2)", f"cos(X)^3/sin(X)^2", f"cos(X)^2/sin(X)^2", f"1/cos(X)^4", f"sin(X)^2/cos(X)^4",
        f"1/(X^2+{a})^2", f"({a}X+{b})/(X^2+{a + 1})^2", f"1/({a}X^2+{abs(b) + 3})^2",
        f"1/(1+sin({a}X))", f"1/(1_cos({a}X+1))",
        f"sin({a + 1}X)*cos(X)", f"cos({a + 1}X)*cos({a}X+1)", f"sin({a}X)*sin({a + 2}X)",
        f"sqrt(X)/(X+{a})", f"X/sqrt(X+{a})", f"X^2*sqrt({a}X+1)", f"1/({a}+sqrt(X))", f"sqrt({a}X+1)/(X+1)", f"X^2/sqrt(X+{a})", f"X^3/sqrt({a}X+1)", f"(X^2+{a})^2*(X_{b})",
        f"asin(X)^2", f"X^2*asin(X)", f"X^2*acos(X)", f"X*acos(X)*sqrt(1_X^2)", f"acos(X)^2", f"acos(X)*sqrt(1_X^2)", f"X*acos(X)^2",
        f"atan(X)/X^2", f"ln(X)/X^{k + 1}", f"atan({a}X)/X^2", f"ln(X)/sqrt(X)",
        f"X/(X^4+{a})", f"X^3/(X^8+{a})", f"X^5/(X^4+{a})", f"X/sqrt({a + 1}_X^4)",
        f"1/(X*(X_{a})^2)", f"1/(X^2*(X+{a}))", f"({a}X+{b})/((X_1)^2*(X+{a + 2}))", f"X/((X+{a})^3)", f"1/(X^2*(X^2+{a}))", f"1/((X+1)^2*(X^2+{a}))", f"(X^2+{a})/(X*(X_1)^2)",
        f"1/(X*(X^2+{a}))", f"1/(X^4_{a * a})", f"({a}X+{b})/((X_1)*(X^2+{a + 1}))", f"1/(X^3+{a}X)", f"{b}/({a + 1}X^3+{a + 1}X)",
        f"1/({a + 2}+cos(X))", f"1/({a + 2}+sin({a}X))", f"1/({a + 2}+{a}cos(X))", f"1/(sin(X)+cos(X))", f"1/({a}+{b + 4}cos(X))",
        f"tan(X)^{k + 1}*sec(X)^{k}", f"tan({a}X)^{k + 1}*sec({a}X)^2", f"cot(X)^{k + 1}*csc(X)^{k}", f"(sin(X)+cos(X))^{k + 1}", f"1/(sec(X)+2)", f"(1+sec(X))^2",
        f"({b}X+1)/(X^2+{2 * a}X+{a * a + 1})^2", f"1/({a}X^2+{2 * a}X+{a + 1})^2",
        f"sin(2X)/({a}+cos(X))", f"sin(2X)/({a + 1}+sin(X))", f"cos(2X)/sin(X)", f"sin(2X)*e^(sin(X))",
        f"1/(1+e^X)", f"e^X/({a}+e^(2X))", f"1/(e^X+e^(-X))", f"1/(1+e^({a}X))", f"e^(2X)/(e^X+{a})",
    ])

def nz(lo, hi):
    return random.choice([v for v in range(lo, hi + 1) if v])

def gen_eq():
    # equations (left minus right) the solver has a method for, roots are what a class would ask for
    a, b, c, d = nz(1, 4), nz(-6, 6), nz(-6, 6), nz(-5, 5)
    p, q = nz(-3, 3), nz(2, 4)
    return random.choice([
        f"{a}X^2+{b}X+{c}", f"{a}X+{b}_{c}X+{d}", f"X^3_{b}X^2+{c}X_{d}", f"(X_{b})(X+{c})(X_{d})", f"({a}X+{b})({d}X_{c})",
        f"X^4_{a + 3}X^2+{abs(c) % 3 + 2}", f"({a}X+{b})/({abs(c) % 3 + 1}X+{d})_{p}", f"1/(X_{b})+1/(X+{abs(c) + 1})_{p}",
        f"sqrt({a}X+{abs(b) + 3})_({p}X+{d})", f"sqrt(X+{abs(a)})+sqrt(X)_{abs(b) % 4 + 1}", f"sqrt(X+{abs(b)})_X",
        f"({a}X+{b + 8})^(1/3)_{abs(c)}", f"{a}e^({p}X)_{abs(c) + 1}", f"e^(2X)_{a}e^X+{abs(b)}", f"ln(X+{abs(a) + 1})_{p}",
        f"ln(X)+ln(X+{abs(a)})_{abs(b) % 3 + 1}", f"abs({a}X+{b})_{abs(c) + 1}", f"abs(X_{b})_abs(X+{c})", f"abs(X)_{a}X_{abs(d)}",
        f"2^X_{abs(a) + 5}", f"{a}^X_{abs(b) + 1}", f"sin({a}X)_{p}/{q}", f"cos(X)_{p}/{q}", f"tan(X)_{p}", f"2sin(X)^2_sin(X)_1",
        f"sin(X)^2_{abs(p)}/{q}", f"cos(X)^2_cos(X)", f"sin(X)_cos(X)", f"sin(2X)_sin(X)", f"X^2_{a}X_{abs(b)}X",
        f"X^5+{a}X+{b}", f"cos(X)_X/{q}", f"e^X+{a}X_{abs(b)}", f"ln(X)+{a}X_{abs(c)}", f"X*e^X_{abs(d)}", f"tan(X)_{a}X", f"X^5_{a}X^2_{abs(b)}",
        f"sin(X)_X/{q + 2}", f"e^X_{a}X^2", f"X^4+X_{abs(c) + 1}",
    ])

def solve_check(expr, roots):
    f = to_sympy(expr)
    fam = any("N" in r for r in roots)
    vals = []
    for r in roots:
        v = to_sympy(r)
        vals.append(v)
        if abs(complex(v.subs(Nn, 0).evalf()).imag) > 1e-9:
            return f"root {r} isn't real"
        for k in ((-1, 0, 1, 2) if fam else (0,)):
            vv = complex((f.subs(X, v.subs(Nn, k)) if fam else f.subs(X, v)).evalf())
            if abs(vv) > 1e-6 or vv != vv:
                return f"root {r} gives {vv:.3g}" + (f" at N={k}" if fam else "")
    if fam:
        return None
    truth = sp.solveset(f, X, sp.S.Reals)
    if not isinstance(truth, sp.FiniteSet):
        return None
    # solveset has roots of the combined ln(x)+ln(x+4) the real function isn't defined at
    def defined(t):
        if abs(complex(f.subs(X, t).evalf()).imag) > 1e-9:
            return False
        return all(complex(a.args[0].subs(X, t).evalf()).real >= 0 for a in f.atoms(sp.log))
    tv = sorted(float(t) for t in truth if defined(t))
    gv = sorted(float(v.evalf()) for v in vals)
    gv = [g for i, g in enumerate(gv) if i == 0 or abs(g - gv[i - 1]) > 1e-9]
    if len(tv) != len(gv) or any(abs(x - y) > 1e-6 for x, y in zip(tv, gv)):
        return f"missed or extra: want {tv}"
    return None

def gen_sys():
    # two equations in X and Y (each is left minus right): lines, a line and a circle / hyperbola / parabola
    a, b, c, d, e, f = nz(1, 4), nz(-4, 4), nz(-6, 6), nz(1, 4), nz(-4, 4), nz(-6, 6)
    r = nz(2, 6)
    return random.choice([
        (f"{a}X+{b}Y_{c}", f"{d}X_{abs(e)}Y+{f}"), (f"X+{a}Y_{c}", f"{b}X+{b * a}Y_{f}"), (f"{a}X+{b}Y_{c}", f"X_{d}Y_{e}"),
        (f"X^2+Y^2_{r * r}", f"X+{a}Y_{c}"), (f"X^2+Y^2_{r * r}", f"X_{d}Y"), (f"X^2+Y^2_{r * r}", f"{a}X+{d}Y_{abs(c)}"),
        (f"X*Y_{c}", f"X_Y_{e}"), (f"X*Y_{c}", f"X+Y_{f}"), (f"Y_X^2_{c}", f"Y_{a}X_{b}"), (f"Y_X^2+{abs(c)}", f"X+Y_{f}"),
        (f"X^2_Y^2_{r}", f"X+Y_{d}"), (f"{a}X^2+Y^2_{abs(c) + 4}", f"Y_{d}X"),
        (f"X^2+Y^2_{r * r}", f"X^2_Y_{c}"), (f"{a}X^2+{d}Y^2_{abs(c) + 6}", f"X+Y_{e}"), (f"X*Y_{c}", f"X^2+Y^2_{abs(f) + 5}"),
        (f"X^2+Y^2_{r * r}", f"X*Y_{c}"), (f"X^2_Y_{c}", f"Y^2_X_{f}"),
    ])

def sys_check(e1, e2, sets):
    f1, f2 = to_sympy(e1), to_sympy(e2)
    real = []
    for sx, sy in sets:
        vx, vy = to_sympy(sx), to_sympy(sy)
        for f in (f1, f2):
            v = complex(f.subs({X: vx, Y: vy}).evalf())
            if abs(v) > 1e-6 or v != v:
                return f"set X={sx} Y={sy} gives {v:.3g}"
        cx, cy = complex(vx.evalf()), complex(vy.evalf())
        if abs(cx.imag) < 1e-9 and abs(cy.imag) < 1e-9:
            real.append((round(cx.real, 6), round(cy.real, 6)))
    truth = set()
    for s in sp.solve([f1, f2], [X, Y], dict=True):
        if X in s and Y in s:
            cx, cy = complex(s[X].evalf()), complex(s[Y].evalf())
            if abs(cx.imag) < 1e-9 and abs(cy.imag) < 1e-9:
                truth.add((round(cx.real, 6), round(cy.real, 6)))
    if truth != set(real):
        return f"missed or extra: want {sorted(truth)} got {sorted(set(real))}"
    return None

def gen_lim():
    # calc 1 limits: holes, sin(x)/x kinds at 0, rational / exp / root at inf, a few with no two-sided limit
    a, b, c = nz(1, 4), nz(-4, 4), nz(-4, 4)
    return random.choice([
        (f"((X_{a})(X+{b}))/((X_{a})(X+{c}))", str(a)), (f"(X^2_{a * a})/(X_{a})", str(a)), (f"sin({a}X)/({abs(b)}X)", "0"),
        (f"(1_cos({a}X))/X^2", "0"), (f"(e^({a}X)_1)/X", "0"), (f"ln(1+{a}X)/X", "0"), (f"(sqrt(X+{a * a})_{a})/X", "0"),
        (f"tan({a}X)/X", "0"), (f"({a}X^2+{b}X+1)/({abs(c)}X^2+X+{a})", "inf"), (f"({a}X^3+X)/({abs(b)}X^2+1)", "inf"),
        ("X^2/e^X", "inf"), ("X*e^(0_X)", "inf"), (f"sqrt(X^2+{a}X)_X", "inf"), (f"(1+{a}/X)^X", "inf"), ("ln(X)/X", "inf"),
        (f"{a}X*sin(1/X)", "inf"), (f"1/(X_{a})", str(a)), (f"1/(X_{a})^2", str(a)), (f"abs(X_{a})/(X_{a})", str(a)),
        (f"({a}X+{b})/(X_{c})", str(c)), (f"(sqrt(X+{a * a})_{a})/(X+{b})", "0"),
    ] + one_sided(a))

def one_sided(a):
    # the same kinds with a side picked: |x|/x and 1/x at a point, e^(1/x), roots and logs from the right
    side = random.choice(["1", "-1"])
    return [(f"abs(X_{a})/(X_{a})", str(a), side), (f"1/(X_{a})", str(a), side), ("e^(1/X)", "0", side), ("atan(1/X)", "0", side),
            ("1/X^2", "0", side), ("sqrt(X)", "0", "1"), ("ln(X)", "0", "1"), ("X*ln(X)", "0", "1"), ("X^X", "0", "1"),
            (f"(X^2_{a * a})/abs(X_{a})", str(a), side), ("sin(X)/abs(X)", "0", side), (f"ln(abs(X_{a}))", str(a), side)]

def lim_check(expr, pt, out, side=None):
    f = to_sympy(expr)
    where = {"inf": sp.oo}.get(pt) or sp.Integer(pt)
    sides = ["+", "-"]
    if pt == "inf":
        sides = ["-"]
    elif side:
        sides = ["+" if side == "1" else "-"]
    vals = [sp.limit(f, X, where, dir=d) for d in sides]
    if any(v.has(sp.nan) or v.has(sp.I) or v.has(sp.AccumBounds) for v in vals):
        return "skip"
    dne = len(vals) == 2 and vals[0] != vals[1]
    gave_up = out.startswith("limit(")
    if dne:
        return None if gave_up else f"sympy says no limit ({vals[0]} / {vals[1]}), got {out}"
    if gave_up:
        return "skip"
    want = vals[0]
    if out in ("inf", "-inf"):
        return None if want == (sp.oo if out == "inf" else -sp.oo) else f"want {want}, got {out}"
    got = to_sympy(out)
    if want in (sp.oo, -sp.oo) or abs(complex((got - want).evalf())) > 1e-6:
        return f"want {want}, got {out}"
    return None

def gen_fn():
    # functions a calc 1 class asks critical points of
    a, b, c = nz(1, 4), nz(-5, 5), nz(-5, 5)
    return random.choice([
        f"X^3_{3 * a}X", f"X^3+{a}X^2_{b}X", f"X^4_{2 * a}X^2+{c}", f"{a}X^2+{b}X+{c}", f"X^3_{3 * a}X^2+{b}", f"X*e^(0_{a}X)", f"X*e^({a}X)",
        "X^2*e^(0_X)", f"X/(X^2+{a})", f"(X^2+{a})/X", f"X^2/(X_{a})", "X*ln(X)", "ln(X)/X", "sin(X)+X", "X_2sin(X)", "cos(X)^2", f"sqrt(X)_{a}X",
        "X^(2/3)", f"X^3/(X^2+{a})", f"abs(X_{a})", f"e^X_{a}X", f"X^5_5X^4", f"(X_{a})^2*(X+{abs(b) + 1})",
    ])

def critical_check(f_str, roots):
    # roots of f' as solve gives them, against sympy's *dih*rivative of f
    f = to_sympy(f_str)
    d = sp.diff(f, X)
    fam = any("N" in r for r in roots)
    for r in roots:
        v = to_sympy(r)
        if abs(complex(v.subs(Nn, 0).evalf()).imag) > 1e-9:
            return f"root {r} isn't real"
        for k in ((0, 1) if fam else (0,)):
            vv = complex(d.subs(X, v.subs(Nn, k)).evalf())
            if abs(vv) > 1e-6 or vv != vv:
                return f"f'({r}) = {vv:.3g}"
    if fam:
        return None
    truth = sp.solveset(d, X, sp.S.Reals)
    if not isinstance(truth, sp.FiniteSet):
        return None
    def defined(t):
        try:
            dv = complex(d.subs(X, t).evalf())
            return abs(complex(f.subs(X, t).evalf()).imag) < 1e-9 and dv == dv
        except Exception:
            return False
    tv = sorted(float(t) for t in truth if defined(t))
    gv = sorted(float(to_sympy(r).evalf()) for r in roots)
    gv = [g for i, g in enumerate(gv) if i == 0 or abs(g - gv[i - 1]) > 1e-9]
    if len(tv) != len(gv) or any(abs(x - y) > 1e-6 for x, y in zip(tv, gv)):
        return f"missed or extra: want {tv}"
    return None

def run(expr):
    if cmd == "limit":
        expr = f"limit({expr[0]},X,{expr[1]}{',' + expr[2] if len(expr) > 2 else ''})"
    if cmd == "solve":
        p = subprocess.run(["build-pc/fluxion", "solve", expr], capture_output=True, text=True, timeout=20)
        return re.findall(r"^Root: (.*)$", p.stdout, re.M), re.findall(r"^Numeric: (.*)$", p.stdout, re.M)
    if cmd == "integral":
        expr = f"integral({expr},X)"
    p = subprocess.run(["build-pc/fluxion", "simplify" if cmd in ("integral", "limit") else cmd, expr] + (["X"] if cmd == "derivative" else []), capture_output=True, text=True, timeout=20)
    m = re.search(r"^Output: (.*)$", p.stdout, re.M)
    return m.group(1) if m else None

def points():
    for _ in range(4):
        if mode == "pos":
            yield random.uniform(0.2, 4), random.uniform(0.2, 4)
        else:
            yield random.uniform(-4, 4), random.uniform(-4, 4)

def val(f, px, py):
    try:
        v = complex(f.subs({X: px, Y: py}).evalf())
    except Exception:
        return None
    return v

bad, why, fat, skipped, ok = [], [], [], 0, 0
for i in range(count):
    if cmd == "system":
        e1, e2 = gen_sys()
        try:
            p = subprocess.run(["build-pc/fluxion", "system", e1, e2], capture_output=True, text=True, timeout=20)
        except subprocess.TimeoutExpired:
            bad.append((f"{e1}, {e2}", "TIMEOUT", None)); continue
        if "System: unknown" in p.stdout:
            skipped += 1; why.append((f"{e1}, {e2}", "can't solve")); continue
        if "System: all" in p.stdout:
            ok += 1; continue
        sets = re.findall(r"^Set: X=(\S+) Y=(\S+)$", p.stdout, re.M)
        try:
            problem = sys_check(e1, e2, sets)
        except Exception as ex:
            skipped += 1; why.append((f"{e1}, {e2}", f"{sets} ({ex})")); continue
        if problem: bad.append((f"{e1}, {e2}", sets, problem))
        else: ok += 1
        continue
    expr = gen_int() if cmd == "integral" else gen_eq() if cmd == "solve" else gen_lim() if cmd == "limit" else gen(random.choice([1, 2, 2, 3]))
    if cmd == "critical":
        f_str = gen_fn()
        try:
            d = subprocess.run(["build-pc/fluxion", "simplify", f"deriv({f_str},X,X)"], capture_output=True, text=True, timeout=20)
            dm = re.search(r"^Output: (.*)$", d.stdout, re.M)
            if dm is None:
                skipped += 1; why.append((f_str, "no derivative")); continue
            p = subprocess.run(["build-pc/fluxion", "solve", dm.group(1)], capture_output=True, text=True, timeout=20)
            roots = re.findall(r"^Root: (.*)$", p.stdout, re.M)
        except subprocess.TimeoutExpired:
            bad.append((f_str, "TIMEOUT", None)); continue
        if roots == ["unknown"] or roots == ["all"]:
            skipped += 1; why.append((f_str, f"{dm.group(1)} -> {roots}")); continue
        if roots == ["none"]:
            roots = []
        try:
            problem = critical_check(f_str, roots)
        except Exception as ex:
            skipped += 1; why.append((f_str, f"{roots} ({ex})")); continue
        if problem: bad.append((f_str, roots, problem))
        else: ok += 1
        continue
    if cmd == "limit":
        try:
            out = run(expr)
            problem = lim_check(expr[0], expr[1], out, expr[2] if len(expr) > 2 else None) if out is not None else "skip"
        except subprocess.TimeoutExpired:
            bad.append((expr, "TIMEOUT", "timeout")); continue
        except Exception as ex:
            skipped += 1; why.append((expr, f"{ex}")); continue
        if problem == "skip":
            skipped += 1; why.append((expr, f"{out}")); continue
        if problem: bad.append((expr, out, problem))
        else: ok += 1
        continue
    if cmd == "solve":
        try:
            roots, nums = run(expr)
        except subprocess.TimeoutExpired:
            bad.append((expr, "TIMEOUT", None)); continue
        if roots == ["unknown"] and nums:  # the numeric last resort, each one has to be a zero
            f = to_sympy(expr)
            worst = max(abs(complex(f.subs(X, sp.Float(n)).evalf())) for n in nums)
            if worst > 1e-3 * (1 + max(abs(float(n)) for n in nums)) ** 5:
                bad.append((expr, nums, f"numeric root gives {worst:.3g}")); continue
            ok += 1; continue
        if roots == ["unknown"]:
            skipped += 1; why.append((expr, "can't solve")); continue
        if roots == ["none"]:
            roots = []
        try:
            problem = None if roots == ["all"] else solve_check(expr, roots)
        except Exception as ex:
            skipped += 1; why.append((expr, f"{roots} ({ex})")); continue
        if problem: bad.append((expr, roots, problem))
        else: ok += 1
        continue
    try:
        out = run(expr)
    except subprocess.TimeoutExpired:
        bad.append((expr, "TIMEOUT", None)); continue
    if out is None:
        skipped += 1; why.append((expr, "no output")); continue  # oom or parse failure
    try:
        fi, fo = to_sympy(expr), to_sympy(out)
        if cmd == "derivative":
            fi = sp.diff(fi, X)
    except Exception as ex:
        skipped += 1; why.append((expr, f"{out} ({ex})")); continue
    worst = None
    for px, py in points():
        a, b = val(fi, px, py), val(fo, px, py)
        if cmd == "integral":  # the answer's slope should be the integrand (the constant doesn't show), abs has no clean *dih*rivative
            lo, hi = val(fo, px - 1e-5, py), val(fo, px + 1e-5, py)
            b = (hi - lo) / 2e-5 if lo is not None and hi is not None else None
        if a is None or abs(a.imag) > 1e-9 or abs(a) > 1e8 or a != a:
            continue  # input not real here
        if b is None or abs(a - b) > 1e-4 * max(1, abs(a)):
            worst = (px, py, a, b); break
    if worst: bad.append((expr, out, worst))
    else:
        ok += 1
        if cmd in ("simplify", "expand", "factor") and len(out) > 2 * len(expr) + 60: fat.append((expr, out))  # right, just huge

print(f"{ok} ok, {len(bad)} wrong, {skipped} skipped of {count} (seed {seed}, {mode})")
if fat:
    print(f"{len(fat)} bloated (output over twice the input and 60 characters more)")
    shown = fat if "-v" in sys.argv else fat[:5]
    for expr, out in shown: print(f"  bloat {expr}\n    -> {out}")
if "-v" in sys.argv:
    for expr, r in why: print(f"  skipped {expr} -> {r}")
for expr, out, w in bad:
    if cmd in ("solve", "limit", "critical", "system"):
        print(f"  {expr}\n    -> {out}\n    {w}"); continue
    print(f"  {expr}\n    -> {out}\n    at X={w[0]:.3f} Y={w[1]:.3f}: want {w[2]:.5g}, got {w[3]}" if w else f"  {expr} -> {out}")
