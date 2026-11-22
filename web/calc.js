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

// the nspire, drawn off the real CX II (a black body with blue rails, grey number keys, the keys in
// pairs) and with the key order and printed labels from Firebird's keypad, which has the same
// ones. sizes and spots are my own measuring, no code or images from either (Firebird's GPL).
// same 162 x 367 as the CE so the page's sizing doesn't care which one's showing
// [name (web.c), label, printed above, icon]. the letters are keys of their own, no alpha
const NSPIRE_LEFT = [
    [["=", "=", "|≠≥▸"], ["trig", "trig", "(?)"]],
    [["^", "^", "ⁿ√x"], ["x^2", "x²", "√"]],
    [["e^x", "eˣ", "ln"], ["10^x", "10ˣ", "log"]],
    [["(", "(", "[ ]"], [")", ")", "{ }"]],
];
const NSPIRE_RIGHT = [
    [["template", "", ":=", "templates"], ["cat", "", "∞β°", "book"]],
    [["*", "×", "\"□\""], ["/", "÷", "@fraction"]],
    [["+", "+", "◐▸"], ["-", "−", "◂◐"]],
];
const NSPIRE_NUMBERS = [
    [["7"], ["8"], ["9"]],
    [["4"], ["5"], ["6"]],
    [["1"], ["2"], ["3"]],
    [["0"], [".", ".", "capture"], ["neg", "(−)", "ans"]],
];
const NSPIRE_LETTERS = [
    [["ee", "EE"], ..."abcdefg".split("").map(c => [c, c.toUpperCase()]), ["?!", "?!▸"]],
    [["pi", "π▸"], ..."hijklmn".split("").map(c => [c, c.toUpperCase()]), ["flag", "⚑"]],
    [[",", ","], ..."opqrstu".split("").map(c => [c, c.toUpperCase()]), ["return", "↵"]],
    [null, ..."vwxyz".split("").map(c => [c, c.toUpperCase()]), ["space", "space", "", 2]],
];
// where the touchpad sits, a rounded square with the click in the middle
const NSPIRE_PAD = { cx: 81, cy: 36.3, w: 61.2, h: 52.8, cut: 9, click: 18 };

