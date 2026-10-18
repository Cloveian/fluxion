// the CE as an svg, screen half and keypad. sizes and colours measured off CEmu's (its 162x238 grid),
// no code from it tho, its GPL

// [name (keys_pc.c), label, 2nd, alpha]
const GRAPH = [
    ["y=", "y=", "stat plot", "f1"],
    ["window", "window", "tblset", "f2"],
    ["zoom", "zoom", "format", "f3"],
    ["trace", "trace", "calc", "f4"],
    ["graph", "graph", "table", "f5"],
];

// next to the arrows
const TOP = [
    [["2nd", "2nd", "", "", "second"], ["mode", "mode", "quit", ""], ["del", "del", "ins", ""]],
    [["alpha", "alpha", "A-lock", "", "alpha"], ["xt", "X,T,θ,n", "link", ""], ["stat", "stat", "list", ""]],
];

// 5th is the kind, dark if left out
const ROWS = [
    [["math", "math", "test", "A"], ["apps", "apps", "angle", "B"], ["prgm", "prgm", "draw", "C"],
     ["vars", "vars", "distr", ""], ["clear", "clear", "", ""]],
    [["x^-1", "x⁻¹", "matrix", "D"], ["sin", "sin", "sin⁻¹", "E"], ["cos", "cos", "cos⁻¹", "F"],
     ["tan", "tan", "tan⁻¹", "G"], ["^", "^", "π", "H"]],
    [["x^2", "x²", "√", "I"], [",", ",", "EE", "J"], ["(", "(", "{", "K"],
     [")", ")", "}", "L"], ["/", "÷", "e", "M", "light"]],
    [["log", "log", "10ˣ", "N"], ["7", "7", "u", "O", "num"], ["8", "8", "v", "P", "num"],
     ["9", "9", "w", "Q", "num"], ["*", "×", "[", "R", "light"]],
    [["ln", "ln", "eˣ", "S"], ["4", "4", "L4", "T", "num"], ["5", "5", "L5", "U", "num"],
     ["6", "6", "L6", "V", "num"], ["-", "−", "]", "W", "light"]],
    [["sto", "sto→", "rcl", "X"], ["1", "1", "L1", "Y", "num"], ["2", "2", "L2", "Z", "num"],
     ["3", "3", "L3", "θ", "num"], ["+", "+", "mem", "\"", "light"]],
    [["on", "on", "off", ""], ["0", "0", "catalog", "␣", "num"], [".", ".", "i", ":", "num"],
     ["neg", "(−)", "ans", "?", "num"], ["enter", "enter", "entry", "solve", "light"]],
];

// the page inverted, a pastel one with black on it (they do sell CEs in colours). the key
// half is the page's pink, flat like everything else on the page. shading it like the
// plastic curving away just made it look grubby next to all the flat stuff
const COLOURS = {
    body: "#f5a9c9", bodyEdge: "#a05577", // the pink darker in oklch, mixing in black went grey
    head: "#141319", bezel: "#000000", // not quite black or the top vanishes into the page
    dark: "#141319", light: "#faf8ff", edge: "#141319",
    // the page's softened 2nd and alpha (style.css), not CEmu's. dark text on the keys,
    // white didn't read on pastels
    second: "#9cc7f2", alpha: "#c6e38f",
    secondInk: "#10141a", alphaInk: "#12160a",
    // the little 2nd / alpha labels above the keys, the pastels vanish on a pastel body
    // so they're printed deeper like on the light coloured real ones
    secondLabel: "#073a7d", alphaLabel: "#1e6626",
    text: "#f4f2fa", ink: "#141319", title: "#ffffff",
};

// number block is taller and sits a bit lower, like the real one
function layout() {
    const keys = [];
    const add = ([name, label, second, alpha, kind = "dark"], x, y, w, h, radii) =>
        keys.push({ name, label, second, alpha, kind, x, y, w, h, radii });

    // CEmu's corner numbers are diameters, these are half of them
    GRAPH.forEach((k, c) => add([...k, "graph"], 18 + 27 * c, 14, 18, 9,
        [c === 0 ? 3 : 2, c === 4 ? 3 : 2, c === 4 ? 3 : 2, c === 0 ? 3 : 2])); // 4 cut them to a point
    TOP.forEach((row, r) => row.forEach((k, c) => add(k, 18 + 27 * c, 37 + 21 * r, 18, 12, [2, 2, 2, 2])));
    ROWS.forEach((row, r) => row.forEach((k, c) => {
        if (k[4] !== "num")
            return add(k, 18 + 27 * c, 79 + 21 * r, 18, 12, [2, 2, 2, 2]);
        // 7 9 0 (-) get the round corners
        const radii = [r === 3 && c === 1, r === 3 && c === 3, r === 6 && c === 3, r === 6 && c === 1]
            .map(big => big ? 4 : 2);
        add(k, 18 + 27 * c, 142 + 23 * (r - 3), 18, 14, radii);
    }));
    return keys;
}

