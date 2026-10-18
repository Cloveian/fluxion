# writes web/frames.css, the cut corner outlines on the showcase page. each one's a little
# 9-slice svg used as a border-image: the corners stay their size and only the straight
# bits stretch. border-image gets snapped to the screen's pixels and clip-path doesnt, so
# the outlines come out crisp all the way round instead of smeared on whatever side lands
# on half a pixel (it's always some side, text widths are never whole numbers)
#
# colours come out of style.css's :root so they're only written down once. svg in a data
# url can't see css vars, hence a generator at all
#
#   python3 tools/frames.py

import math, os, re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WEB = os.path.join(ROOT, "web")
BEVEL = .1  # how much of each cut's two corners gets taken off again, softens them
TINTS = ("pink", "peach", "butter", "mint", "blue", "lilac")  # the feature cards' colours, features.js picks

# selector, cut, fill, edge (None = no outline, just the shape), then optionally how thick
# the edge is (t), the bottom edge if it's thicker (bb, a keycap's side) and which corners
# get cut. the big stuff gets 2px, 1 looked flimsy on all that black
FRAMES = [
    (".panel", 22, "bg", "blue", {"t": 2}),
    (".keys", 12, "bg", "pink", {"t": 2}),
    (".install", 18, "bg", "mint", {"t": 2}),
    (".card", 14, "bg", "line"),
    *((f".card.tint-{t}", 14, "bg", f"tint-{t}") for t in TINTS),
    (".key-tables > div", 12, "bg", "line", {"t": 2}),
    ("button, .button", 9, "bg", "line", {"t": 2}),
    ("button:hover, .button:hover, .button:focus-visible", 9, "bg", "blue", {"t": 2}),
    (".primary, .primary:hover", 9, "blue", None),
    ("kbd", 5, "bg", "#5e5a6e", {"bb": 3}),
    ("kbd.calc", 4, "bg", "#6b6878"),
    ("kbd.calc.light", 4, "ink", None),
    ("kbd.calc.second", 4, "blue", None),
    ("kbd.calc.alpha", 4, "green", None),
    (".why > i", 3, "bg", "lilac"),
    (".why:hover > i, .why:focus > i", 3, "lilac", None),
    *((f".tint-{t} h3", 3, f"tint-{t}", None) for t in TINTS),
    (".err", 12, "bg", "butter", {"t": 2}),
    (".folders [aria-pressed=\"true\"]", 9, "ink", None),
    *((f".tint-{t} li.playing, .tint-{t} li.playing:hover", 7, f"tint-{t}", None) for t in TINTS),
    ("code", 4, "bg", "line"),
    ("#try[data-tip]:not([data-tip=\"\"])::before, .tip::after, .why::after, .gotcha", 7, "bg", "lilac/60", {"t": 2}),
    # the ?'s in a card go its colour, tooltip and all
    *((f".tint-{t} .why > i", 3, "bg", f"tint-{t}") for t in TINTS),
    *((f".tint-{t} .why:hover > i, .tint-{t} .why:focus > i", 3, f"tint-{t}", None) for t in TINTS),
    *((f".tint-{t} .why::after", 7, "bg", f"tint-{t}", {"t": 2}) for t in TINTS),
]


def palette():
    with open(os.path.join(WEB, "style.css")) as f:
        root = re.search(r":root\s*{(.*?)}", f.read(), re.S).group(1)
    return dict(re.findall(r"--([\w-]+):\s*(#[0-9a-fA-F]{6})", root))


def shape(x0, y0, x1, y1, c, d, cut):
    # 16 points, the cut and then a little bevel either side of it. e is the bevel along
    # the diagonal, d along the straight side. cut = which corners, clockwise from top left
    e = d * .7071
    pts = [(0, c + d), (e, c - e), (c - e, e), (c + d, 0)]
    corners = []
    for i, (sx, sy, ox, oy) in enumerate(((1, 1, x0, y0), (-1, 1, x1, y0), (-1, -1, x1, y1), (1, -1, x0, y1))):
        p = [(ox + sx * x, oy + sy * y) for x, y in (pts if i in cut else [(0, 0)])]
        corners += p if sx * sy > 0 else p[::-1]
    n = lambda v: f"{v:.3f}".rstrip("0").rstrip(".")
    return "M" + "L".join(f"{n(x)} {n(y)}" for x, y in corners) + "Z"


def frame(c, fill, edge, t=1, bb=None, cut=(0, 1, 2, 3)):
    bb = bb or t
    d = c * BEVEL
    s = math.ceil(c + d) + t  # slice, the bit that doesnt stretch
    sb = s + bb - t
    w, h = 2 * s + 2, s + sb + 2
    if edge:
        # each pixel in from the outline takes .586 off the cut and .215 off the bevel
        # (1 - tan 22.5, and the same with 11.25 for the bevel's two corners)
        paths = f"<path fill='{edge}' d='{shape(0, 0, w, h, c, d, cut)}'/>" \
                f"<path fill='{fill}' d='{shape(t, t, w - t, h - bb, c - .586 * t, d - .215 * t, cut)}'/>"
    else:
        paths = f"<path fill='{fill}' d='{shape(0, 0, w, h, c, d, cut)}'/>"
    svg = f"<svg xmlns='http://www.w3.org/2000/svg' width='{w}' height='{h}'>{paths}</svg>"
    url = svg.replace("#", "%23").replace("<", "%3C").replace(">", "%3E")
    return f'url("data:image/svg+xml,{url}") {s} {s} {sb} fill / {s}px {s}px {sb}px'


def main():
    colours = palette()
    def col(v):
        if v is None or v.startswith("#"):
            return v
        name, _, pct = v.partition("/")  # pink/25 = a quarter pink over the black
        if not pct:
            return colours[name]
        rgb = [int(colours[name][i:i + 2], 16) for i in (1, 3, 5)]
        bg = [int(colours["bg"][i:i + 2], 16) for i in (1, 3, 5)]
        return "#" + "".join(f"{round(b + (c - b) * int(pct) / 100):02x}" for c, b in zip(rgb, bg))

    out = ["/* made by tools/frames.py, dont edit, edit that */"]
    for sel, c, fill, edge, *opts in FRAMES:
        out.append(f"{sel} {{ border-image: {frame(c, col(fill), col(edge), **(opts or [{}])[0])}; }}")
    with open(os.path.join(WEB, "frames.css"), "w") as f:
        f.write("\n".join(out) + "\n")


main()
