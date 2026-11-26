# turns the calc 1 webassign export (markdown) into a jsonl of checkable items, one per answer blank
# usage: python3 tools/bench_import.py [all-problems.md] [problems.jsonl]   (defaults ~/Downloads/all-problems.md and ~/Downloads/calc1-problems/problems.jsonl)
# latex it can't parse cleanly becomes convertible:false with a "latex: ..." reason, never a guess. key_mismatch only compares the answer key with sympy's own answer
import json, os, random, re, signal, sys
import sympy as sp
from sympy.parsing.sympy_parser import parse_expr, standard_transformations, implicit_multiplication_application, convert_xor

SP_TR = standard_transformations + (implicit_multiplication_application, convert_xor)

TexError = ValueError

# ---- reading the markdown ----

def read_problems(path):
    lines = open(path, encoding="utf-8").read().split("\n")
    probs, sec, mode = [], None, "body"
    for i, l in enumerate(lines):
        if l.startswith("## "):
            sec = l[3:].strip()
            continue
        m = re.match(r"### (\d+)\. \((.*?)\)(?: · (\S+) pts?)?\s*$", l)
        if m:
            probs.append(dict(n=int(m.group(1)), id=m.group(2), pts=m.group(3), section=sec, line=i + 1, body=[], key=[]))
            mode = "body"
            continue
        if not probs:
            continue
        if l.startswith("> [!note]"):
            mode = "note"
        elif l.startswith("> [!success]"):
            mode = "key"
        elif mode == "key" and l.startswith(">"):
            probs[-1]["key"].append(l[1:].strip())
        elif mode == "body":
            probs[-1]["body"].append(l)
    return probs

def key_entries(raw):
    # answer key lines -> numbered {n: text}, choices [text], lettered {c: [text]}, plain [text], wrong_n (numbers marked wrong in the key)
    num, choice, letter, plain, wrong_n = {}, [], {}, [], set()
    for t in raw:
        if t.startswith("- "):
            t = t[2:]
        t = t.strip()
        if not t or t.startswith(">") or t.startswith("!["):
            continue
        if "✗" in t:
            mm = re.match(r"^\((\d+)\)", t)
            if mm:
                wrong_n.add(int(mm.group(1)))
        t = re.sub(r"\s*(✓|✗)?\s*\*\((answer key|marked wrong)\)\*", "", t)
        t = t.replace(" ✓", "").replace(" ✗", "").strip()
        m = re.match(r"^\((\d+)\)\s*(.*)$", t)
        m2 = re.match(r"^\*{0,2}\(([a-z])\)\*{0,2}\s*(.*)$", t)
        if m:
            num[int(m.group(1))] = m.group(2)
        elif t.startswith("choice:"):
            choice.append(t[7:].strip())
        elif m2:
            letter.setdefault(m2.group(1), []).append(m2.group(2))
        else:
            plain.append(t)
    return dict(num=num, choice=choice, letter=letter, plain=plain, wrong_n=wrong_n)

# body -> segments: {"k": "t" text | "m" math | "b" blank | "fig" | "mc" choices | "tbl", ...}

BLANK = re.compile(r"\\underline\{(?:\\qquad|\\quad\\text\{\((\d+)\)\}\\quad)\}")

def split_blanks(s, display):
    # a line with blanks in it -> segments, keeping the $ balanced when a blank sits inside inline math
    out, pos, inmath = [], 0, False
    def push(txt):
        if txt.replace("$", "").strip():
            out.append(dict(k="m", s=txt.strip().strip("$"), disp=True) if display else dict(k="t", s=txt.strip()))
    for m in BLANK.finditer(s):
        frag = s[pos:m.start()]
        odd = frag.count("$") % 2 == 1
        if odd != inmath:
            inmath = odd
        if inmath and not display:
            frag += "$"
        push(frag)
        out.append(dict(k="b", n=int(m.group(1)) if m.group(1) else None))
        pos = m.end()
        if inmath and not display:
            s = s[:pos] + "$" + s[pos:]
            inmath = False
    push(s[pos:])
    return out

def to_segments(lines):
    segs, i = [], 0
    while i < len(lines):
        l = lines[i].strip()
        i += 1
        if not l or l == "---" or l.startswith(">") or l.startswith("## "):
            continue
        if l.startswith("![") or l.startswith("- !["):
            segs.append(dict(k="fig"))
            continue
        if re.match(r"^\*\*(Given )?Figure", l):
            segs.append(dict(k="fig"))
            continue
        if l == "$$":
            buf = []
            while i < len(lines) and lines[i].strip() != "$$" and not lines[i].strip().startswith("$$"):
                buf.append(lines[i])
                i += 1
            if i < len(lines):
                i += 1
            segs.extend(split_blanks("\n".join(buf), True))
            continue
        if l.startswith("$$") and l.endswith("$$") and len(l) > 4:
            segs.extend(split_blanks(l[2:-2], True))
            continue
        if l.startswith("|"):
            if not re.match(r"^\|[\s\-|]*$", l):
                segs.append(dict(k="tbl"))
            if "\\underline" in l:
                for sg in split_blanks(l.replace("|", " "), False):
                    sg["tb"] = True
                    segs.append(sg)
            continue
        bul = False
        if l.startswith("- "):
            bul, l = True, l[2:]
        for sg in split_blanks(l, False):
            if sg["k"] == "t":
                sg["bul"] = bul
            segs.append(sg)
    # bullets that aren't labels for a blank are answer choices, a run of them is one multiple choice
    out = []
    for j, sg in enumerate(segs):
        nxt = segs[j + 1] if j + 1 < len(segs) else None
        nxt2 = segs[j + 2] if j + 2 < len(segs) else None
        is_label = (nxt and nxt["k"] == "b") or (nxt and nxt2 and nxt["k"] in ("t", "m") and re.search(r"=\s*\$?\s*$", nxt["s"]) and nxt2["k"] == "b")
        if sg["k"] == "t" and sg.get("bul") and not is_label:
            if out and out[-1]["k"] == "mc":
                out[-1]["opts"].append(sg["s"])
            else:
                out.append(dict(k="mc", opts=[sg["s"]]))
        else:
            out.append(sg)
    return out

def split_parts(segs):
    # parts are cut at a standalone (a) (b) ... label, (i) only counts as a part after (h) and so on
    parts, cur, last = [], dict(label=None, segs=[]), None
    for sg in segs:
        m = re.match(r"^\(([a-z])\)\s*(.*)$", sg["s"]) if sg["k"] == "t" else None
        if m:
            c = m.group(1)
            ok = c not in "ivx" or (last is not None and ord(c) == ord(last) + 1)
            if ok and (last is None or ord(c) == ord(last) + 1) and (last is not None or c == "a"):
                parts.append(cur)
                cur, last = dict(label=c, segs=[]), c
                if m.group(2):
                    cur["segs"].append(dict(k="t", s=m.group(2), bul=False))
                continue
        cur["segs"].append(sg)
    parts.append(cur)
    return [p for p in parts if p["segs"] or p["label"]]

# --- latex -> tokens -> tree

FUNCS = {"sin": "sin", "cos": "cos", "tan": "tan", "cot": "cot", "sec": "sec", "csc": "csc", "sinh": "sinh", "cosh": "cosh", "tanh": "tanh",
         "ln": "ln", "log": "log10", "arcsin": "asin", "arccos": "acos", "arctan": "atan", "arccot": "acot", "arcsec": "asec", "arccsc": "acsc", "exp": "exp"}
TRIG = {"sin", "cos", "tan", "cot", "sec", "csc", "sinh", "cosh", "tanh"}
GREEK = {"theta", "alpha", "beta", "phi", "omega", "lambda", "mu", "rho", "sigma", "tau", "gamma"}
FUNLETTERS = set("fghFGHpq")

def clean_tex(s):
    s = s.strip().strip("$").strip()
    s = re.sub(r"[.,;\s]+$", "", s)
    s = re.sub(r"(?<![\d.])(\d{1,3}(?:,\d{3})+)(?![\d,])(?!\s*[)\]])", lambda m: m.group(1).replace(",", ""), s)
    s = s.replace("−", "-").replace("∞", "\\infty ").replace("π", "\\pi ").replace("⁄", "/").replace("·", "\\cdot ").replace("×", "\\times ")
    s = s.replace("𝜃", "\\theta ").replace("𝜋", "\\pi ").replace("𝜀", "\\epsilon ").replace("𝛿", "\\delta ").replace("‴", "'''").replace("′", "'")
    s = re.sub(r"\\left\.(.*?)\\right\|", r"|\1|", s)
    s = re.sub(r"\\(left|right)\s*([()\[\]|])", r"\2", s)
    s = re.sub(r"\\(left|right)\\([{}])", r"\\\1\2", s)
    s = s.replace("\\dfrac", "\\frac").replace("\\tfrac", "\\frac").replace("\\rightarrow", "\\to").replace("\\Bigg", "").replace("\\bigg", "").replace("\\big", "")
    s = re.sub(r"\\(displaystyle|quad|qquad)", " ", s)
    s = re.sub(r"\\[,;:! ]", " ", s)
    s = s.replace("~", " ")
    s = re.sub(r"\\(mathbf|boldsymbol|textbf)\s*\{", "{", s)
    return s

TOK = re.compile(r"\s*(\\[A-Za-z]+|\\.|\d*\.\d+|\d+|[A-Za-z]|[{}()\[\]|^_+\-*/=,!<>'.:;&])")

def lex(s):
    s = clean_tex(s)
    toks, pos = [], 0
    while pos < len(s):
        if s[pos:].strip() == "":
            break
        m = TOK.match(s, pos)
        if not m:
            raise TexError("char %r" % s[pos:pos + 6].strip())
        toks.append(m.group(1))
        pos = m.end()
    return toks

def is_num(t):
    return t is not None and re.match(r"^\d*\.?\d+$", t) is not None

def is_letter(t):
    return t is not None and re.match(r"^[A-Za-z]$", t) is not None

def new_state(toks):
    return dict(t=toks, i=0, absd=0, dvar=None)

def p_peek(st, k=0):
    return st["t"][st["i"] + k] if st["i"] + k < len(st["t"]) else None

def p_next(st):
    if st["i"] >= len(st["t"]):
        raise TexError("unexpected end")
    st["i"] += 1
    return st["t"][st["i"] - 1]