function nspireLayout() {
    const keys = [];
    const add = ([name, label = name, above = "", icon], kind, x, y, w, h, radii = 2, size, squish) => {
        const r = Array.isArray(radii) ? radii : [radii, radii, radii, radii];
        keys.push({ name, label, second: above, alpha: "", icon, squish, kind, x, y, w, h, radii: r,
                    size: size ?? (kind === "num" ? 8.2 : kind === "letter" ? 5 : 5.4) });
    };
    // the ctrl row, then four rows of pairs and numbers
    const ROWS = [78, 95, 113.5, 132, 150.7];
    const W = 17.4, HALF = 12.65;

    [["esc"], ["pad", "", "save", "calculator"], ["tab"]].forEach((k, i) => add(k, "dark", 18.5, 10.6 + 20.2 * i, W, 10.4));
    [["home", "⌂on"], ["doc", "doc ▾", "+page"], ["menu"]].forEach((k, i) => add(k, "dark", 125.9, 10.6 + 20.2 * i, W, 10.4));
    const { cx, cy, click } = NSPIRE_PAD;
    add(["click", ""], "click", cx - click / 2, cy - click / 2, click, click, 4);

    add(["ctrl"], "second", 18.5, ROWS[0], W, 10);
    add(["shift", "⇧shift", "CAPS"], "dark", 49.75, ROWS[0], 17.3, 10, 2, undefined, .86); // fills the key otherwisse
    add(["vars", "var", "sto→"], "dark", 94.95, ROWS[0], 17.3, 10);
    add(["del", "del", "clear"], "dark", 125.9, ROWS[0], W, 10);

    // a pair is one pill with a line down the middle, so the two halves touch
    const pair = (cells, x, row, kind) => cells.forEach((k, c) =>
        add(k, kind, x + HALF * c, ROWS[row], HALF, 10.5, c ? [0, 2, 2, 0] : [2, 0, 0, 2]));
    NSPIRE_LEFT.forEach((cells, r) => pair(cells, 18.5, r + 1, "dark"));
    NSPIRE_RIGHT.forEach((cells, r) => pair(cells, 118.2, r + 1, "dark"));
    add(["enter", "enter", "≈"], "dark", 118.6, ROWS[4], 24.9, 10.5, 2, 5.8);
    NSPIRE_NUMBERS.forEach((cells, r) => cells.forEach((k, c) =>
        add(k, "num", 49.75 + 22.6 * c, ROWS[r + 1], 17.3, 10.5, 2, r === 3 && c ? 6.4 : undefined)));

    // nine columns, the ends a bit wider
    const COLS = [24.2, 39.6, 53.4, 67.2, 81, 94.8, 108.6, 122.4, 137.8];
    NSPIRE_LETTERS.forEach((cells, r) => cells.forEach((k, c) => {
        if (!k) return;
        const end = c === 0 || c === 8, w = k[3] ? 22.2 : end ? 11 : 8.4;
        const mid = k[3] ? (COLS[c] + COLS[c + 1]) / 2 : COLS[c];
        add(k.slice(0, 3), "letter", mid - w / 2, 172 + 13.4 * r, w, 8.6, 2);
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

// per calculator. body is everything behind the keys, touchpad the arrows or pad, look a key's colours and label size
const MODELS = {
    ce: {
        body: ceBody, touchpad: ceTouchpad, look: ceLook,
        layout,
        modifiers: { on: "on", "2nd": "second", alpha: "alpha" },
        repeats: ["up", "down", "left", "right", "clear", "del"],
    },
    nspire: {
        body: nspireBody, touchpad: nspireTouchpad, look: nspireLook,
        layout: nspireLayout,
        modifiers: { ctrl: "ctrl", shift: "shift" },
        repeats: ["up", "down", "left", "right", "del"],
    },
};

function ceBody(svg, height) {
    // all flat, no plastic shading, like the page
    el("path", { d: roundPath({ x: 0, y: 0, w: 162, h: height, radii: [21, 21, 20, 20] }), fill: COLOURS.body }, svg);
    el("path", { d: roundPath({ x: 0, y: 0, w: 162, h: HEAD, radii: [21, 21, 0, 0] }), fill: COLOURS.head }, svg);
    // a darker pink edge all the way round like the boxes on the page. .7 is about 2px at
    // the size it shows, pulled in half of that so it doesnt get clipped
    el("path", { d: roundPath({ x: .35, y: .35, w: 161.3, h: height - .7, radii: [20.65, 20.65, 19.65, 19.65] }),
                 fill: "none", stroke: COLOURS.bodyEdge, "stroke-width": .7 }, svg);

    const title = el("text", { x: 81, y: 10.5, "font-size": 7.2, "text-anchor": "middle", fill: COLOURS.title, class: "title" }, svg);
    el("tspan", {}, title).textContent = "TI-84 ";
    el("tspan", { "font-weight": 400 }, title).textContent = "Plus CE";
    el("path", { d: roundPath({ x: 10, y: 21, w: 142, h: 103.4, radii: [10.3, 10.3, 10.3, 10.3] }), fill: COLOURS.bezel }, svg);
    el("rect", { ...SCREEN, fill: "#000" }, svg);
}

function ceTouchpad(pad, hook) {
    for (const [name, angle] of ARROWS) {
        const g = el("g", { class: "key arrow", "data-key": name }, pad);
        const d = arrowPath(angle);
        el("path", { d, fill: COLOURS.light, class: "cap" }, g);
        el("path", { d, class: "shine" }, g);
        el("path", { d: arrowHead(angle), fill: COLOURS.ink }, g);
        hook(g, name);
    }
}

function ceLook(k) {
    return { fill: COLOURS[CAP[k.kind]], ink: COLOURS[INK[k.kind] ?? "text"], size: labelSize(k),
             second: COLOURS.secondLabel, alpha: COLOURS.alphaLabel };
}

// the real one's black with blue rails down both sides, in the page's own blue and greys
const NSPIRE = {
    black: COLOURS.head, panel: "#1b1a22", edge: "#2f2d3a",
    rail: COLOURS.second, railEdge: "#5d8fbf", // the blue darker, like the CE's pink edge
    ring: "#9a96a8",
};

function nspireBody(svg, height) {
    const body = roundPath({ x: 0, y: 0, w: 162, h: height, radii: [2.5, 2.5, 26, 26] });
    const clip = el("clipPath", { id: "nspire-body" }, el("defs", {}, svg));
    el("path", { d: body }, clip);
    el("path", { d: body, fill: NSPIRE.black }, svg);
    // the key half's a shade lighter, the touchpad's black above it
    const top = HEAD + 69.5;
    el("rect", { x: 0, y: top, width: 162, height: height - top, fill: NSPIRE.panel, "clip-path": "url(#nspire-body)" }, svg);
    el("rect", { x: 11.5, y: top - .25, width: 139, height: .5, fill: NSPIRE.edge }, svg);
    // the rails go the whole way, edge to edge and top to bottom, cut to the body's corners
    for (const x of [0, 150.5])
        el("rect", { x, y: 0, width: 11.5, height, fill: NSPIRE.rail, "clip-path": "url(#nspire-body)" }, svg);
    for (const x of [11.5, 150.5])
        el("rect", { x: x - .25, y: 0, width: .5, height, fill: NSPIRE.railEdge, "clip-path": "url(#nspire-body)" }, svg);
    el("path", { d: roundPath({ x: .3, y: .3, w: 161.4, h: height - .6, radii: [2.2, 2.2, 25.7, 25.7] }),
                 fill: "none", stroke: NSPIRE.edge, "stroke-width": .6 }, svg);

    const title = el("text", { x: 80, y: 11.5, "font-size": 8.4, "text-anchor": "middle", fill: COLOURS.second, class: "title" }, svg);
    el("tspan", {}, title).textContent = "TI-";
    el("tspan", { "font-style": "italic", "font-weight": 600 }, title).textContent = "nspire";
    el("tspan", {}, title).textContent = " CX II";
    el("text", { x: 140, y: 11.5, "font-size": 7.4, "text-anchor": "end", fill: COLOURS.light, class: "title", style: "font-weight:700" }, svg).textContent = "CAS";
    // no bezel, the screen goes straight into the body. a hairline keeps its edge on a dark theme
    el("rect", { ...SCREEN, fill: "#000", stroke: NSPIRE.edge, "stroke-width": .4 }, svg);
}

// one rounded square to push, which way depends on where. four zones cut off by the diagnals
// and the click on top of them (it's in the layout, drawn after)
function nspireTouchpad(pad, hook) {
    const { cx, cy, w, h, cut } = NSPIRE_PAD;
    const x = cx - w / 2, y = cy - h / 2;
    const outer = capPath({ x, y, w, h, radii: [cut, cut, cut, cut] });
    const clip = el("clipPath", { id: "nspire-pad" }, el("defs", {}, pad));
    el("path", { d: outer }, clip);
    el("path", { d: outer, fill: "#0b0a0f", stroke: NSPIRE.ring, "stroke-width": 1.5 }, pad);

    const corners = { tl: [x, y], tr: [x + w, y], bl: [x, y + h], br: [x + w, y + h] };
    const zones = { up: ["tl", "tr"], down: ["bl", "br"], left: ["tl", "bl"], right: ["tr", "br"] };
    for (const [name, [a, b]] of Object.entries(zones)) {
        const g = el("g", { class: "key pad", "data-key": name, "clip-path": "url(#nspire-pad)" }, pad);
        const d = `M${cx},${cy} L${corners[a].join(",")} L${corners[b].join(",")} Z`;
        el("path", { d, fill: "transparent" }, g);
        el("path", { d, class: "shine" }, g);
        hook(g, name);
    }
    // the little arrows printed on it
    const arrow = (px, py, deg) => el("path", { d: "M0,-1.7 L1.5,1.1 L-1.5,1.1 Z", fill: NSPIRE.ring,
        transform: `translate(${px} ${py}) rotate(${deg})`, "pointer-events": "none" }, pad);
    arrow(cx, y + 6, 0);
    arrow(cx, y + h - 6, 180);
    arrow(x + 6, cy, 270);
    arrow(x + w - 6, cy, 90);
}

function nspireLook(k) {
    const look = {
        dark: { fill: "#26242f", stroke: "#4a4757", ink: "#e6e2ee" },
        num: { fill: "#3d3b49", stroke: "#5e5a6e", ink: COLOURS.light },
        letter: { fill: "#201e28", stroke: "#3a3847", ink: "#b4b0c2" },
        second: { fill: COLOURS.second, stroke: "#c6defa", ink: COLOURS.secondInk }, // ctrl, the 2nd key's blue
        click: { fill: "#0f0e13", stroke: NSPIRE.ring, ink: COLOURS.light },
    }[k.kind];
    return { ...look, size: k.size, second: COLOURS.second };
}

// pictures on and over the keys, drawn small round a point. ICONS are outlines, FILLED the solid bits
const ICONS = {
    book: "M-4.2,-2.6 Q-2.1,-3.6 0,-2.4 Q2.1,-3.6 4.2,-2.6 V2.6 Q2.1,1.6 0,2.8 Q-2.1,1.6 -4.2,2.6 Z M0,-2.4 V2.8",
    fraction: "M-1.7,0 H1.7 M-1.1,.9 H1.1 V2.7 H-1.1 Z",
    templates: "M-4.16 -2 V2 M-3.04 -0.96 H-1.28 V0.96 H-3.04 Z M1.28 -2.08 Q0.32 -2.08 0.32 -1.12 V-0.4 Q0.32 0 -0.32 0 Q0.32 0 0.32 0.4 V1.12 Q0.32 2.08 1.28 2.08 M2.24 -0.96 H4.16 V0.96 H2.24 Z",
};
// the calculator's a solid with the screen and keys cut out of it, so these are drawn evenodd
const FILLED = {
    fraction: "M-1.35,-2.95 H1.35 V-.65 H-1.35 Z", // as big as the outlined box under it, stroke and all
    calculator: "M-1.9,-3.2 H1.9 Q2.7,-3.2 2.7,-2.4 V2.4 Q2.7,3.2 1.9,3.2 H-1.9 Q-2.7,3.2 -2.7,2.4 V-2.4 Q-2.7,-3.2 -1.9,-3.2 Z M-1.8,-2.5 H1.8 V-1.1 H-1.8 Z M-1.8,-0.6 h.8 v.8 h-.8 Z M-0.4,-0.6 h.8 v.8 h-.8 Z M1,-0.6 h.8 v.8 h-.8 Z M-1.8,0.6 h.8 v.8 h-.8 Z M-0.4,0.6 h.8 v.8 h-.8 Z M1,0.6 h.8 v.8 h-.8 Z M-1.8,1.8 h.8 v.8 h-.8 Z M-0.4,1.8 h.8 v.8 h-.8 Z M1,1.8 h.8 v.8 h-.8 Z",
};

function drawIcon(parent, name, transform, colour, width) {
    const g = el("g", { transform, fill: "none", stroke: colour, "stroke-width": width, "stroke-linejoin": "round",
                        "pointer-events": "none" }, parent);
    if (ICONS[name]) el("path", { d: ICONS[name] }, g);
    if (FILLED[name]) el("path", { d: FILLED[name], fill: colour, "fill-rule": "evenodd", stroke: "none" }, g);
}

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
.nspire text { font-family: "Noto Sans", "Open Sans", "DejaVu Sans", "Segoe UI", Arial, sans-serif; font-weight: 600; }
.nspire .title { font-weight: 400; }
`;

// the screen half, measured off a CEmu screenshot since that bit's a skin image, not code
const HEAD = 129;
const SCREEN = { x: 22.5, y: 28.35, width: 117.8, height: 88.35 };

function labelSize(k) {
    if (k.kind === "num") return 7;
    if (k.kind === "graph") return 4.4;
    return k.label.length <= 2 ? 6 : k.label.length <= 5 ? 5 : 4.2;
}

// onpress(name, held) with whatever's held ({ on, second, alpha } on the CE, { ctrl, shift } on the
// nspire), holding works like keys_ti.c
export function calculator(parent, onpress, model = "ce") {
    const spec = MODELS[model];
    const height = HEAD + 238;
    const svg = el("svg", { viewBox: `0 0 162 ${height}`, class: `calc ${model}`, style: "touch-action: none" }, parent);
    el("style", {}, svg).textContent = STYLE;

    spec.body(svg, height);
    const pad = el("g", { transform: `translate(0 ${HEAD})` }, svg);

    const keys = {};
    // down then up with nothing pressed in between is a tap, the app makes those sticky
    const MOD = spec.modifiers;
    const mods = Object.fromEntries(Object.values(MOD).map(m => [m, {}]));
    const REPEATS = new Set(spec.repeats);

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

    spec.touchpad(pad, hook);

    for (const k of spec.layout()) {
        const g = el("g", { class: `key ${k.kind}`, "data-key": k.name }, pad);
        const look = spec.look(k);
        const d = capPath(k);
        el("path", { d, fill: look.fill, class: "cap", ...(look.stroke && { style: `stroke:${look.stroke}` }) }, g);
        el("path", { d, class: "shine" }, g);
        if (k.label) {
            const cx = k.x + k.w / 2, cy = k.y + k.h / 2;
            el("text", { x: cx, y: cy, "font-size": look.size, "text-anchor": "middle", fill: look.ink,
                         ...(k.squish && { transform: `translate(${cx} ${cy}) scale(${k.squish} 1) translate(${-cx} ${-cy})` }) }, g)
                .textContent = k.label;
        }
        if (k.icon)
            drawIcon(g, k.icon, `translate(${k.x + k.w / 2} ${k.y + k.h / 2})`, look.ink, .55);

        // 2nd on the left, alpha on the right, or 2nd in the middle if it's alone
        const above = (str, x, anchor, colour) => {
            if (!str) return;
            if (str.startsWith("@")) // a picture instead of words
                drawIcon(pad, str.slice(1), `translate(${x} ${k.y - 3.2})`, colour, .5);
            else
                el("text", { x, y: k.y - 3, "font-size": 4.3, "text-anchor": anchor, fill: colour }, pad).textContent = str;
        };
        if (k.alpha) {
            above(k.second, k.x - 1, "start", look.second);
            above(k.alpha, k.x + k.w + 1, "end", look.alpha);
        } else {
            above(k.second, k.x + k.w / 2, "middle", look.second);
        }
        hook(g, k.name);
    }

    // screen is in viewBox units, the page puts fluxion's canvas over it
    return { svg, light, screen: { ...SCREEN, of: { width: 162, height } } };
}