// radii go tl tr br bl. the body and the screen keep the real thing's round corners
function roundPath({ x, y, w, h, radii }) {
    const [tl, tr, br, bl] = radii.map(r => Math.min(r, w / 2, h / 2));
    return `M${x + tl},${y} H${x + w - tr} A${tr},${tr} 0 0 1 ${x + w},${y + tr}`
        + ` V${y + h - br} A${br},${br} 0 0 1 ${x + w - br},${y + h}`
        + ` H${x + bl} A${bl},${bl} 0 0 1 ${x},${y + h - bl}`
        + ` V${y + tl} A${tl},${tl} 0 0 1 ${x + tl},${y} Z`;
}

// the keys get cut corners like the rest of the page (frames.py)
// instead of CEmu's round ones, each radius is how big that cut is, same little bevel
function capPath({ x, y, w, h, radii }) {
    const pts = [];
    [[x, y, 1, 1], [x + w, y, -1, 1], [x + w, y + h, -1, -1], [x, y + h, 1, -1]].forEach(([ox, oy, sx, sy], i) => {
        const c = Math.min(radii[i], w / 2, h / 2), d = c * .1, e = d * .7071;
        const corner = [[0, c + d], [e, c - e], [c - e, e], [c + d, 0]].map(([a, b]) => [ox + sx * a, oy + sy * b]);
        pts.push(...(sx * sy > 0 ? corner : corner.reverse()));
    });
    return "M" + pts.map(([a, b]) => `${+a.toFixed(3)},${+b.toFixed(3)}`).join(" L") + " Z";
}

// an octagon cut in four round a square hole, flat sides facing the arrows. was round,
// but it was the one round thing left on a pad of cut corner keys
const PAD = { cx: 121, cy: 53, outer: 17.5, inner: 6.5, gap: 2, round: .6 };
const ARROWS = [["right", 0], ["down", 90], ["left", 180], ["up", 270]];

function arrowPath(angle) {
    const { cx, cy, outer, inner, gap } = PAD;
    const dir = deg => [Math.cos(deg * Math.PI / 180), Math.sin(deg * Math.PI / 180)];
    const vert = (r, deg) => dir(deg).map(v => v * r); // octagon corners are at 22.5 + 45k
    const cross = (u, p) => u[0] * p[1] - u[1] * p[0];
    // where the octagon side p -> q crosses the line `gap` off the divider at `deg`, so the
    // gap's the same width inside and out
    const cut = (p, q, deg, off) => {
        const u = dir(deg), t = (off - cross(u, p)) / cross(u, [q[0] - p[0], q[1] - p[1]]);
        return [p[0] + t * (q[0] - p[0]), p[1] + t * (q[1] - p[1])];
    };
    const out = [
        cut(vert(outer, angle - 67.5), vert(outer, angle - 22.5), angle - 45, gap),
        vert(outer, angle - 22.5), vert(outer, angle + 22.5),
        cut(vert(outer, angle + 22.5), vert(outer, angle + 67.5), angle + 45, -gap),
    ];
    // the hole's a square, its corners sit on the dividers so each bit gets one side of it
    const a = vert(inner * Math.SQRT2, angle - 45), b = vert(inner * Math.SQRT2, angle + 45);
    const pts = [...out, cut(a, b, angle + 45, -gap), cut(a, b, angle - 45, gap)]
        .map(([x, y]) => [cx + x, cy + y]);
    // every corner eased off a touch, start a bit back along one side and curve round
    // through the corner to a bit along the next
    const f = ([x, y]) => `${x.toFixed(2)},${y.toFixed(2)}`;
    const toward = (p, q) => {
        const d = Math.hypot(q[0] - p[0], q[1] - p[1]), r = Math.min(PAD.round, d / 2);
        return [p[0] + (q[0] - p[0]) * r / d, p[1] + (q[1] - p[1]) * r / d];
    };
    return pts.map((p, i) => {
        const prev = pts[(i + pts.length - 1) % pts.length], next = pts[(i + 1) % pts.length];
        return `${i ? "L" : "M"}${f(toward(p, prev))} Q${f(p)} ${f(toward(p, next))}`;
    }).join(" ") + " Z";
}

