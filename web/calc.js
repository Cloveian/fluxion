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

// the black one
const COLOURS = {
    sides: "#3b3b3b", centre: "#191919",
    dark: "#1d1d1d", light: "#eeeeee",
    second: "#93c3f3", alpha: "#a0ca1e",
    // the 2nd and alpha keys themselves, a bit darker than CEmu's so their white text reads.
    // the little labels above the keys keep the bright ones, they're on black
    secondKey: "#6fa3d8", alphaKey: "#84a816",
    text: "#eeeeee", ink: "#191919",
};

// number block is taller and sits a bit lower, like the real one
function layout() {
    const keys = [];
    const add = ([name, label, second, alpha, kind = "dark"], x, y, w, h, radii) =>
        keys.push({ name, label, second, alpha, kind, x, y, w, h, radii });

    // CEmu's corner numbers are diameters, these are half of them
    GRAPH.forEach((k, c) => add([...k, "graph"], 18 + 27 * c, 14, 18, 9,
        [c === 0 ? 4 : 2, c === 4 ? 4 : 2, c === 4 ? 4 : 2, c === 0 ? 4 : 2]));
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

// radii go tl tr br bl
function capPath({ x, y, w, h, radii }) {
    const [tl, tr, br, bl] = radii.map(r => Math.min(r, w / 2, h / 2));
    return `M${x + tl},${y} H${x + w - tr} A${tr},${tr} 0 0 1 ${x + w},${y + tr}`
        + ` V${y + h - br} A${br},${br} 0 0 1 ${x + w - br},${y + h}`
        + ` H${x + bl} A${bl},${bl} 0 0 1 ${x},${y + h - bl}`
        + ` V${y + tl} A${tl},${tl} 0 0 1 ${x + tl},${y} Z`;
}

// a ring cut in four
const PAD = { cx: 121, cy: 53, outer: 17.5, inner: 8, gap: 2 };
const ARROWS = [["right", 0], ["down", 90], ["left", 180], ["up", 270]];

function arrowPath(angle) {
    const { cx, cy, outer, inner, gap } = PAD;
    const at = (r, deg) => {
        const a = deg * Math.PI / 180;
        return `${(cx + r * Math.cos(a)).toFixed(2)},${(cy + r * Math.sin(a)).toFixed(2)}`;
    };
    // so the gap is the same width inside and out
    const half = r => 45 - Math.asin(gap / r) * 180 / Math.PI;
    const ho = half(outer), hi = half(inner);
    return `M${at(outer, angle - ho)} A${outer},${outer} 0 0 1 ${at(outer, angle + ho)}`
        + ` L${at(inner, angle + hi)} A${inner},${inner} 0 0 0 ${at(inner, angle - hi)} Z`;
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

const CAP = { graph: "light", dark: "dark", light: "light", num: "light", second: "secondKey", alpha: "alphaKey" };
const INK = { graph: "ink", light: "ink", num: "ink" };

const STYLE = `
.key { cursor: pointer; }
.cap { stroke: #808080; stroke-width: .375; }
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
    const defs = el("defs", {}, svg);
    // stops are [offset, colour, opacity]. across by default, [0, 0, 0, 1] is top to bottom
    const gradient = (id, stops, [x1, y1, x2, y2] = [0, 0, 1, 0]) => {
        const g = el("linearGradient", { id, x1, y1, x2, y2 }, defs);
        for (const [offset, colour, opacity = 1] of stops)
            el("stop", { offset, "stop-color": colour, "stop-opacity": opacity }, g);
    };
    gradient("calc-face", [[0, COLOURS.sides], [.18, COLOURS.centre], [.82, COLOURS.centre], [1, COLOURS.sides]]);
    // the screen half is shinier black plastic than the keys, and the bezel's shinier again
    gradient("calc-head", [[0, "#242424"], [.18, "#0e0e0e"], [.82, "#0e0e0e"], [1, "#242424"]]);
    gradient("calc-gloss", [[0, "#fff", .13], [.3, "#fff", .03], [1, "#fff", 0]], [0, 0, 0, 1]);
    gradient("calc-bezel", [[0, "#232323"], [1, "#0a0a0a"]], [0, 0, 0, 1]);
    gradient("calc-shine", [[0, "#fff", .2], [.42, "#fff", .05], [.43, "#fff", 0]], [0, 0, 1, 1]);
    // svg text doesn't take css text-shadow everywhere, so the 2nd / alpha labels get this
    el("feDropShadow", { dx: 0, dy: .3, stdDeviation: .25, "flood-color": "#000", "flood-opacity": .45 },
        el("filter", { id: "calc-text-shadow" }, defs));

    el("path", { d: capPath({ x: 0, y: 0, w: 162, h: height, radii: [21, 21, 20, 20] }), fill: "url(#calc-face)" }, svg);
    const head = capPath({ x: 0, y: 0, w: 162, h: HEAD, radii: [21, 21, 0, 0] });
    el("path", { d: head, fill: "url(#calc-head)" }, svg);
    el("path", { d: head, fill: "url(#calc-gloss)" }, svg);

    const title = el("text", { x: 81, y: 10.5, "font-size": 7.2, "text-anchor": "middle", fill: "#fff", class: "title" }, svg);
    el("tspan", {}, title).textContent = "TI-84 ";
    el("tspan", { "font-weight": 400 }, title).textContent = "Plus CE";
    const bezel = { x: 10, y: 21, width: 142, height: 103.4, rx: 10.3 };
    el("rect", { ...bezel, fill: "url(#calc-bezel)", stroke: "#050505", "stroke-width": .6 }, svg);
    el("rect", { ...bezel, fill: "url(#calc-shine)" }, svg);
    el("rect", { ...SCREEN, fill: "#000", stroke: "#08080b", "stroke-width": .7 }, svg);
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
        if (k.kind === "second" || k.kind === "alpha")
            label.setAttribute("filter", "url(#calc-text-shadow)");

        // 2nd on the left, alpha on the right, or 2nd in the middle if it's alone
        const above = (str, x, anchor, colour) => {
            if (str) el("text", { x, y: k.y - 3, "font-size": 3.8, "text-anchor": anchor, fill: colour }, pad).textContent = str;
        };
        if (k.alpha) {
            above(k.second, k.x - 1, "start", COLOURS.second);
            above(k.alpha, k.x + k.w + 1, "end", COLOURS.alpha);
        } else {
            above(k.second, k.x + k.w / 2, "middle", COLOURS.second);
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