def p_expect(st, x):
    got = p_next(st)
    if got != x:
        raise TexError("expected %s got %s" % (x, got))

def p_done(st):
    if st["i"] < len(st["t"]):
        raise TexError("left over %s" % " ".join(st["t"][st["i"]:st["i"] + 4]))

def p_expr(st):
    if p_peek(st) == "-":
        p_next(st)
        left = ("neg", p_term(st))
    else:
        if p_peek(st) == "+":
            p_next(st)
        left = p_term(st)
    while p_peek(st) in ("+", "-"):
        op = p_next(st)
        left = ("bin", op, left, p_term(st))
    return left

def p_starts_factor(st, t):
    if t is None:
        return False
    if is_num(t) or is_letter(t) or t in ("(", "[", "{"):
        return True
    if t == "|":
        return st["absd"] == 0
    if t.startswith("\\"):
        n = t[1:]
        return n in FUNCS or n in GREEK or n in ("pi", "frac", "sqrt", "infty", "lim", "int", "sum")
    return False

def p_term(st):
    left = p_unary(st)
    while True:
        t = p_peek(st)
        if t in ("*", "\\cdot", "\\times"):
            p_next(st)
            left = ("bin", "*", left, p_unary(st))
        elif t == "/":
            p_next(st)
            left = ("bin", "/", left, p_unary(st))
        elif p_starts_factor(st, t):
            left = ("bin", "*", left, p_power(st))
        else:
            return left

def p_unary(st):
    if p_peek(st) == "-":
        p_next(st)
        return ("neg", p_unary(st))
    if p_peek(st) == "+":
        p_next(st)
        return p_unary(st)
    return p_power(st)

def p_power(st):
    base = p_atom(st)
    if p_peek(st) == "^":
        p_next(st)
        base = ("bin", "^", base, p_exp_arg(st))
    if p_peek(st) == "_":
        raise TexError("subscript")
    if p_peek(st) == "!":
        raise TexError("factorial")
    if p_peek(st) == "'":
        raise TexError("prime")
    return base

def p_exp_arg(st):
    t = p_peek(st)
    if t == "{":
        p_next(st)
        e = p_expr(st)
        p_expect(st, "}")
        return e
    if t == "-":
        p_next(st)
        return ("neg", p_exp_arg(st))
    if is_num(t):
        return p_single(st)
    return p_atom(st)

def p_single(st):
    # one unbraced digit or letter, like the ^2 in x^23 or the 1 4 in \frac14
    t = p_peek(st)
    if is_num(t) and len(t) > 1 and "." not in t:
        st["t"][st["i"]] = t[1:]
        return ("num", t[0])
    st["i"] += 1
    if is_num(t):
        return ("num", t)
    st["i"] -= 1
    return p_atom(st)

def p_arg(st):
    # the argument of \frac or \sqrt, a group or one character
    if p_peek(st) == "{":
        p_next(st)
        if p_peek(st) == "d" and is_letter(p_peek(st, 1)) and p_peek(st, 2) == "}":
            st["dvar"] = p_peek(st, 1)
            st["i"] += 3
            return ("num", "1")
        e = p_expr(st)
        p_expect(st, "}")
        return e
    return p_single(st)

def p_group_tokens(st):
    # raw tokens inside the next {...}, for the limit / sum subscripts
    p_expect(st, "{")
    depth, out = 1, []
    while True:
        t = p_next(st)
        depth += (t == "{") - (t == "}")
        if depth == 0:
            return out
        out.append(t)

def p_script(st):
    # a ^ or _ argument as tokens: {group} or a single token
    if p_peek(st) == "{":
        return p_group_tokens(st)
    t = p_next(st)
    return [t]

def p_atom(st):
    t = p_next(st)
    if is_num(t):
        return ("num", t)
    if t in ("(", "["):
        e = p_expr(st)
        p_expect(st, ")" if t == "(" else "]")
        return e
    if t == "{":
        e = p_expr(st)
        p_expect(st, "}")
        return e
    if t == "|":
        st["absd"] += 1
        e = p_expr(st)
        p_expect(st, "|")
        st["absd"] -= 1
        return ("fn", "abs", e, None)
    if is_letter(t):
        if t == "e":
            return ("const", "e")
        if t in FUNLETTERS and p_peek(st) == "(":
            raise TexError("unknown function %s(...)" % t)
        return ("var", t)
    if not t.startswith("\\"):
        raise TexError("token %s" % t)
    n = t[1:]
    if n == "pi":
        return ("const", "pi")
    if n == "infty":
        return ("const", "inf")
    if n in GREEK:
        return ("var", n)
    if n == "frac":
        a = p_arg(st)
        b = p_arg(st)
        return ("bin", "/", a, b)
    if n == "sqrt":
        idx = None
        if p_peek(st) == "[":
            p_next(st)
            idx = p_expr(st)
            p_expect(st, "]")
        a = p_arg(st)
        return ("sqrt", a) if idx is None else ("root", idx, a)
    if n in FUNCS:
        return p_function(st, n)
    if n == "lim":
        return p_limit(st)
    if n == "int":
        return p_integral(st)
    if n == "sum":
        return p_summation(st)
    raise TexError("command %s" % t)

def p_function(st, n):
    name, power, base = FUNCS[n], None, None
    while p_peek(st) in ("^", "_"):
        which = p_next(st)
        e = p_full(new_state(p_script(st)))
        if which == "^":
            power = e
        else:
            base = e
    if p_peek(st) == "(":
        p_next(st)
        arg = p_expr(st)
        p_expect(st, ")")
    elif p_peek(st) == "[":
        raise TexError("bracket argument")
    else:
        arg = p_func_arg(st)
    if base is not None:
        if name != "log10":
            raise TexError("subscript on %s" % n)
        node = ("fn", "logb", arg, base)
    else:
        node = ("fn", name, arg, None)
    if power is not None:
        if power == ("neg", ("num", "1")):
            if n not in TRIG:
                raise TexError("inverse %s" % n)
            node = ("fn", "a" + name, arg, None)
        else:
            node = ("bin", "^", node, power)
    return node

def p_func_arg(st):
    # \sin 2x, \ln x^2, \tan\theta: a run of simple factors, stopping at the next function
    fs = []
    while True:
        t = p_peek(st)
        if t is None:
            break
        if t == "|" and not fs and st["absd"] == 0:
            fs.append(p_power(st))
            continue
        simple = is_num(t) or is_letter(t) or t in ("{",) or (t.startswith("\\") and (t[1:] in GREEK or t[1:] in ("pi", "frac", "sqrt")))
        if not simple or (fs and t == "{"):
            break
        fs.append(p_power(st))
    if not fs:
        raise TexError("function without an argument")
    out = fs[0]
    for f in fs[1:]:
        out = ("bin", "*", out, f)
    return out

def p_full(st):
    e = p_expr(st)
    p_done(st)
    return e

def p_limit(st):
    if p_next(st) != "_":
        raise TexError("limit without subscript")
    spec = p_script(st)
    if "\\to" not in spec:
        raise TexError("limit subscript")
    k = spec.index("\\to")
    var, pt, side = spec[:k], spec[k + 1:], None
    if len(var) != 1 or not (is_letter(var[0]) or var[0][1:] in GREEK):
        raise TexError("limit variable")
    if len(pt) >= 3 and pt[-3:] in (["^", "+"], ["^", "-"]) or (len(pt) >= 4 and pt[-4] == "^" and pt[-3] == "{" and pt[-1] == "}"):
        if pt[-1] == "}":
            side, pt = pt[-2], pt[:-4]
        else:
            side, pt = pt[-1], pt[:-2]
    elif len(pt) >= 2 and pt[-2:] in (["^", "+"], ["^", "-"]):
        side, pt = pt[-1], pt[:-2]
    if pt and pt[-1] in ("+", "-") and len(pt) > 2 and pt[-2] == "^":
        side, pt = pt[-1], pt[:-2]
    body = p_term(st)
    return ("lim", var[0] if is_letter(var[0]) else var[0][1:], p_full(new_state(pt)), side, body)

def p_integral(st):
    lo = hi = None
    while p_peek(st) in ("_", "^"):
        which = p_next(st)
        e = p_full(new_state(p_script(st)))
        if which == "_":
            lo = e
        else:
            hi = e
    # the integrand runs up to a d + letter at depth 0
    depth, j = 0, st["i"]
    end = None
    while j < len(st["t"]):
        t = st["t"][j]
        depth += (t in ("{", "(", "[")) - (t in ("}", ")", "]"))
        if depth == 0 and t == "d" and j + 1 < len(st["t"]) and (is_letter(st["t"][j + 1]) or st["t"][j + 1][1:] in GREEK) and j > st["i"]:
            nxt = st["t"][j + 2] if j + 2 < len(st["t"]) else None
            if nxt is None or nxt in (")", "+", "-", ",", "]", "}"):
                end = j
                break
        j += 1
    if end is None:
        sub = new_state(st["t"][st["i"]:])
        body = p_expr(sub)
        p_done(sub)
        st["i"] = len(st["t"])
        var = sub["dvar"]
        if var is None:
            raise TexError("integral without dx")
    else:
        sub = new_state(st["t"][st["i"]:end])
        body = p_full(sub)
        v = st["t"][end + 1]
        var = v if is_letter(v) else v[1:]
        st["i"] = end + 2
    return ("int", var, lo, hi, body)

def p_summation(st):
    if p_next(st) != "_":
        raise TexError("sum without subscript")
    spec = p_script(st)
    if len(spec) < 3 or not is_letter(spec[0]) or spec[1] != "=":
        raise TexError("sum subscript")
    lo = p_full(new_state(spec[2:]))
    if p_next(st) != "^":
        raise TexError("sum without upper limit")
    hi = p_full(new_state(p_script(st)))
    return ("sum", spec[0], lo, hi, p_term(st))


def parse(tex):
    toks = lex(tex)
    if not toks:
        raise TexError("empty")
    return p_full(new_state(toks))

def split_depth(toks, sep):
    # split a token list at sep tokens that sit outside every bracket
    out, cur, depth = [], [], 0
    for t in toks:
        if depth == 0 and t == sep:
            out.append(cur)
            cur = []
            continue
        depth += (t in ("{", "(", "[")) - (t in ("}", ")", "]"))
        cur.append(t)
    out.append(cur)
    return out

