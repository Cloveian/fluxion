# random expressions through build-pc/fluxion, checked numerically against sympy
# usage: python3 tools/fuzz.py [count] [seed] [pos|any] [simplify|expand|factor|derivative|integral|solve|limit] [-v]   (pos = letters only get positive test points)
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
names = {"N": Nn, "rootf": rootf, "i": sp.I, "X": X, "Y": Y, "e": sp.E, "ln": sp.log, "abs": sp.Abs, "sqrt": sp.sqrt, "asin": sp.asin, "acos": sp.acos, "atan": sp.atan, "sec": sp.sec, "csc": sp.csc, "cot": sp.cot, "logb": lambda v, b: sp.log(v, b)}

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

TOK = re.compile(r"(?:sqrt|ln|abs|asin|acos|atan|logb|sec|csc|cot|sin|cos|tan|root|pi|e|i|X|Y|N)|\d+(?:\.\d+)?|[-+*/^()_,]")
ATOM = r"(?:[\w.]+(?:\((?:[^()]|\([^()]*\))*\))?|\((?:[^()]|\([^()]*\))*\))"

def to_sympy(s):
    # outputs run letters together (Xpi, 3rootX, pie), so tokenize first
    toks = TOK.findall(s)
    if "".join(toks) != s:
        raise ValueError("untokenized")
    out = ""
    for t in toks:
        if t in ("sqrt", "ln", "abs", "asin", "acos", "atan", "logb", "sec", "csc", "cot", "sin", "cos", "tan"): out += t
        elif t == "_": out += "-"
        elif t == "root": out += " root "
        else: out += " " + t + " "
    out = re.sub(r"(sqrt|ln|abs|asin|acos|atan|logb|sec|csc|cot|sin|cos|tan)\s*", r"\1", out)
    out = re.sub("(" + ATOM + r")\s*root\s*(" + ATOM + ")", r"rootf(\1,\2)", out)
    return parse_expr(out, local_dict=names, transformations=tr)

def gen_int():
    # the integrals a calc 1 class has: x^k times something, trig powers, 1/(quadratic), sums of those
    k, a, b = random.randint(1, 3), random.randint(1, 4), random.randint(-3, 3) or 1
    return random.choice([
        f"X^{k}*e^X", f"X^{k}*sin(X)", f"X^{k}*cos({a}X)", f"X*ln(X)", f"X^{k}*ln(X)", "atan(X)", f"{a}X*e^({b}X)",
        f"sin(X)^{k + 1}", f"cos(X)^{k + 1}", f"sin({a}X)^2", f"cos({a}X)^3", f"sin(X)^2*cos(X)", "sin(X)*cos(X)",
        f"1/(X^2_{a * a})", f"1/(X^2+{a + b + 4}X+{a + 1})", f"1/(X^2+{a * a})", f"X/(X^2+{a})", f"({a}X+{b})/(X^2_1)", f"1/(X^3_X)",
        f"sqrt({a}X+{b + 4})", f"{a}X*sqrt(X^2+{a})", f"e^({a}X)*{b}", f"{a}/(X+{b + 4})", f"X^{k}+{a}X+{b}", f"tan(X)", f"sec(X)^2",
        f"X^{k + 2}*e^X", f"e^({a}X)*sin({abs(b)}X)", f"e^({a}X)*cos({abs(b)}X)", f"X*atan({a}X)", f"abs({a}X+{b})", f"ln(X)^{k + 1}", f"X*ln(X)^2",
        f"1/(X^2+{a}X+{a + 2})", f"1/(X^2+X_{a})", f"(2X+{b})/(X^2+X+{a})", f"X^2/(X^2+{a})", f"X^3/(X^2+{a})", f"sqrt({a + 3}_X^2)",
        f"sqrt(X^2+{a})", f"1/sqrt(X^2+{a})", f"1/sqrt({a + 3}_X^2)", f"tan(X)^{k + 1}", f"sec(X)^3", f"sin(X)^2*cos(X)^2", f"asin(X)", f"sec(X)", f"cot(X)^{k + 1}", f"csc(X)^3", f"sec(X)^{k + 2}", f"sin({a}X)^2*cos({a}X)^2", f"sin(X)^2*cos(X)^4", f"tan({a}X)^2",
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
    ])

def lim_check(expr, pt, out):
    f = to_sympy(expr)
    where = {"inf": sp.oo}.get(pt) or sp.Integer(pt)
    sides = ["-"] if pt == "inf" else ["+", "-"]
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

def run(expr):
    if cmd == "limit":
        expr = f"limit({expr[0]},X,{expr[1]})"
    if cmd == "solve":
        p = subprocess.run(["build-pc/fluxion", "solve", expr], capture_output=True, text=True, timeout=20)
        return re.findall(r"^Root: (.*)$", p.stdout, re.M)
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

bad, why, skipped, ok = [], [], 0, 0
for i in range(count):
    expr = gen_int() if cmd == "integral" else gen_eq() if cmd == "solve" else gen_lim() if cmd == "limit" else gen(random.choice([1, 2, 2, 3]))
    if cmd == "limit":
        try:
            out = run(expr)
            problem = lim_check(expr[0], expr[1], out) if out is not None else "skip"
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
            roots = run(expr)
        except subprocess.TimeoutExpired:
            bad.append((expr, "TIMEOUT", None)); continue
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
    else: ok += 1

print(f"{ok} ok, {len(bad)} wrong, {skipped} skipped of {count} (seed {seed}, {mode})")
if "-v" in sys.argv:
    for expr, r in why: print(f"  skipped {expr} -> {r}")
for expr, out, w in bad:
    if cmd in ("solve", "limit"):
        print(f"  {expr}\n    -> {out}\n    {w}"); continue
    print(f"  {expr}\n    -> {out}\n    at X={w[0]:.3f} Y={w[1]:.3f}: want {w[2]:.5g}, got {w[3]}" if w else f"  {expr} -> {out}")
