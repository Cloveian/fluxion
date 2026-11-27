# scores the CAS on a converted problem set (tools/bench_import.py's problems.jsonl): right / wrong / gave up per item, answers checked with sympy
# usage: python3 tools/bench.py problems.jsonl [kind ...] [-v] [-j 8]    (run from the repo root, build-pc/fluxion and fluxion-app have to be built, per-item verdicts go to /tmp/bench-results.jsonl)
import concurrent.futures, contextlib, io, json, multiprocessing, os, random, re, subprocess, sys
import sympy as sp
from sympy.parsing.sympy_parser import parse_expr

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.argv, args = sys.argv[:1] + ["0"], sys.argv[1:]
with contextlib.redirect_stdout(io.StringIO()):
    import fuzz     # its Fluxion text -> sympy, importing it with a count of 0 runs nothing

verbose = "-v" in args
workers = int(args[args.index("-j") + 1]) if "-j" in args else 8
rest = [a for i, a in enumerate(args) if a not in ("-v", "-j") and not (i and args[i - 1] == "-j")]
path, kinds = rest[0], set(rest[1:])

def sym(text):
    # a problem's own sympy string
    return parse_expr(text, transformations=fuzz.tr, local_dict={"E": sp.E, "oo": sp.oo, "pi": sp.pi})

def from_cas(text):
    # what the CAS printed, in lower case letters like the problems use
    e = fuzz.to_sympy(text)
    return e.subs({s: sp.Symbol(s.name.lower()) for s in e.free_symbols})

def cli(command):
    p = subprocess.run(["build-pc/fluxion", "simplify", command], capture_output=True, text=True, timeout=60)
    m = re.search(r"^Output: (.*)$", p.stdout, re.M)
    return m.group(1) if m else None

def numbers(f, var):
    # real sample points of f where it is defined, a few of them
    pts = []
    rnd = random.Random(1)
    for _ in range(40):
        x = rnd.uniform(-3, 3) if rnd.random() < 0.5 else rnd.uniform(0.2, 4)
        try:
            v = complex(f.subs(var, x).evalf())
        except Exception:
            continue
        if abs(v.imag) < 1e-9 and abs(v) < 1e8 and v == v:
            pts.append((x, v.real))
        if len(pts) == 5:
            break
    return pts

def close(a, b):
    return abs(a - b) <= 1e-4 * max(1, abs(a))

def gave_up(out):
    return out is None or re.match(r"(integral|limit|deriv|solve|unknown)\(?", out) is not None or out in ("", "unknown")

def limit_ends(chk):
    # the limit from each side that was asked for, sympy's, both are the same when it exists
    var, f, to = sp.Symbol(chk.get("var", "x")), sym(chk["expr"]), sym(chk["to"])
    return [sp.limit(f, var, to, s) for s in ([chk["side"]] if chk.get("side") else ["+", "-"])]

fuzz.TOK = fuzz.make_tok("[A-Z]")    # any capital letter is a variable, the problems use a b t u

def values_agree(a, b):
    # the same function of whatever letters they have, at a few whole number points
    letters = sorted(a.free_symbols | b.free_symbols, key=str)
    rnd = random.Random(2)
    compared = 0
    for _ in range(4):
        at = {s: rnd.randint(2, 9) for s in letters}
        try:
            x, y = complex(a.subs(at).evalf()), complex(b.subs(at).evalf())
        except (TypeError, ValueError):
            continue
        if abs(x - y) > 1e-6 * max(1, abs(x)):
            return False
        compared += 1
    if compared == 0:
        raise ValueError("no point where both have a value")    # an unscored item, not a right one
    return True

def app_lines(command):
    # (answer lines, None), or (None, why) when the app gave up
    lines = fuzz.app_answers(fuzz.app_text(command))
    if not lines:
        return None, "no output"
    if lines[0][0] == "message":
        return None, lines[0][1]
    return lines, None

def sum_score(command, chk):
    lines, why = app_lines(command)
    if lines is None:
        return "gaveup", why
    got, want = lines[0][1], sym(chk["expr"]).doit()
    return ("right", got) if values_agree(from_cas(got), want) else ("wrong", f"{got}, want {want}")

def solve_score(command, chk):
    # every root the app gives has to make the equation true, a numeric one close enough
    lines, why = app_lines(command)
    if lines is None:
        return "gaveup", why
    m = re.match(r"(?i)^[A-Z]\s*=\s*(.*?)(?:\s*\(numeric\))?$", lines[0][1])
    if not m:
        return "unscored", f"can't read {lines[0][1]}"
    eq, var = sym(chk["eqs"][0]), sp.Symbol(chk["vars"][0])
    for root in fuzz.top_split(m.group(1)):
        r = from_cas(root)
        gap = abs(complex((eq.lhs - eq.rhs).subs(var, r).evalf()))
        if gap > (1e-3 if chk.get("numeric") else 1e-6) * (1 + abs(complex(r.evalf()))) ** 3:
            return "wrong", f"{root} leaves {gap:.3g} in {chk['eqs'][0]}"
    return "right", lines[0][1]