function arrowHead(angle) {
    const { cx, cy } = PAD;
    const a = angle * Math.PI / 180, px = -Math.sin(a), py = Math.cos(a);
    const pt = (r, side) => `${(cx + r * Math.cos(a) + side * px).toFixed(2)},${(cy + r * Math.sin(a) + side * py).toFixed(2)}`;
    return `M${pt(15, 0)} L${pt(11.5, 2.2)} L${pt(11.5, -2.2)} Z`;
}

const NS = "http://www.w3.org/2000/svg";

function el(tag, attrs, parent) {
    const e = document.createElementNS(NS, tag);
    for (const [k, v] of Object.entries(attrs))
        e.setAttribute(k, v);
    parent?.append(e);
    return e;
}

const CAP = { graph: "light", dark: "dark", light: "light", num: "light", second: "second", alpha: "alpha" };
const INK = { graph: "ink", light: "ink", num: "ink", second: "secondInk", alpha: "alphaInk" };

const STYLE = `
.key { cursor: pointer; }
/* the page backwards, black outlines on pastel. the white and 2nd keys got lost on the lilac */
.cap { stroke: ${COLOURS.edge}; stroke-width: .5; }
.key .shine { fill: var(--keypad-lit, #8ab4ff); opacity: 0; transition: opacity .3s; }
.key.lit .shine { opacity: .5; transition: none; }
.key.lit { transform: translateY(.6px); }
text { font-family: "Archivo Narrow", "Arial Narrow", "Liberation Sans Narrow", sans-serif;
       font-weight: 700; dominant-baseline: central; pointer-events: none; user-select: none; }
.title { font-family: Arial, Helvetica, "Liberation Sans", sans-serif; }
`;

// the screen half, measured off a CEmu screenshot since that bit's a skin image, not code
const HEAD = 129;
const SCREEN = { x: 22.5, y: 28.35, width: 117.8, height: 88.35 };

function labelSize(k) {
    if (k.kind === "num") return 7;
    if (k.kind === "graph") return 4.4;
    return k.label.length <= 2 ? 6 : k.label.length <= 5 ? 5 : 4.2;
}