# tree -> sympy string and engine string. engine syntax is tools/fuzz.py's (capital letters, _ for minus), the app commands are the same spelling, lowercased with - for _ like fuzz.py's app_text

def free_vars(n, acc=None):
    acc = set() if acc is None else acc
    if n[0] == "var":
        acc.add(n[1])
    elif n[0] in ("lim", "int", "sum"):
        for c in n[2:]:
            if isinstance(c, tuple):
                free_vars(c, acc)
        acc.add(n[1])
    else:
        for c in n[1:]:
            if isinstance(c, tuple):
                free_vars(c, acc)
    return acc

def make_vmap(vars_, main=None):
    # single lowercase letters for sympy, capitals for the engine. the main variable becomes x / X when that doesn't clash
    vars_ = sorted(vars_)
    py, eng = {}, {}
    taken = set()
    def put(v, letter):
        if letter in taken:
            raise TexError("variable clash on %s" % letter)
        taken.add(letter)
        py[v] = letter
        eng[v] = letter.upper()
    if main is not None and main in vars_ and (main == "x" or "x" not in vars_) and (main != "e"):
        put(main, "x")
    for v in vars_:
        if v in py:
            continue
        if len(v) == 1:
            put(v, v.lower())
        else:
            for c in "tuvwabcdfghjkmnpqrsz":
                if c not in taken and c not in [w for w in vars_ if len(w) == 1] and c.upper() not in [w for w in vars_ if len(w) == 1]:
                    put(v, c)
                    break
    return py, eng

PY_FN = {"sin": "sin", "cos": "cos", "tan": "tan", "cot": "cot", "sec": "sec", "csc": "csc", "sinh": "sinh", "cosh": "cosh", "tanh": "tanh", "ln": "log",
         "asin": "asin", "acos": "acos", "atan": "atan", "acot": "acot", "asec": "asec", "acsc": "acsc", "asinh": "asinh", "acosh": "acosh", "atanh": "atanh", "exp": "exp", "abs": "Abs"}

def prec(n):
    if n[0] == "bin":
        return {"+": 1, "-": 1, "*": 2, "/": 2, "^": 4}[n[1]]
    if n[0] == "neg":
        return 3
    return 5

def wrap(n, s, need):
    return "(" + s + ")" if prec(n) < need or n[0] == "neg" and need >= 4 else s

def simple_exp(b):
    return b[0] in ("var", "const") or (b[0] == "num" and re.match(r"^\d+$", b[1]) is not None)

def emit(n, vm, eng):
    # one tree -> sympy string (eng False) or engine string (eng True)
    k = n[0]
    em = lambda x: emit(x, vm, eng)
    if k == "num":
        return n[1]
    if k == "var":
        return vm[n[1]].upper() if eng else vm[n[1]]
    if k == "const":
        return {"pi": "pi", "e": "e" if eng else "E", "inf": "inf" if eng else "oo"}[n[1]]
    if k == "neg":
        return "-" + wrap(n[1], em(n[1]), 3)
    if k == "bin":
        op, a, b = n[1], n[2], n[3]
        sa, sb = em(a), em(b)
        if op == "^":
            ex = sb if simple_exp(b) else "(" + sb + ")"
            return wrap(a, sa, 5) + ("^" if eng else "**") + ex
        if op == "/":
            return wrap(a, sa, 2) + "/" + wrap(b, sb, 3)
        if op == "*":
            return wrap(a, sa, 2) + "*" + wrap(b, sb, 3 if b[0] != "neg" else 4)
        if op == "-":
            return wrap(a, sa, 1) + ("_" if eng else "-") + wrap(b, sb, 2 if b[0] != "neg" else 4)
        return wrap(a, sa, 1) + "+" + wrap(b, sb, 1 if b[0] != "neg" else 4)
    if k == "fn":
        name, arg = n[1], n[2]
        if name == "logb":
            return "logb(%s,%s)" % (em(arg), em(n[3])) if eng else "log(%s,%s)" % (em(arg), em(n[3]))
        if name == "log10":
            return "log(%s)" % em(arg) if eng else "log(%s,10)" % em(arg)
        if name == "exp":
            return "e^(%s)" % em(arg) if eng else "exp(%s)" % em(arg)
        return "%s(%s)" % (name if eng else PY_FN[name], em(arg))
    if k == "sqrt":
        return "sqrt(%s)" % em(n[1])
    if k == "root":
        return "(%s)root(%s)" % (em(n[1]), em(n[2])) if eng else "(%s)**(1/(%s))" % (em(n[2]), em(n[1]))
    if k == "lim":
        v = vm[n[1]].upper() if eng else vm[n[1]]
        if eng:
            return "limit(%s,%s,%s%s)" % (em(n[4]), v, em(n[2]), {"+": ",1", "-": ",-1", None: ""}[n[3]])
        return "Limit(%s,%s,%s%s)" % (em(n[4]), v, em(n[2]), {"+": ",'+'", "-": ",'-'", None: ""}[n[3]])
    if k == "int":
        v = vm[n[1]].upper() if eng else vm[n[1]]
        if eng:
            return "integral(%s,%s%s)" % (em(n[4]), v, (",%s,%s" % (em(n[2]), em(n[3]))) if n[2] is not None else "")
        return "Integral(%s,%s)" % (em(n[4]), ("(%s,%s,%s)" % (v, em(n[2]), em(n[3]))) if n[2] is not None else v)
    if k == "sum":
        v = vm[n[1]].upper() if eng else vm[n[1]]
        if eng:
            return "sum(%s,%s,%s,%s)" % (em(n[4]), v, em(n[2]), em(n[3]))
        return "Sum(%s,(%s,%s,%s))" % (em(n[4]), v, em(n[2]), em(n[3]))
    raise TexError("emit %s" % k)

def to_py(n, vm):
    return emit(n, vm, False)

def to_eng(n, vm):
    return emit(n, vm, True)

def subst(n, var, rep):
    # the tree with every free var replaced, for f(3) style asks
    if n[0] == "var":
        return rep if n[1] == var else n
    if n[0] in ("lim", "int", "sum"):
        return n
    return (n[0],) + tuple(subst(c, var, rep) if isinstance(c, tuple) else c for c in n[1:])

# sympy side

def _alarm(sig, frm):
    raise TimeoutError()

def timed(fn, secs=8):
    signal.signal(signal.SIGALRM, _alarm)
    signal.setitimer(signal.ITIMER_REAL, secs)
    try:
        return fn()
    finally:
        signal.setitimer(signal.ITIMER_REAL, 0)

def sp_parse(s):
    return parse_expr(s, transformations=SP_TR)

def settle(e):
    # an unevaluated Sum gets done so it can be compared
    if e.has(sp.Sum):
        e = e.doit()
    return e

def agree(a, b, tol=1e-6, tries=5):
    # do two sympy expressions match at some random positive points: True / False, None when no point is real for both
    syms = sorted(a.free_symbols | b.free_symbols, key=str)
    rnd = random.Random(7)
    ok = bad = 0
    for _ in range(tries * 4):
        sub = {s: sp.Float(rnd.uniform(0.3, 2.5)) for s in syms}
        try:
            va, vb = complex(a.subs(sub).evalf()), complex(b.subs(sub).evalf())
        except Exception:
            continue
        if any(v != v or abs(v) > 1e12 for v in (va, vb)) or abs(va.imag) > 1e-9 or abs(vb.imag) > 1e-9:
            continue
        if abs(va - vb) <= tol * max(1.0, abs(va), abs(vb)):
            ok += 1
        else:
            bad += 1
        if ok + bad >= tries:
            break
    return None if ok + bad == 0 else bad == 0

# --- one problem -> contexts, one per answer blank

def seg_text(sg):
    k = sg["k"]
    if k == "t":
        return sg["s"]
    if k == "m":
        return "$" + sg["s"] + "$"
    if k == "b":
        return "___"
    if k == "mc":
        return " / ".join(sg["opts"])
    return ""

def maths_of(segs):
    out = []
    for sg in segs:
        if sg["k"] == "m":
            out.append(sg["s"])
        elif sg["k"] == "t":
            out += re.findall(r"\$([^$]+)\$", sg["s"])
    return out

def words_of(segs):
    # the prose of some segments, math taken out, lower case
    out = []
    for sg in segs:
        if sg["k"] == "t":
            out.append(re.sub(r"\$[^$]*\$", " ", sg["s"]))
        elif sg["k"] == "mc":
            out.append(" ".join(sg["opts"]))
    return re.sub(r"\s+", " ", " ".join(out)).lower()

def build_asks(prob):
    segs = to_segments(prob["body"])
    parts = split_parts(segs)
    pre = []
    if parts and parts[0]["label"] is None:
        for sg in parts[0]["segs"]:
            if sg["k"] in ("b", "mc"):
                break
            pre.append(sg)
    asks = []
    for p in parts:
        part_pre = []
        for sg in p["segs"]:
            if sg["k"] in ("b", "mc"):
                break
            part_pre.append(sg)
        local, nasks = [], 0
        for sg in p["segs"] + [dict(k="end")]:
            if sg["k"] in ("b", "mc"):
                asks.append(dict(part=p["label"], pre=pre, part_pre=part_pre if p["label"] else [], local=list(local), blank=sg))
                local, nasks = [], nasks + 1
            elif sg["k"] == "end":
                if nasks == 0 and p["label"]:
                    asks.append(dict(part=p["label"], pre=pre, part_pre=part_pre, local=list(local), blank=None))
            else:
                local.append(sg)
    if not asks:
        asks.append(dict(part=None, pre=[], part_pre=[], local=segs, blank=None))
    for a in asks:
        a["segs"] = unique_segs(a["pre"] + a["part_pre"] + a["local"])
    return asks

def unique_segs(segs):
    seen, out = set(), []
    for sg in segs:
        if id(sg) not in seen:
            seen.add(id(sg))
            out.append(sg)
    return out

# the answer key

def final_line(lines):
    # the line that holds the answer: the last with a \mathbf, else the last display math, else the last line
    lines = [l for l in lines if l.strip()]
    if not lines:
        return None
    for l in reversed(lines):
        if "\\mathbf{" in l or "**" in l:
            return l
    for l in reversed(lines):
        if l.startswith("$$"):
            return l
    return lines[-1]