def concave_score(command, chk):
    # the command is increasing(f',x): where f' goes up is where f is concave up
    lines, why = app_lines(command)
    if lines is None:
        return "gaveup", why
    problem = fuzz.increasing_check(sp.diff(sym(chk["expr"]).subs(sp.Symbol(chk.get("var", "x")), fuzz.X), fuzz.X), lines)
    text = " | ".join(t for _, t in lines)
    if problem == "skip":
        return "unscored", f"fuzz check skipped it: {text}"
    return ("wrong", f"{text}: {problem}") if problem else ("right", text)

def volume_score(command, chk):
    # the V = line against the book's value
    lines, why = app_lines(command)
    if lines is None:
        return "gaveup", why
    m = re.match(r"(?i)^v\s*=(.*)$", lines[-1][1])
    if len(lines) < 2 or not m:
        return "gaveup", " | ".join(t for _, t in lines)
    got, want = fuzz.fval(fuzz.to_sympy(m.group(1)), 0), float(sym(chk["want"]).evalf())
    return ("right", lines[-1][1]) if got is not None and not fuzz.far(got, want, 1e-6) else ("wrong", f"{lines[-1][1]}, want {want:.8g}")

APP_KINDS = {"tangent", "inflect", "increasing", "extrema", "implicit", "critical", "average", "mvt", "area"}

def app_score(command):
    # the commands that live in the app, run through it and chekced with fuzz.py's own checks
    name = command.split("(")[0]
    args = fuzz.top_split(command[len(name) + 1:-1])
    lines, why = app_lines(command)
    if lines is None:
        return "gaveup", why
    f = fuzz.to_sympy(args[0]) if name != "implicit" else None
    num = lambda s: int(fuzz.to_sympy(s)) if fuzz.to_sympy(s).is_Integer else float(fuzz.to_sympy(s))
    if name == "tangent":
        problem = fuzz.tangent_check(f, num(args[1]), lines)
    elif name == "increasing":
        problem = fuzz.increasing_check(f, lines)
    elif name in ("inflect", "extrema"):
        problem = fuzz.turning_check(f, lines, name == "inflect")
    elif name == "implicit":
        problem = fuzz.implicit_check(args[0], lines)
    elif name == "average":
        problem = fuzz.average_check(f, num(args[1]), num(args[2]), lines)
    elif name == "mvt":
        problem = fuzz.mvt_check(f, num(args[1]), num(args[2]), lines)
    elif name == "area":
        bounds = (num(args[2]), num(args[3])) if len(args) > 3 else None
        problem = fuzz.area_check(f, fuzz.to_sympy(args[1]), bounds, lines)
    elif name == "critical":
        roots = [r for r in re.sub(r"^[A-Za-z]=", "", lines[0][1]).split(",") if r and r != "none"]
        problem = fuzz.critical_check(args[0], roots)
    else:
        return "unscored", f"no checker for {name}"
    text = " | ".join(t for _, t in lines)
    if problem and re.search(r"None", str(problem)):    # fuzz's reference can't evaluate it (a fractional power at a negative point), not the CAS
        return "unscored", f"reference undefined: {text}"
    if problem == "skip":
        return "unscored", f"fuzz check skipped it: {text}"
    return ("wrong", f"{text}: {problem}") if problem else ("right", text)