// onpress(name, { on, second, alpha }) with whatever's held, holding works like keys_ti.c
export function calculator(parent, onpress) {
    const height = HEAD + 238;
    const svg = el("svg", { viewBox: `0 0 162 ${height}`, class: "calc", style: "touch-action: none" }, parent);
    el("style", {}, svg).textContent = STYLE;

    // all flat, no plastic shading, like the page. used to be gradients and gloss here
    el("path", { d: roundPath({ x: 0, y: 0, w: 162, h: height, radii: [21, 21, 20, 20] }), fill: COLOURS.body }, svg);
    el("path", { d: roundPath({ x: 0, y: 0, w: 162, h: HEAD, radii: [21, 21, 0, 0] }), fill: COLOURS.head }, svg);
    // a darker pink edge all the way round like the boxes on the page. .7 is about 2px at
    // the size it shows, pulled in half of that so it doesnt get clipped
    el("path", { d: roundPath({ x: .35, y: .35, w: 161.3, h: height - .7, radii: [20.65, 20.65, 19.65, 19.65] }),
                 fill: "none", stroke: COLOURS.bodyEdge, "stroke-width": .7 }, svg);

    const title = el("text", { x: 81, y: 10.5, "font-size": 7.2, "text-anchor": "middle", fill: COLOURS.title, class: "title" }, svg);
    el("tspan", {}, title).textContent = "TI-84 ";
    el("tspan", { "font-weight": 400 }, title).textContent = "Plus CE";
    const bezel = roundPath({ x: 10, y: 21, w: 142, h: 103.4, radii: [10.3, 10.3, 10.3, 10.3] });
    el("path", { d: bezel, fill: COLOURS.bezel }, svg);
    el("rect", { ...SCREEN, fill: "#000" }, svg);
    const pad = el("g", { transform: `translate(0 ${HEAD})` }, svg);

    const keys = {};
    // down then up with nothing pressed in between is a tap, the app makes those sticky
    const mods = { on: {}, second: {}, alpha: {} };
    const MOD = { on: "on", "2nd": "second", alpha: "alpha" };
    const REPEATS = new Set(["up", "down", "left", "right", "clear", "del"]);

    // also what the autoplay uses to show what it's pressing
    function light(name, ms = 150) {
        const k = keys[name];
        if (!k) return;
        k.g.classList.add("lit");
        clearTimeout(k.unlit);
        k.unlit = setTimeout(() => k.holds || k.g.classList.remove("lit"), ms);
    }

    function press(name) {
        const k = keys[name];
        if (k.holds++) return; // another finger's already on it
        k.g.classList.add("lit");
        const mod = mods[MOD[name]];
        if (mod) {
            mod.down = true;
            mod.used = false;
            return;
        }
        const held = {};
        for (const [m, state] of Object.entries(mods)) {
            held[m] = !!state.down;
            state.used ||= held[m];
        }
        onpress(name, held);
        // same timing as the calc, 250 ms then 35 a second
        if (REPEATS.has(name))
            k.repeat = setTimeout(function again() {
                onpress(name, held);
                k.repeat = setTimeout(again, 1000 / 35);
            }, 250);
    }

    function release(name) {
        const k = keys[name];
        if (--k.holds) return;
        k.g.classList.remove("lit");
        clearTimeout(k.repeat);
        const mod = mods[MOD[name]];
        if (mod?.down && !mod.used) onpress(name, {});
        if (mod) mod.down = false;
    }

    // whatever key each finger (or the mouse) is on. slide off and it lets go, slide onto one
    // and it's pressed, like CEmu. by position, since touches stick to where they started
    const pointers = new Map();
    const keyAt = e => {
        const g = document.elementFromPoint(e.clientX, e.clientY)?.closest(".key");
        return g && svg.contains(g) ? g.dataset.key : null;
    };
    function track(id, now) {
        const was = pointers.get(id);
        if (now === was) return;
        pointers.set(id, now);
        if (was) release(was);
        if (now) press(now);
    }
    svg.addEventListener("pointerdown", e => {
        e.preventDefault();
        pointers.set(e.pointerId, null);
        track(e.pointerId, keyAt(e));
    });
    addEventListener("pointermove", e => pointers.has(e.pointerId) && track(e.pointerId, keyAt(e)));
    for (const type of ["pointerup", "pointercancel"])
        addEventListener(type, e => {
            if (!pointers.has(e.pointerId)) return;
            track(e.pointerId, null);
            pointers.delete(e.pointerId);
        });

    const hook = (g, name) => keys[name] = { g, holds: 0 };

    for (const k of layout()) {
        const g = el("g", { class: `key ${k.kind}`, "data-key": k.name }, pad);
        const d = capPath(k);
        el("path", { d, fill: COLOURS[CAP[k.kind]], class: "cap" }, g);
        el("path", { d, class: "shine" }, g);
        const label = el("text", { x: k.x + k.w / 2, y: k.y + k.h / 2, "font-size": labelSize(k), "text-anchor": "middle",
                                   fill: COLOURS[INK[k.kind] ?? "text"] }, g);
        label.textContent = k.label;

        // 2nd on the left, alpha on the right, or 2nd in the middle if it's alone
        const above = (str, x, anchor, colour) => {
            if (str) el("text", { x, y: k.y - 3, "font-size": 4.3, "text-anchor": anchor, fill: colour }, pad).textContent = str;
        };
        if (k.alpha) {
            above(k.second, k.x - 1, "start", COLOURS.secondLabel);
            above(k.alpha, k.x + k.w + 1, "end", COLOURS.alphaLabel);
        } else {
            above(k.second, k.x + k.w / 2, "middle", COLOURS.secondLabel);
        }
        hook(g, k.name);
    }

    for (const [name, angle] of ARROWS) {
        const g = el("g", { class: "key arrow", "data-key": name }, pad);
        const d = arrowPath(angle);
        el("path", { d, fill: COLOURS.light, class: "cap" }, g);
        el("path", { d, class: "shine" }, g);
        el("path", { d: arrowHead(angle), fill: COLOURS.ink }, g);
        hook(g, name);
    }

    // screen is in viewBox units, the page puts fluxion's canvas over it
    return { svg, light, screen: { ...SCREEN, of: { width: 162, height } } };
}