def key_for(ask, ent, single, part_alone):
    # the raw key text for one blank
    b = ask["blank"]
    if b is None:
        return None
    if b["n"] is not None and b["n"] in ent["num"]:
        return ent["num"][b["n"]]
    if single and ent["plain"]:
        return final_line(ent["plain"])
    if single and len(ent["num"]) == 1:
        return list(ent["num"].values())[0]
    if ask["part"] and ask["part"] in ent["letter"] and part_alone:
        return final_line(ent["letter"][ask["part"]])
    return None

def tex_bits(txt):
    # key text -> the math part to convert: last \mathbf{..} content, else the whole thing without the markdown
    t = txt.strip()
    ms = list(re.finditer(r"\\mathbf\{", t))
    if ms:
        i = ms[-1].end()
        depth, j = 1, i
        while j < len(t) and depth:
            depth += (t[j] == "{") - (t[j] == "}")
            j += 1
        if depth == 0:
            t = t[i:j - 1]
    t = re.sub(r"\*\*", "", t).strip()
    t = re.sub(r"^\$+|\$+$", "", t).strip()
    return t

def key_to_expr(txt, strip_c=False, strip_d=False):
    # key text -> (sympy ast or None, note). a list, interval, words or anything with commas is None
    t = tex_bits(txt)
    if re.match(r"^\W*(DNE|does not exist)", t, re.I):
        return "dne"
    if re.match(r"^\W*(indeterminate)", t, re.I):
        return None
    toks = lex(t)
    sides = []
    cur, depth = [], 0
    for tk in toks:
        depth += (tk in ("{", "(", "[")) - (tk in ("}", ")", "]"))
        if depth == 0 and (tk == "=" or tk == "\\approx"):
            sides.append(cur)
            cur = []
        else:
            cur.append(tk)
    sides.append(cur)
    toks = sides[-1]
    if strip_c and len(toks) >= 2 and toks[-2] == "+" and toks[-1] in ("C", "K"):
        toks = toks[:-2]
    if strip_d:
        if len(toks) >= 2 and toks[-2] == "d" and is_letter(toks[-1]):
            toks = toks[:-2]
        elif len(toks) >= 3 and toks[-3] == "d" and toks[-2] == "\\theta":
            toks = toks[:-3]
        elif len(toks) >= 2 and toks[-2] == "d" and toks[-1][1:] in GREEK:
            toks = toks[:-2]
    if not toks:
        raise TexError("empty key")
    return p_full(new_state(toks))

def parse_label(s):
    # what the text right before a blank asks for: ('f', 1, 'x') for f'(x)=, ('d', 1, 'y/x') for dy/dx, None for anything else
    s = clean_tex(s).strip()
    s = re.sub(r"[=\s]+$", "", s).strip()
    s = s.replace("^{\\prime}", "'")
    m = re.match(r"^([A-Za-z])\s*('*)\s*(?:\(([^()]*)\))?$", s)
    if m:
        return (m.group(1), len(m.group(2)), m.group(3))
    m = re.match(r"^\\frac\s*\{\s*d\s*(?:\^\{?(\d)\}?)?\s*([a-z])\s*\}\s*\{\s*d\s*([a-z])\s*(?:\^\{?\d\}?)?\s*\}$", s)
    if m:
        return ("d", int(m.group(1) or 1), m.group(2) + "/" + m.group(3))
    m = re.match(r"^d\s*([a-z])$", s)
    if m:
        return ("dif", 1, m.group(1))
    return None

# --- pieces of math: definitions, equations, points, intervals ---

def skip(reason, kind=None, setup=False):
    # not convertible: a LookupError carries (reason, kind, setup) up to convert_problem
    raise LookupError(reason, kind, setup)

def balanced_outer(toks):
    # does the first token's bracket close at the very last token
    depth = 0
    for i, t in enumerate(toks):
        depth += (t in ("{", "(", "[")) - (t in ("}", ")", "]"))
        if depth == 0 and i < len(toks) - 1:
            return False
    return depth == 0

def head(toks):
    # left side of a definition: y, f(x), f'(x), s
    n = len(toks)
    if n == 0 or not is_letter(toks[0]):
        return None
    i, primes = 1, 0
    while i < n and toks[i] == "'":
        primes += 1
        i += 1
    if i == n:
        return (toks[0], None, primes)
    if n - i == 3 and toks[i] == "(" and toks[-1] == ")" and (is_letter(toks[i + 1]) or toks[i + 1][1:] in GREEK):
        v = toks[i + 1]
        return (toks[0], v if is_letter(v) else v[1:], primes)
    return None

def classify_piece(pc):
    # one comma-separated piece of math -> a list of pieces (a chain like f(t)=a g(t)=b comes out as two definitions)
    if any(t in ("<", ">", "\\le", "\\ge", "\\ne") for t in pc):
        return [dict(t="cond", toks=pc)]
    try:
        if pc[0] == "(" and pc[-1] == ")" and balanced_outer(pc) and len(split_depth(pc[1:-1], "=")) == 2:
            pc = pc[1:-1]
        sides = split_depth(pc, "=")
        if len(sides) == 2 and not sides[1]:
            return [dict(t="label", toks=sides[0])]
        if len(sides) == 3 and head(sides[0]) and len(sides[1]) > 4 and head(sides[1][-4:]):
            return classify_piece(sides[0] + ["="] + sides[1][:-4]) + classify_piece(sides[1][-4:] + ["="] + sides[2])
        if len(sides) == 2:
            h = head(sides[0])
            if h:
                return [dict(t="def", name=h[0], var=h[1], primes=h[2], rhs=p_full(new_state(sides[1])))]
            return [dict(t="eq", lhs=p_full(new_state(sides[0])), rhs=p_full(new_state(sides[1])))]
        if len(sides) > 2:
            return [dict(t="err", msg="chained equation", tex=" ".join(pc))]
        if pc[0] in ("(", "[") and pc[-1] in (")", "]") and balanced_outer(pc):
            parts = split_depth(pc[1:-1], ",")
            if len(parts) == 2:
                return [dict(t="point" if pc[0] == "(" else "interval", a=p_full(new_state(parts[0])), b=p_full(new_state(parts[1])))]
        return [dict(t="expr", ast=p_full(new_state(pc)))]
    except TexError as e:
        return [dict(t="err", msg=str(e), tex=" ".join(pc))]

def collect(maths):
    out = []
    for tex in maths:
        try:
            toks = lex(tex)
        except TexError as e:
            out.append(dict(t="err", msg=str(e), tex=tex))
            continue
        if not toks:
            continue
        if "\\begin" in toks or "\\end" in toks:
            out.append(dict(t="err", msg="piecewise or array definition", tex=tex))
            continue
        for pc in split_depth(toks, ","):
            if pc:
                out += classify_piece(pc)
    return out

def var_of(d):
    # the variable a definition is a function of
    if d["var"]:
        return d["var"]
    vs = free_vars(d["rhs"]) - {d["name"]}
    if len(vs) == 1:
        return list(vs)[0]
    for v in ("x", "t"):
        if v in vs:
            return v
    raise TexError("ambiguous variable")

def fun_defs(info):
    # definitions that are functions (a free variable on the right)
    out = []
    for p in info:
        if p["t"] == "def" and p["primes"] == 0 and ((free_vars(p["rhs"]) - {p["name"]}) or p["var"]):
            out.append(p)
    return out

def val_defs(info):
    out = {}
    for p in info:
        if p["t"] == "def" and p["primes"] == 0 and not (free_vars(p["rhs"])):
            out[p["name"]] = p["rhs"]
    return out

def const_of(ast):
    # a number-only tree is a value
    return not free_vars(ast)

# recognizers. check types: derivative derivative_at limit integral definite solve tangent evaluate simplify none, and the app commands implicit(lhs rhs y x) critical extrema(which) inflect concave(which) increasing(which) mvt average area(f g var a b)
# an order key is repeated derivatives / integrals

SETUP_TOPIC = {"3.9": "related_rates", "4.7": "optimization", "6.2": "volume", "6.3": "volume"}
FIG_WORDS = ["given graph", "graph of", "graphs of", "graph is", "graph shown", "use the graph", "the graph to", "from the graph", "given figure", "shaded region",
             "shown in the figure", "trace or copy", "shown below", "graph of the function", "figure is", "given figures"]

def topic_of(pid):
    m = re.match(r"^(\d)\.xp\.(\d+)\.", pid.lower()) or re.match(r"^(\d)\.(\d+)\.", pid.lower())
    return "%s.%s" % (m.group(1), m.group(2)) if m else None

def vmap_for(rhs_asts, main):
    vs = set()
    for a in rhs_asts:
        vs |= free_vars(a)
    if main:
        vs.add(main)
    return make_vmap(vs, main)

def fun_maps(d, *extra):
    # a definition's variable and its sympy / engine letters, other trees that share them in extra
    var = var_of(d)
    py, eng = vmap_for([d["rhs"], *extra], var)
    return var, py, eng

def res(kind, fluxion, check, cmp=None, vm=None):
    return dict(kind=kind, fluxion=fluxion, check=check, cmp=cmp, vm=vm)

def deriv_chain(f_eng, X, order, at_eng):
    # deriv(f,X,X) nested for higher orders, the last one at a value if asked
    e = f_eng
    for k in range(order):
        last = k == order - 1
        e = "deriv(%s,%s,%s)" % (e, X, at_eng if (last and at_eng is not None) else X)
    return e

def r_derivative(d, order, at, dx=False):
    var = var_of(d)
    py, eng = vmap_for([d["rhs"]] + ([at] if at is not None else []), var)
    f_py, f_eng = to_py(d["rhs"], py), to_eng(d["rhs"], eng)
    x, X = py[var], eng[var]
    at_py = to_py(at, py) if at is not None else None
    at_eng = to_eng(at, eng) if at is not None else None
    chk = dict(type="derivative_at" if at is not None else "derivative", expr=f_py, var=x)
    if at is not None:
        chk["at"] = at_py
    if order > 1:
        chk["order"] = order
    return res("derivative_at" if at is not None else "derivative", [deriv_chain(f_eng, X, order, at_eng)], chk,
               cmp=dict(type=chk["type"], expr=f_py, var=x, order=order, at=at_py, dx=dx), vm=py)

def r_limit(node):
    _, v, pt, side, body = node
    py, eng = vmap_for([body, pt], v)
    chk = dict(type="limit", expr=to_py(body, py), var=py[v], to=to_py(pt, py), side=side)
    return res("limit", [to_eng(node, eng)], chk, cmp=dict(type="limit", expr=chk["expr"], var=chk["var"], to=chk["to"], side=side), vm=py)