def score(item):
    # (verdict, detail). verdict: right wrong gaveup timeout unscored
    random.seed(item["id"])     # fuzz.py's checks pick random points, the same ones every run
    chk, cmds = item.get("check") or {}, item.get("fluxion")
    if not item.get("convertible") or not cmds or chk.get("type") in (None, "none"):
        return "unscored", item.get("reason") or "no check"
    kind = chk["type"]
    try:
        if cmds[0].startswith(("sum(", "product(")):
            return sum_score(cmds[0], chk)
        if kind == "solve":
            return solve_score(cmds[0], chk)
        if kind == "concave":
            return concave_score(cmds[0], chk)
        if kind in ("derivative", "derivative_at", "limit", "integral", "definite", "evaluate"):
            out = cli(re.sub(r"^eval\((.*)\)$", r"\1", cmds[0]))    # eval( is the app's word for a bare expression
            var = sp.Symbol(chk.get("var", "x"))
            if gave_up(out):
                if kind == "limit" and limit_ends(chk)[0] != limit_ends(chk)[-1]:
                    return "gaveup", f"DNE, the CAS has no way to say it ({out})"
                return "gaveup", out or "no output"
            if kind == "limit" and out == "dne":
                ends = limit_ends(chk)
                none = ends[0] != ends[-1] or any(e.has(sp.AccumBounds) for e in ends)   # the sides *dih*fer, or it keeps going round
                return ("right", out) if none else ("wrong", f"dne but the limit is {ends[0]}")
            got = sp.oo if out == "inf" else -sp.oo if out == "-inf" else from_cas(out)
            order = chk.get("order", 1)
            if kind in ("derivative", "integral"):
                # a *dih*rivative is compared with sympy's, an answer to an integral has to differentiate back to the integrand (the constant doesn't show)
                want, have = (sp.diff(sym(chk["expr"]), var, order), got) if kind == "derivative" else (sym(chk["expr"]), sp.diff(got, var, order))
                bad = []
                for x, w in numbers(want, var):
                    try:
                        g = complex(have.subs(var, x).evalf())
                    except (TypeError, ValueError):
                        continue    # not a number here (a complex or undefined end), nothing to compare
                    if abs(g.imag) > 1e-9 or not close(w, g.real):
                        bad.append((x, w, g))
                return ("wrong", f"{out} at {bad[0]}") if bad else ("right", out)
            if kind == "limit":
                ends = limit_ends(chk)
                if ends[0] != ends[-1]:
                    return "wrong", f"limit doesn't exist ({ends[0]} and {ends[-1]}) but it said {out}"
                want = ends[0]
                if want in (sp.oo, -sp.oo):
                    return ("right", out) if got == want else ("wrong", f"{out}, want {want}")
                return ("right", out) if close(float(want), float(got.evalf())) else ("wrong", f"{out}, want {want}")
            if kind == "definite":
                want = float(sp.Integral(sym(chk["expr"]), (var, sym(chk["a"]), sym(chk["b"]))).evalf())
            elif kind == "evaluate":
                want = float(sym(chk["expr"]).evalf())
            else:
                want = float(sp.diff(sym(chk["expr"]), var, order).subs(var, sym(chk["at"])).evalf())
            return ("right", out) if close(want, float(got.evalf())) else ("wrong", f"{out}, want {want}")
        if kind == "volume":
            return volume_score(cmds[0], chk)
        if kind in APP_KINDS:
            return app_score(cmds[0])
        if kind == "simplify":
            out = cli(cmds[0])
            if out is None:
                return "gaveup", "no output"
            return ("right", out) if values_agree(from_cas(out), sym(chk["expr"])) else ("wrong", f"{out}, want {sym(chk['expr'])}")
        return "unscored", f"no checker for {kind} yet"
    except subprocess.TimeoutExpired:
        return "timeout", cmds[0]
    except Exception as ex:
        return "unscored", f"checker: {ex}"

items = [json.loads(line) for line in open(path)]
hand = os.path.join(os.path.dirname(path), "hand.jsonl")     # items written by hand, replacing the importer's for the same id
if os.path.exists(hand):
    mine = {i["id"]: i for i in map(json.loads, open(hand))}
    items = [mine.pop(i["id"], i) for i in items] + list(mine.values())
items = [i for i in items if not kinds or i["kind"] in kinds]
with concurrent.futures.ProcessPoolExecutor(workers, mp_context=multiprocessing.get_context("fork")) as pool:  # processes, so every item has its own random state
    results = list(pool.map(score, items))

table = {}
for item, (verdict, detail) in zip(items, results):
    row = table.setdefault(item["kind"], {})
    row[verdict] = row.get(verdict, 0) + 1
print(f"{'kind':16}{'right':>7}{'wrong':>7}{'gave up':>9}{'timeout':>9}{'unscored':>10}")
for kind, row in sorted(table.items()):
    print(f"{kind:16}{row.get('right', 0):7}{row.get('wrong', 0):7}{row.get('gaveup', 0):9}{row.get('timeout', 0):9}{row.get('unscored', 0):10}")
for item, (verdict, detail) in zip(items, results):
    if verdict == "wrong" or (verbose and verdict in ("gaveup", "timeout")):
        print(f"{verdict:8} {item['id']:14} {(item.get('fluxion') or [''])[0]}\n           {detail}")
with open("/tmp/bench-results.jsonl", "w") as out:
    for item, (verdict, detail) in zip(items, results):
        out.write(json.dumps({"id": item["id"], "kind": item["kind"], "verdict": verdict, "detail": detail}) + "\n")
