# random expressions through build-pc/fluxion simplify, checked numerically against sympy
# usage: python3 tools/fuzz.py [count] [seed] [pos|any]   (pos = letters only get positive test points)
import random, re, subprocess, sys
import sympy as sp
from sympy.parsing.sympy_parser import parse_expr, standard_transformations, implicit_multiplication_application, convert_xor

count = int(sys.argv[1]) if len(sys.argv) > 1 else 300
seed = int(sys.argv[2]) if len(sys.argv) > 2 else 1
mode = sys.argv[3] if len(sys.argv) > 3 else "any"
random.seed(seed)

X, Y = sp.symbols("X Y")
tr = standard_transformations + (implicit_multiplication_application, convert_xor)
rootf = lambda n, v: v ** (1 / sp.sympify(n))
names = {"rootf": rootf, "X": X, "Y": Y, "e": sp.E, "ln": sp.log, "abs": sp.Abs, "sqrt": sp.sqrt}

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

TOK = re.compile(r"(?:sqrt|ln|abs|sin|cos|tan|root|pi|e|X|Y)|\d+(?:\.\d+)?|[-+*/^()_]")
ATOM = r"(?:[\w.]+(?:\((?:[^()]|\([^()]*\))*\))?|\((?:[^()]|\([^()]*\))*\))"

def to_sympy(s):
    # outputs run letters together (Xpi, 3rootX, pie), so tokenize first
    toks = TOK.findall(s)
    if "".join(toks) != s:
        raise ValueError("untokenized")
    out = ""
    for t in toks:
        if t in ("sqrt", "ln", "abs", "sin", "cos", "tan"): out += t
        elif t == "_": out += "-"
        elif t == "root": out += " root "
        else: out += " " + t + " "
    out = re.sub(r"(sqrt|ln|abs|sin|cos|tan)\s*", r"\1", out)
    out = re.sub("(" + ATOM + r")\s*root\s*(" + ATOM + ")", r"rootf(\1,\2)", out)
    return parse_expr(out, local_dict=names, transformations=tr)

def run(expr):
    p = subprocess.run(["build-pc/fluxion", "simplify", expr], capture_output=True, text=True, timeout=20)
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
    expr = gen(random.choice([1, 2, 2, 3]))
    try:
        out = run(expr)
    except subprocess.TimeoutExpired:
        bad.append((expr, "TIMEOUT", None)); continue
    if out is None:
        skipped += 1; why.append((expr, "no output")); continue  # oom or parse failure
    try:
        fi, fo = to_sympy(expr), to_sympy(out)
    except Exception as ex:
        skipped += 1; why.append((expr, f"{out} ({ex})")); continue
    worst = None
    for px, py in points():
        a, b = val(fi, px, py), val(fo, px, py)
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
    print(f"  {expr}\n    -> {out}\n    at X={w[0]:.3f} Y={w[1]:.3f}: want {w[2]:.5g}, got {w[3]}" if w else f"  {expr} -> {out}")