def r_integral(node):
    _, v, lo, hi, body = node
    py, eng = vmap_for([body] + ([lo, hi] if lo is not None else []), v)
    e = to_py(body, py)
    if lo is None:
        chk = dict(type="integral", expr=e, var=py[v])
        return res("integral", [to_eng(node, eng)], chk, cmp=dict(type="integral", expr=e, var=py[v], order=1), vm=py)
    chk = dict(type="definite", expr=e, var=py[v], a=to_py(lo, py), b=to_py(hi, py))
    return res("definite", [to_eng(node, eng)], chk, cmp=dict(type="definite", expr=e, var=py[v], a=chk["a"], b=chk["b"]), vm=py)

def r_value(node, kind="evaluate"):
    # a variable-free expression to work out
    py, eng = vmap_for([node], None)
    e = to_py(node, py)
    return res(kind, ["eval(%s)" % to_eng(node, eng)], dict(type="evaluate", expr=e), cmp=dict(type="evaluate", expr=e), vm=py)

def cond_bounds(info, var):
    # 0 <= x <= pi style conditions -> (lo, hi) trees
    for p in info:
        if p["t"] == "cond":
            t = p["toks"]
            if len(t) >= 5 and t[1] in ("\\le", "<") and t[3] in ("\\le", "<") and t[2] == var:
                try:
                    return p_full(new_state(t[:1])), p_full(new_state(t[4:]))
                except TexError:
                    pass
    return None

def interval_of(info):
    for p in info:
        if p["t"] == "interval":
            return p["a"], p["b"]
    return None

def pick_def(info, name=None):
    ds = fun_defs(info)
    if name:
        m = [d for d in ds if d["name"] == name]
        return m[0] if m else None
    return ds[0] if ds else None

def errs_of(info):
    # parse failures that are about real math, not a stray unit or a label
    return [p for p in info if p["t"] == "err" and "\\text" not in p["tex"] and len(p["tex"].split()) > 2]

def num_arg(arg):
    # an argument that is a plain number like 4, -1 or \pi/4, else None (x, a, t stay symbolic)
    if arg is None or not re.match(r"^\s*[-+]?[\d\\.]", arg):
        return None
    try:
        a = parse(arg)
    except TexError:
        return None
    return a if const_of(a) else None

def last_words(ask):
    # the prose right before the blank, lower case
    for sg in reversed(ask["local"]):
        if sg["k"] == "t":
            t = re.sub(r"\s+", " ", re.sub(r"\$[^$]*\$", " ", sg["s"])).strip().lower()
            if t:
                return t
    return ""

def own_text(ask):
    return " ".join(sg.get("s", "") for sg in ask["part_pre"] + ask["local"] if sg["k"] == "t")

def word_derivative(c, ds):
    # velocity / current / density at a number, with the function given. only when the number is easy to find
    w, ask = c["w"], c["ask"]
    orders = [("velocity", 1), ("acceleration", 2), ("current", 1), ("linear density", 1), ("marginal", 1), ("rate of change", 1), ("rate of increase", 1)]
    order = next((o for k, o in orders if k in w), None)
    if order is None or len(ds) != 1:
        return None
    own = own_text(ask)
    bare = re.sub(r"\$[^$]*\$", " ", own)
    hits = re.findall(r"[a-z]\s*=\s*\$?\s*(-?\d+(?:\.\d+)?)", own) + re.findall(r"after\s+(-?\d+(?:\.\d+)?)\s*(?:s\b|sec)", bare)
    m = re.match(r"^\s*(-?\d+(?:\.\d+)?)\s*[a-zA-Z]*\s*$", bare)
    if m:
        hits.append(m.group(1))
    if len(set(hits)) != 1:
        return None
    return r_derivative(ds[0], order, parse(hits[0]))

def derivative_rule(c, ds, lab):
    w, info, raw = c["w"], c["info"], c["raw"]
    if any(k in w for k in ("implicit", "tangent", "linear", "critical", "newton", "limit", "antiderivative", "average", "secant")):
        return None
    wants = "differentiat" in w or "derivative" in w or "differential" in w or re.search(r"find \$?f'\(", raw) is not None
    if lab:
        name, primes, arg = lab
        if name == "dif" and ds:
            if "evaluate" in c["lw"] or "compute" in w or "for the given values" in c["lw"]:
                skip("differential worked out at given numbers: no command", kind="derivative")
            return r_derivative(pick_def(info, arg) or ds[0], 1, None, dx=True)
        if name == "d" and ds and arg.split("/")[0] != arg.split("/")[1] and arg == "y/x":
            return r_derivative(ds[0], primes, None)
        if name not in ("d", "dif") and primes > 0:
            d = pick_def(info, name)
            if d is None:
                return None
            at = num_arg(arg)
            if at is None and arg in val_defs(info):
                at = val_defs(info)[arg]
            return r_derivative(d, primes, at)
        if name in ("v", "a") and len(ds) == 1 and ds[0]["name"] not in ("v", "a"):
            return r_derivative(ds[0], 1 if name == "v" else 2, num_arg(arg))
    if wants and ds:
        if re.search(r"f'\(a\)", raw) and "a" in val_defs(info):
            return r_derivative(ds[0], 1, val_defs(info)["a"])
        return r_derivative(ds[0], 2 if "second derivative" in w else 1, None)
    return word_derivative(c, ds)

def recognize(c):
    ask = c["ask"]
    w, lw, info, linfo, raw = c["w"], c["lw"], c["info"], c["linfo"], c["raw"]
    topic, b, label = c["topic"], ask["blank"], c["label"]
    errs = errs_of(info)
    segs = ask["segs"]
    has_fig = any(s["k"] == "fig" for s in segs)
    allmath = " ".join(maths_of(segs))
    gk = guess_kind(w, allmath)

    # things that can't be done from the text
    if has_fig and any(fw in w for fw in FIG_WORDS):
        skip("graph/figure needed", kind=gk)
    if topic in SETUP_TOPIC:
        skip("word problem, the function or equation has to be set up from the words", kind=SETUP_TOPIC[topic], setup=True)
    if any(s["k"] == "tbl" for s in ask["local"]) or (b and b.get("tb")):
        skip("table of data", kind=gk)
    if b is None:
        skip("sketch / graph / explanation, no answer blank to check" if "sketch" in lw or has_fig else "no answer blank", kind=gk)
    if "squeeze" in w or "intermediate value" in w or "completed sequentially" in w or "to show that" in w or "show that" in lw:
        skip("proof / fill-in step", kind=gk)
    if "epsilon" in allmath or "\\delta" in allmath or "definition of a limit" in w or "prove" in w:
        skip("epsilon-delta proof or fill-in", kind="limit")
    tail = last_words(ask) if b["k"] == "b" else lw
    if "sketch" in tail or "illustrate" in tail:
        skip("sketch / graph", kind=gk)
    if re.search(r"\b(explain|show that|why|what is wrong|interpret|what does|meaning)\b", tail):
        skip("explain in words", kind=gk)

    cand = linfo if any(p["t"] != "cond" for p in linfo) else info
    exprs = [p for p in cand if p["t"] == "expr"]

    # multiple choice, only the infinite limit ones are checkable
    if b["k"] == "mc":
        lim = [p for p in info if p["t"] == "expr" and p["ast"][0] == "lim"]
        opts = [o.replace("$", "").replace("−", "-").strip() for o in b["opts"]]
        if lim and "infinite limit" in w and all(re.match(r"^-?\s*(∞|\\infty)$", o) for o in opts):
            return r_limit(lim[0]["ast"])
        skip("multiple choice", kind=gk)
    lab = parse_label(label) if label else None
    if lab is None and c["rawc"]:
        m = re.search(r"find \$?([A-Za-z])('+)\(([^()$]*)\)\$?", c["rawc"])
        if m:
            lab = (m.group(1), len(m.group(2)), m.group(3))
    if "express the limit" in w or "as a limit" in w or ("set up" in w and "integral" in w and "volume" not in w):
        skip("set-up pieces to fill in (limit / integral)", kind=("area" if "area" in w else "integral"))
    if "read and complete" in lw and "limit" not in w and not (lab and (lab[1] > 0 or lab[0] in ("v", "a", "dif", "d"))):
        skip("fill-in step of an explanation", kind=gk)
    ds = fun_defs(info)
    vals = val_defs(info)

    # limits
    lims = [p for p in cand if p["t"] == "expr" and p["ast"][0] == "lim"]
    if not lims and not linfo_has_math(ask) and "limit" in w:
        lims = [p for p in info if p["t"] == "expr" and p["ast"][0] == "lim"][:1]
    if "find the limits as" in w and len(ds) == 1:
        # as x -> inf and as x -> -inf, one blank each
        d = ds[0]
        v = var_of(d)
        inf = ("const", "inf")
        pt = inf if c["idx"] == 0 else ("neg", inf)
        return r_limit(("lim", v, pt, None, d["rhs"]))
    if lims and ("limit" in w or "continuity" in w):
        if "find the value of" in w and " c " in w:
            skip("parameter in a piecewise limit", kind="continuity")
        if len(lims) > 1 and cand is linfo:
            skip("several limits in one blank", kind="limit")
        if "express the limit" in w:
            skip("write the limit as an integral: set-up", kind="limit")
        if "given that" in w:
            skip("uses given limit values", kind="limit")
        return r_limit(lims[0]["ast"])
    if "limit" in w and any("\\lim" in p["tex"] for p in errs):
        skip(latex_reason(best_err(errs)["msg"]), kind="limit")

    # integrals
    ints = [p for p in cand if p["t"] == "expr" and p["ast"][0] == "int"]
    if ints and ("integral" in w or "antiderivative" in w or "evaluate" in w or "find" in w):
        if "express the limit" in w or "estimate" in w or "property" in w or "given that" in w or "if \\int" in allmath:
            skip("integral set-up or bounds, not a value to compute", kind="integral")
        if "what is" in w or "represent" in w:
            skip("interpretation", kind="integral")
        return r_integral(ints[0]["ast"])
    if "integral" in w and errs:
        skip(latex_reason(errs[0]["msg"]), kind="integral")

    # sums
    sums = [p for p in cand if p["t"] == "expr" and p["ast"][0] == "sum"]
    if sums and ("value of the sum" in w or "expanded form" in w):
        node = sums[0]["ast"]
        py, eng = vmap_for([node], None)
        e = to_py(node, py)
        return res("sum", [to_eng(node, eng)], dict(type="evaluate", expr=e), cmp=dict(type="evaluate", expr=e), vm=py)
    if "sigma notation" in w:
        skip("answer is a sigma-notation term, free form", kind="sum")

    # domain of a function or of its derivative
    if "domain" in lw and ds:
        d = ds[0]
        var, py, eng = fun_maps(d)
        f_eng, X = to_eng(d["rhs"], eng), eng[var]
        if "derivative" in lw:
            f_eng = "deriv(%s,%s,%s)" % (f_eng, X, X)
        return res("other", ["domain(%s,%s)" % (f_eng, X)], dict(type="none"), vm=py)

    # derivatives, by label or by words
    r = derivative_rule(c, ds, lab)
    if r is not None:
        return r
    if lab and lab[2] is not None and lab[1] == 0 and pick_def(info, lab[0]) is not None and lab[0] not in ("v", "a"):
        d = pick_def(info, lab[0])
        sub = subst(d["rhs"], var_of(d), parse(lab[2]))
        if const_of(sub):
            return r_value(sub)
        py, eng = vmap_for([sub], None)
        return res("simplify", [to_eng(sub, eng)], dict(type="simplify", expr=to_py(sub, py)), vm=py)
    if lab is None and label and vals:
        # a label like sin(t)= with t given somewhere above
        try:
            e = parse(re.sub(r"[=\s]+$", "", clean_tex(label)))
            if free_vars(e) and free_vars(e) <= set(vals):
                for v in free_vars(e):
                    e = subst(e, v, vals[v])
                if const_of(e):
                    return r_value(e)
        except TexError:
            pass

    # implicit differentiation
    if "implicit" in w and "tangent" not in w:
        eqs = [p for p in cand if p["t"] == "eq"] or [p for p in info if p["t"] == "eq"]
        if not eqs:
            skip(latex_reason(errs[0]["msg"]) if errs else "no equation found", kind="implicit")
        e = eqs[0]
        fv = free_vars(e["lhs"]) | free_vars(e["rhs"])
        wrt_y = lab and lab[0] == "d" and lab[2] == "x/y"
        if lab and lab[0] not in ("d", "dif") and lab[1] >= 2:
            skip("second implicit derivative: no command", kind="implicit")
        if "x" not in fv or "y" not in fv:
            skip("implicit equation without both x and y", kind="implicit")
        py, eng = vmap_for([e["lhs"], e["rhs"]], None)
        num, den = ("x", "y") if wrt_y else ("y", "x")
        l_py, r_py = to_py(e["lhs"], py), to_py(e["rhs"], py)
        eq_eng = "%s=%s" % (to_eng(e["lhs"], eng), to_eng(e["rhs"], eng))
        chk = dict(type="implicit", lhs=l_py, rhs=r_py, y=py[num], x=py[den])
        return res("implicit", ["implicit(%s,%s,%s)" % (eq_eng, eng[num], eng[den])], chk, cmp=dict(chk), vm=py)
    if "implicit" in w:
        skip("tangent to an implicit curve: no single command", kind="implicit")

    # tangent lines and linearization
    if "tangent line" not in w and ("linearization" in w or "linear approximation" in w) and not (lab and lab[0] == "L"):
        skip("approximating a number with the linearization", kind="tangent")
    if "tangent" in w and "normal" in last_words(ask):
        skip("normal line: no command", kind="tangent")
    if ("tangent line" in w or "linearization" in w or "linear approximation" in w) and "approximate" not in lw and "estimate" not in lw:
        if "linear approximation" in w and "numbers" in lw:
            skip("uses the linearization to approximate numbers", kind="tangent")
        if not ds:
            if any(p["t"] == "eq" for p in info):
                skip("tangent to an implicit curve: no single command", kind="tangent")
            skip(latex_reason(errs[0]["msg"]) if errs else "no curve given", kind="tangent")
        d = ds[0]
        var = var_of(d)
        pts = [p for p in info if p["t"] == "point"]
        a = pts[0]["a"] if pts else (vals.get("a") if "a" in vals else vals.get(var))
        if a is None:
            skip("no point given for the tangent", kind="tangent")
        if "slope" in lw:
            return r_derivative(d, 1, a)
        py, eng = vmap_for([d["rhs"], a], var)
        chk = dict(type="tangent", expr=to_py(d["rhs"], py), var=py[var], at=to_py(a, py))
        return res("tangent", ["tangent(%s,%s,%s)" % (to_eng(d["rhs"], eng), to_eng(a, eng), eng[var])], chk, cmp=dict(chk), vm=py)

    # critical numbers, extrema, inflection, increasing / decreasing
    if "critical number" in w and ds:
        d = ds[0]
        var, py, eng = fun_maps(d)
        chk = dict(type="critical", expr=to_py(d["rhs"], py), var=py[var])
        return res("critical", ["critical(%s,%s)" % (to_eng(d["rhs"], eng), eng[var])], chk, vm=py)
    if ds and "(x,y)" in re.sub(r"\s+", "", " ".join(seg_text(sg) for sg in ask["local"])[-40:]):
        d = ds[0]
        var, py, eng = fun_maps(d)
        return res("inflect", ["inflect(%s,%s)" % (to_eng(d["rhs"], eng), eng[var])], dict(type="inflect", expr=to_py(d["rhs"], py), var=py[var]), vm=py)
    if ds and "absolute" in w and ("maximum" in w or "minimum" in w):
        skip("absolute extrema on an interval: no single command (critical numbers plus end values)", kind="extrema")
    tl = last_words(ask) or lw
    if ds and re.search(r"(increasing|decreasing|of increase|of decrease|concave|inflection|local m(?:in|ax))", tl):
        d = ds[0]
        var, py, eng = fun_maps(d)
        f_py, f_eng, X = to_py(d["rhs"], py), to_eng(d["rhs"], eng), eng[var]
        if "concave" in tl:
            up = "upward" in tl or "concave up" in tl
            chk = dict(type="concave", expr=f_py, var=py[var], which="up" if up else "down")
            return res("inflect", ["increasing(deriv(%s,%s,%s),%s)" % (f_eng, X, X, X)], chk, vm=py)
        if "inflection" in tl:
            return res("inflect", ["inflect(%s,%s)" % (f_eng, X)], dict(type="inflect", expr=f_py, var=py[var]), vm=py)
        if re.search(r"increasing|of increase", tl):
            return res("increasing", ["increasing(%s,%s)" % (f_eng, X)], dict(type="increasing", expr=f_py, var=py[var], which="increasing"), vm=py)
        if re.search(r"decreasing|of decrease", tl):
            return res("increasing", ["increasing(%s,%s)" % (f_eng, X)], dict(type="increasing", expr=f_py, var=py[var], which="decreasing"), vm=py)
        which = "min" if ("minimum" in tl and "maximum" not in tl) else "max" if ("maximum" in tl and "minimum" not in tl) else None
        return res("extrema", ["extrema(%s,%s)" % (f_eng, X)], dict(type="extrema", expr=f_py, var=py[var], which=which), vm=py)

    # mean value theorem, Rolle, average value
    if ("rolle" in w or "mean value theorem" in w) and ds and ("find all numbers" in w or "find all values" in w or "numbers c" in w):
        iv = interval_of(info)
        if iv is None:
            skip("interval not found", kind="mvt")
        d = ds[0]
        var, py, eng = fun_maps(d, iv[0], iv[1])
        chk = dict(type="mvt", expr=to_py(d["rhs"], py), var=py[var], a=to_py(iv[0], py), b=to_py(iv[1], py))
        return res("mvt", ["mvt(%s,%s,%s,%s)" % (to_eng(d["rhs"], eng), to_eng(iv[0], eng), to_eng(iv[1], eng), eng[var])], chk, vm=py)
    if "average value" in w and ds:
        iv = interval_of(info)
        if iv is None:
            skip("interval not found", kind="average")
        d = ds[0]
        var, py, eng = fun_maps(d, iv[0], iv[1])
        chk = dict(type="average", expr=to_py(d["rhs"], py), var=py[var], a=to_py(iv[0], py), b=to_py(iv[1], py))
        return res("average", ["average(%s,%s,%s,%s)" % (to_eng(d["rhs"], eng), to_eng(iv[0], eng), to_eng(iv[1], eng), eng[var])], chk, vm=py)

    # area between curves
    if "area" in w and ("region" in w or "curves" in w) and "set up" not in w:
        return r_area(info, errs)
    if "set up" in w and "area" in w:
        skip("integral set-up pieces to fill in", kind="area")

    # newton: only the find-all-roots ones are plain equations
    if "newton" in w:
        eqs = [p for p in info if p["t"] == "eq"]
        if "find all" in w and eqs:
            e = eqs[0]
            py, eng = vmap_for([e["lhs"], e["rhs"]], "x")
            chk = dict(type="solve", eqs=["Eq(%s,%s)" % (to_py(e["lhs"], py), to_py(e["rhs"], py))], vars=[py["x"]], numeric=True)
            return res("newton", ["solve(%s=%s,%s)" % (to_eng(e["lhs"], eng), to_eng(e["rhs"], eng), eng["x"])], chk, vm=py)
        skip("newton iteration steps: no newton command", kind="newton")

    # antiderivatives
    gd = [p for p in info if p["t"] == "def" and p["primes"] > 0]
    if "antiderivative" in w or (gd and re.search(r"find \$f\$", raw)):
        if gd:
            g = gd[0]
            if len([p for p in info if p["t"] in ("def", "eq", "point", "err")]) > 1 or "satisf" in w or "particle" in w:
                skip("initial conditions to solve for the constants", kind="integral")
            var, py, eng = fun_maps(g)
            e, f_eng, X = to_py(g["rhs"], py), to_eng(g["rhs"], eng), eng[var]
            ig = "integral(%s,%s)" % (f_eng, X)
            for _ in range(g["primes"] - 1):
                ig = "integral(%s,%s)" % (ig, X)
            chk = dict(type="integral", expr=e, var=py[var], order=g["primes"])
            return res("integral", [ig], chk, cmp=dict(chk), vm=py)
        if ds and "most general" in w and len(ds) == 1:
            d = ds[0]
            var, py, eng = fun_maps(d)
            e = to_py(d["rhs"], py)
            chk = dict(type="integral", expr=e, var=py[var])
            return res("integral", ["integral(%s,%s)" % (to_eng(d["rhs"], eng), eng[var])], chk, cmp=dict(chk, order=1), vm=py)
        skip("antiderivative with a condition to fit: no single command", kind="integral")

    # precalc: evaluate, simplify, solve
    if re.search(r"(evaluate the expression|find the exact value|evaluate \(if possible\))", w) and exprs and "trigonometric functions" not in w:
        return r_value(exprs[-1]["ast"])
    if re.search(r"(simplify|expand the expression|combine the expression|fundamental identities)", w) and exprs:
        node = exprs[-1]["ast"]
        py, eng = vmap_for([node], None)
        return res("simplify", [to_eng(node, eng)], dict(type="simplify", expr=to_py(node, py)), vm=py)
    if re.search(r"(solve the equation|real solutions)", w):
        eqs = [p for p in info if p["t"] == "eq"]
        if eqs:
            e = eqs[0]
            fv = free_vars(e["lhs"]) | free_vars(e["rhs"])
            if len(fv) == 1:
                v = list(fv)[0]
                py, eng = vmap_for([e["lhs"], e["rhs"]], v)
                chk = dict(type="solve", eqs=["Eq(%s,%s)" % (to_py(e["lhs"], py), to_py(e["rhs"], py))], vars=[py[v]])
                return res("solve", ["solve(%s=%s,%s)" % (to_eng(e["lhs"], eng), to_eng(e["rhs"], eng), eng[v])], chk, vm=py)

    # asymptotes
    if "asymptote" in w and len(ds) == 1:
        d = ds[0]
        var, py, eng = fun_maps(d)
        return res("other", ["asym(%s,%s)" % (to_eng(d["rhs"], eng), eng[var])], dict(type="none"), vm=py)

    # what is left
    if any(p["t"] == "err" and "piecewise" in p["msg"] for p in info):
        skip("piecewise definition (latex: piecewise or array definition)", kind=gk)
    if "particle" in w and "given data" in w:
        skip("initial conditions to solve for the constants", kind="integral")
    if any(e["msg"].startswith("unknown function") for e in errs) and not ds:
        skip(latex_reason([e["msg"] for e in errs if e["msg"].startswith("unknown function")][0]), kind=gk)
    if re.search(r"(average velocity|secant)", w):
        skip("secant slope / average velocity estimate: no command", kind="other")
    if re.search(r"riemann sum|endpoints|midpoint", w):
        skip("riemann sum estimate: no command", kind="sum")
    if len(re.findall(r"[a-z]{3,}", w)) >= 14:
        skip("word problem, the operation has to be worked out from the words", kind="word", setup=True)
    skip("no mechanical conversion found for this prompt", kind=gk)

def linfo_has_math(ask):
    return any(p["t"] in ("expr", "def", "eq", "err") for p in collect(maths_of(ask["local"])))

def best_err(errs):
    for e in errs:
        if e["msg"].startswith("unknown function"):
            return e
    return errs[0]

def latex_reason(msg):
    if msg.startswith("unknown function"):
        return "needs given values or a graph for unknown functions (latex: %s)" % msg
    if "piecewise" in msg or "array" in msg:
        return "piecewise definition (latex: %s)" % msg
    return "latex: " + msg

def guess_kind(w, math):
    pairs = [("related", "related_rates"), ("newton", "newton"), ("mean value", "mvt"), ("rolle", "mvt"), ("critical", "critical"), ("inflection", "inflect"),
             ("concave", "inflect"), ("increasing", "increasing"), ("decreasing", "increasing"), ("implicit", "implicit"), ("tangent", "tangent"),
             ("linear approximation", "tangent"), ("linearization", "tangent"), ("derivative", "derivative"), ("differentiat", "derivative"), ("differential", "derivative"),
             ("average value", "average"), ("volume", "volume"), ("area", "area"), ("continu", "continuity"), ("maximum", "extrema"), ("minimum", "extrema"),
             ("limit", "limit"), ("sum", "sum"), ("antiderivative", "integral"), ("integral", "integral"), ("solve", "solve"), ("simplify", "simplify"),
             ("evaluate", "evaluate")]
    for k, v in pairs:
        if k in w.replace("increasing order", "").replace("decreasing order", ""):
            return v
    if "\\lim" in math:
        return "limit"
    if "\\int" in math:
        return "integral"
    return "other"

def r_area(info, errs):
    curves = [p for p in info if p["t"] == "def" and p["name"] in ("y", "x") and p["primes"] == 0]
    if len(curves) < 2:
        if errs:
            skip(latex_reason(errs[0]["msg"]), kind="area")
        skip("curves not given in y = f(x) form", kind="area")
    ys = [p for p in curves if p["name"] == "y" and free_vars(p["rhs"])]
    zero_y = [p for p in curves if p["name"] == "y" and not free_vars(p["rhs"])]
    xs_const = [p for p in curves if p["name"] == "x" and not free_vars(p["rhs"])]
    xs_fun = [p for p in curves if p["name"] == "x" and free_vars(p["rhs"])]
    cb = cond_bounds(info, "x")
    if len(ys) == 2 and not xs_fun:
        f, g = ys[0]["rhs"], ys[1]["rhs"]
    elif len(ys) == 1 and len(zero_y) == 1:
        f, g = ys[0]["rhs"], zero_y[0]["rhs"]
    elif len(xs_fun) == 2 and not ys:
        py, eng = vmap_for([xs_fun[0]["rhs"], xs_fun[1]["rhs"]], "y")
        fp, gp = to_py(xs_fun[0]["rhs"], py), to_py(xs_fun[1]["rhs"], py)
        chk = dict(type="area", f=fp, g=gp, var=py["y"], a=None, b=None)
        return res("area", ["area(%s,%s)" % (to_eng(xs_fun[0]["rhs"], eng), to_eng(xs_fun[1]["rhs"], eng))], chk, vm=py)
    else:
        skip("curves not given as y = f(x) pairs", kind="area")
    a = b = None
    if len(xs_const) == 2:
        a, b = xs_const[0]["rhs"], xs_const[1]["rhs"]
    elif cb:
        a, b = cb
    elif len(xs_const) == 1:
        skip("one vertical line given, the other end is where the curves cross: no single command", kind="area")
    py, eng = vmap_for([f, g] + ([a, b] if a is not None else []), "x")
    fp, gp = to_py(f, py), to_py(g, py)
    chk = dict(type="area", f=fp, g=gp, var=py["x"], a=to_py(a, py) if a is not None else None, b=to_py(b, py) if b is not None else None)
    args = "%s,%s" % (to_eng(f, eng), to_eng(g, eng))
    if a is not None:
        args += ",%s,%s" % (to_eng(a, eng), to_eng(b, eng))
    return res("area", ["area(%s)" % args], chk, vm=py)

# comparing the key with sympy's own answer

def ext_vmap(vm, ast):
    out, used = dict(vm), set(vm.values())
    for v in free_vars(ast):
        if v not in out:
            letter = v.lower() if len(v) == 1 else None
            if letter is None or letter in used:
                raise TexError("key variable %s" % v)
            out[v] = letter
            used.add(letter)
    return out

def key_vs_truth(cmp, ke, dne, decimal):
    # True when the key agrees with sympy, False when not, None when it can't be told
    tol = 1e-3 if decimal else 1e-6
    t = cmp["type"]
    x = sp.Symbol(cmp["var"]) if cmp.get("var") else None
    if t in ("derivative", "derivative_at"):
        f = settle(sp_parse(cmp["expr"]))
        d = timed(lambda: sp.diff(f, x, cmp["order"]))
        if t == "derivative_at":
            d = d.subs(x, sp_parse(cmp["at"]))
        elif x not in ke.free_symbols and len(ke.free_symbols) == 1 and not (ke.free_symbols & d.free_symbols):
            ke = ke.subs(list(ke.free_symbols)[0], x)
        return agree(d, ke, tol)
    if t == "limit":
        f = settle(sp_parse(cmp["expr"]))
        a = sp_parse(cmp["to"])
        if cmp["side"]:
            v = timed(lambda: sp.limit(f, x, a, cmp["side"]))
        else:
            l = timed(lambda: sp.limit(f, x, a, "-"))
            r = timed(lambda: sp.limit(f, x, a, "+"))
            v = l if l == r else None
        if v is not None and (v.has(sp.nan) or v.has(sp.AccumBounds) or v.has(sp.zoo)):
            return None
        if dne:
            return v is None
        if v is None:
            return False
        if v in (sp.oo, -sp.oo) or ke in (sp.oo, -sp.oo):
            return v == ke
        return agree(v, ke, tol)
    if t == "integral":
        f = settle(sp_parse(cmp["expr"]))
        dk = timed(lambda: sp.diff(ke, x, cmp["order"]))
        return agree(dk, f, tol)
    if t == "definite":
        f = sp_parse(cmp["expr"])
        a, b = sp_parse(cmp["a"]), sp_parse(cmp["b"])
        v = timed(lambda: sp.Integral(f, (x, a, b)).evalf(15), 12)
        return agree(v, ke, tol)
    if t == "tangent":
        f = sp_parse(cmp["expr"])
        a = sp_parse(cmp["at"])
        line = f.subs(x, a) + sp.diff(f, x).subs(x, a) * (x - a)
        return agree(line, ke, tol)
    if t in ("evaluate", "simplify"):
        return agree(settle(sp_parse(cmp["expr"])), settle(ke), tol)
    if t == "area":
        f, g = sp_parse(cmp["f"]), sp_parse(cmp["g"])
        roots = timed(lambda: sp.solveset(f - g, x, sp.S.Reals))
        if not isinstance(roots, sp.FiniteSet):
            return None
        pts = sorted(float(r.evalf()) for r in roots)
        if cmp["a"] is not None:
            a, b = float(sp_parse(cmp["a"]).evalf()), float(sp_parse(cmp["b"]).evalf())
            edges = [a] + [p for p in pts if a < p < b] + [b]
        else:
            edges = pts
        if len(edges) < 2:
            return None
        total = sum(abs(timed(lambda: sp.Integral(f - g, (x, l, h)).evalf(15), 12)) for l, h in zip(edges, edges[1:]))
        return agree(sp.Float(total), ke, tol)
    if t in ("critical", "mvt", "extrema", "average"):
        f = sp_parse(cmp["expr"])
        fp = sp.diff(f, x)
        if t == "critical":
            return abs(complex(fp.subs(x, ke).evalf())) < 1e-6
        if t == "mvt":
            a, b = sp_parse(cmp["a"]), sp_parse(cmp["b"])
            c = complex(ke.evalf())
            return bool(float(a.evalf()) < c.real < float(b.evalf())) and abs(complex((fp.subs(x, ke) - (f.subs(x, b) - f.subs(x, a)) / (b - a)).evalf())) < 1e-6
        if t == "average":
            a, b = sp_parse(cmp["a"]), sp_parse(cmp["b"])
            v = timed(lambda: sp.Integral(f, (x, a, b)).evalf(15), 12) / (b - a)
            return agree(v, ke, tol)
        roots = timed(lambda: sp.solveset(fp, x, sp.S.Reals))
        if not isinstance(roots, sp.FiniteSet):
            return None
        pts = list(roots)
        try:
            pts += [p for p in sp.singularities(fp, x) if p.is_real]
        except Exception:
            pass
        vals = [complex(f.subs(x, r).evalf()) for r in pts]
        kv = complex(ke.evalf())
        return any(abs(v - kv) <= tol * max(1.0, abs(kv)) for v in vals)
    if t == "implicit":
        F = sp_parse(cmp["lhs"]) - sp_parse(cmp["rhs"])
        xs, ys = sp.Symbol(cmp["x"]), sp.Symbol(cmp["y"])
        want = -sp.diff(F, xs) / sp.diff(F, ys)
        rnd = random.Random(3)
        seen = 0
        for _ in range(10):
            x0 = rnd.uniform(0.4, 2.5)
            try:
                y0 = sp.nsolve(F.subs(xs, x0), ys, rnd.uniform(0.3, 3))
                gv, wv = complex(ke.subs({xs: x0, ys: y0}).evalf()), complex(want.subs({xs: x0, ys: y0}).evalf())
            except Exception:
                continue
            if wv != wv or abs(wv) > 1e6:
                continue
            seen += 1
            if abs(gv - wv) > 1e-4 * max(1, abs(wv)):
                return False
            if seen >= 3:
                break
        return True if seen else None
    return None

def key_check(r, raw):
    # key_tex / key_sympy / key_mismatch for a converted item
    out = dict(key_sympy=None, key_mismatch=False, key_checked=False)
    if raw is None:
        return out
    cmp = r.get("cmp") or r["check"]
    try:
        ast = key_to_expr(raw, strip_c=cmp.get("type") == "integral", strip_d=bool(cmp.get("dx")))
    except TexError:
        return out
    if ast is None:
        return out
    dne = ast == "dne"
    ke = None
    if not dne:
        try:
            s = to_py(ast, ext_vmap(r.get("vm") or {}, ast))
            ke = sp_parse(s)
        except Exception:
            return out
        out["key_sympy"] = s
    if cmp:
        try:
            ok = key_vs_truth(cmp, ke, dne, bool(re.search(r"\d\.\d", raw)))
        except Exception:
            ok = None
        if ok is not None:
            out["key_checked"] = True
            out["key_mismatch"] = not ok
    return out

def validate_check(chk):
    # every sympy string in a check has to parse, returns an error text or None
    for k, v in chk.items():
        vals = v if isinstance(v, list) else [v]
        if k in ("expr", "f", "g", "lhs", "rhs", "a", "b", "at", "to", "eqs"):
            for s in vals:
                if s is None:
                    continue
                try:
                    sp_parse(s)
                except Exception as e:
                    return "sympy can't parse %s=%r (%s)" % (k, s, str(e)[:40])
    return None

# putting items together

def plain_text(ask):
    out = []
    for sg in ask["segs"]:
        k = sg["k"]
        piece = {"t": sg.get("s", ""), "m": "$" + sg.get("s", "") + "$", "b": "___", "fig": "[figure]", "tbl": "[table]",
                 "mc": "[choices: " + " | ".join(sg.get("opts", [])) + "]"}.get(k, "")
        if "difficulty" in piece:
            continue
        if out and out[-1] == piece and k in ("fig", "tbl"):
            continue
        out.append(piece)
    return re.sub(r"\s+", " ", " ".join(out)).replace("$$", "$ $").strip()

def label_of(ask):
    for sg in reversed(ask["local"]):
        if sg["k"] == "m":
            return sg["s"]
        if sg["k"] == "t":
            if sg["s"].strip().endswith("$"):
                ms = re.findall(r"\$([^$]+)\$", sg["s"])
                if ms:
                    return ms[-1]
            return sg["s"]
        if sg["k"] in ("fig", "tbl"):
            return None
    return None

def item_ids(prob, asks):
    per_part = {}
    for a in asks:
        per_part[a["part"]] = per_part.get(a["part"], 0) + 1
    counts, ids = {}, []
    for i, a in enumerate(asks):
        if len(asks) == 1:
            ids.append(prob["id"])
            continue
        if not any(x["part"] for x in asks):
            ids.append("%s#%d" % (prob["id"], i + 1))
            continue
        p = a["part"] or "0"
        counts[p] = counts.get(p, 0) + 1
        ids.append("%s#%s" % (prob["id"], p if per_part.get(a["part"]) == 1 else "%s%d" % (p, counts[p])))
    return ids

def merge_table_cells(asks):
    # a run of blanks inside a table is one "complete the table" item
    out = []
    for a in asks:
        if a["blank"] and a["blank"].get("tb") and out and out[-1]["blank"] and out[-1]["blank"].get("tb") and out[-1]["part"] == a["part"]:
            out[-1]["cells"] = out[-1].get("cells", 1) + 1
            continue
        out.append(a)
    return out

def convert_problem(prob):
    asks = merge_table_cells(build_asks(prob))
    ent = key_entries(prob["key"])
    ids = item_ids(prob, asks)
    nblanks = sum(1 for a in asks if a["blank"] and a["blank"]["k"] == "b")
    topic = topic_of(prob["id"])
    items, mcn = [], 0
    for a, iid in zip(asks, ids):
        w = words_of(a["segs"])
        lw = words_of(a["part_pre"] + a["local"]) if a["part"] else words_of(a["local"])
        info = collect(maths_of(a["segs"]))
        linfo = collect(maths_of(a["local"]))
        raw = re.sub(r"\s+", " ", " ".join(seg_text(sg) for sg in a["segs"])).lower()
        rawc = re.sub(r"\s+", " ", " ".join(seg_text(sg) for sg in a["segs"]))
        c = dict(ask=a, w=w, lw=lw, info=info, linfo=linfo, topic=topic, label=label_of(a), raw=raw, rawc=rawc, idx=len(items))
        text = plain_text(a)
        r = None
        try:
            r = recognize(c)
            bad = validate_check(r["check"])
            if bad:
                skip(bad, kind=r["kind"])
        except TexError as e:
            r, why = None, (latex_reason(str(e)), guess_kind(w, " ".join(maths_of(a["segs"]))), False)
        except LookupError as e:
            r, why = None, e.args
        b = a["blank"]
        if b is not None and b["k"] == "mc":
            key = ent["choice"][mcn] if mcn < len(ent["choice"]) else None
            mcn += 1
        elif b is not None and not b.get("tb"):
            key = key_for(a, ent, len(asks) == 1 or nblanks == 1, sum(1 for x in asks if x["part"] == a["part"]) == 1)
        else:
            key = None
        pts = float(prob["pts"]) if prob["pts"] else None
        if pts is not None and pts.is_integer():
            pts = int(pts)
        rec = dict(id=iid, section=prob["section"], pts=pts, text=text)
        if r is not None:
            kc = key_check(r, key)
            rec.update(kind=r["kind"], convertible=True, reason=None, setup_needed=False, setup_text=None, fluxion=r["fluxion"], check=r["check"])
        else:
            kc = dict(key_sympy=None, key_mismatch=False, key_checked=False)
            rec.update(kind=why[1] or "other", convertible=False, reason=why[0], setup_needed=bool(why[2]),
                       setup_text=text if why[2] else None, fluxion=None, check=dict(type="none"))
        rec.update(key_tex=key, key_sympy=kc["key_sympy"], key_mismatch=kc["key_mismatch"], key_checked=kc["key_checked"], source_line=prob["line"])
        if b is not None and b.get("n") in ent["wrong_n"]:
            rec["key_marked_wrong"] = True
        items.append(rec)
    return items

def main():
    src = sys.argv[1] if len(sys.argv) > 1 else os.path.expanduser("~/Downloads/all-problems.md")
    dst = sys.argv[2] if len(sys.argv) > 2 else os.path.expanduser("~/Downloads/calc1-problems/problems.jsonl")
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    probs = read_problems(src)
    items, seen = [], set()
    for p in probs:
        for it in convert_problem(p):
            # the exam reviews reuse the ids of the homework, tag the later ones
            if it["id"] in seen:
                m = re.search(r"Exam (\d)", p["section"] or "")
                it["id"] += "~ex" + m.group(1) if m else "~dup"
            seen.add(it["id"])
            items.append(it)
    with open(dst, "w", encoding="utf-8") as f:
        for it in items:
            f.write(json.dumps(it, ensure_ascii=False) + "\n")
    summary(probs, items)

def summary(probs, items):
    from collections import Counter
    print("%d problems, %d items" % (len(probs), len(items)))
    print("by kind:", dict(Counter(i["kind"] for i in items).most_common()))
    conv = [i for i in items if i["convertible"]]
    setup = [i for i in items if i["setup_needed"]]
    rest = [i for i in items if not i["convertible"] and not i["setup_needed"]]
    print("convertible %d, setup_needed %d, not convertible %d" % (len(conv), len(setup), len(rest)))
    print("top reasons:")
    for k, v in Counter(re.sub(r"\(latex:.*", "", i["reason"]).strip() for i in rest).most_common(14):
        print("  %4d %s" % (v, k))
    print("key_sympy filled %d, compared with sympy %d, key_mismatch %d" % (sum(1 for i in conv if i["key_sympy"]), sum(1 for i in conv if i["key_checked"]), sum(1 for i in conv if i["key_mismatch"])))

if __name__ == "__main__":
    main()

