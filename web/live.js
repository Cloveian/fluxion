// fluxion running in the page, inside calc.js's calculator. the keypad, the keyboard and the
// autoplay all end up in web_press() (src/platform/web.c)
import createFluxion from "./build/fluxion.js";
import { calculator } from "./calc.js";

export async function live(parent, model = "ce") {
    const box = document.createElement("div");
    box.className = "live";
    box.style.position = "relative";
    parent.append(box);

    // fluxion's 320x240 goes in small. that gets blown up a whole number of times with no
    // smoothing into big, then smoothed down to the canvas's real device pixels. so every pixel
    // stays a sharp square at any size and only its edges blend (sharp bilinear, emulators do it)
    const small = document.createElement("canvas"), big = document.createElement("canvas");
    small.width = 320;
    small.height = 240;
    const image = small.getContext("2d").createImageData(320, 240);
    const pixels = new Uint32Array(image.data.buffer);
    const canvas = document.createElement("canvas");
    const ctx = canvas.getContext("2d");

    function paint() {
        const b = big.getContext("2d");
        b.imageSmoothingEnabled = false;
        b.drawImage(small, 0, 0, big.width, big.height);
        ctx.imageSmoothingEnabled = true;
        ctx.imageSmoothingQuality = "high";
        ctx.drawImage(big, 0, 0, canvas.width, canvas.height);
    }

    let mod = null, codes = {};
    function press(name, held = {}) {
        if (!mod) return; // still loading
        const key = codes[name] ??= mod.ccall("web_key", "number", ["string"], [name]);
        // the nspire's ctrl and shift go in as the first two, web.c knows which build it is
        if (key) mod._web_press(key, +!!(held.on ?? held.ctrl), +!!(held.second ?? held.shift), +!!held.alpha);
    }

    const calc = calculator(box, press, model);
    const s = calc.screen, pct = (a, b) => `${(a / b * 100).toFixed(3)}%`;
    Object.assign(canvas.style, {
        position: "absolute",
        left: pct(s.x, s.of.width), top: pct(s.y, s.of.height),
        width: pct(s.width, s.of.width), height: pct(s.height, s.of.height),
    });
    box.append(canvas);
    // the canvas is exactly as many pixels as it covers on screen, so the browser never
    // stretches it after us. device-pixel-content-box is exact, safari doesn't have it
    const fit = new ResizeObserver(([e]) => {
        const exact = e.devicePixelContentBoxSize?.[0];
        canvas.width = exact ? exact.inlineSize : Math.round(e.contentRect.width * devicePixelRatio);
        canvas.height = exact ? exact.blockSize : Math.round(e.contentRect.height * devicePixelRatio);
        const n = Math.max(1, Math.ceil(canvas.width / 320));
        big.width = 320 * n;
        big.height = 240 * n;
        paint();
    });
    try {
        fit.observe(canvas, { box: "device-pixel-content-box" });
    } catch {
        fit.observe(canvas);
    }

    // something on the screen while the wasm comes down, black looked broken on slow connections
    {
        const s = small.getContext("2d");
        s.fillStyle = "#181820"; // dark theme bg in ui/theme.c
        s.fillRect(0, 0, 320, 240);
        s.fillStyle = "#818188"; // and its dim
        s.font = "16px monospace";
        s.textAlign = "center";
        s.fillText("loading...", 160, 124);
        paint();
    }

    const create = model === "ce" ? createFluxion : (await import("./build/fluxion-nspire.js")).default;
    mod = await create({
        // called as Module.present, so this is the module. the first frame comes from main(),
        // before createFluxion has even resolved
        present(fb) {
            const fbuf = this.HEAPU8.subarray(fb, fb + 320 * 240);
            const lut = new Uint32Array(256), seen = new Uint8Array(256);
            for (let i = 0; i < fbuf.length; i++) {
                const c = fbuf[i];
                if (!seen[c]) {
                    const rgb = this._web_rgb(c); // asked every frame, the theme setting changes them
                    lut[c] = 0xff000000 | (rgb & 0xff) << 16 | (rgb & 0xff00) | (rgb >> 16 & 0xff);
                    seen[c] = 1;
                }
                pixels[i] = lut[c];
            }
            small.getContext("2d").putImageData(image, 0, 0);
            paint();
        },
    });

    // wipes the half typed line and the screen, the history stays (up arrow) like on the calc
    const wipe = () => {
        if (model === "ce") {
            press("clear");
            press("clear", { on: true });
        } else {
            for (let i = 0; i < 3; i++) press("esc"); // out of a page or a menu in a menu, ctrl+del doesn't
            press("del", { ctrl: true });
            press("del", { ctrl: true });
        }
    };

    return { press, light: calc.light, calc, canvas, box, model, wipe };
}

// the computer's keyboard, same layout as the PC build (keys_pc.c's header has the table).
// shift, alt and ctrl on their own are alpha, 2nd and on, sent when they're let go
const KEYS = {
    Enter: "enter", Backspace: "clear", Delete: "del", Tab: "2nd",
    ArrowUp: "up", ArrowDown: "down", ArrowLeft: "left", ArrowRight: "right",
    F1: "y=", F2: "window", F3: "zoom", F4: "trace", F5: "graph",
};
const REPEATS = new Set(["up", "down", "left", "right", "clear", "del"]);

// the nspire's: its keys are the computer's, letters and symbols type themselves. ctrl is ctrl,
// let go on its own it's a tap. shift is just shift (capitols and ( ^ need it), only with the arrows
// does it mean something, that's select
const NSPIRE_KEYS = {
    Enter: "enter", Backspace: "del", Delete: "del", Escape: "esc", Home: "home",
    ArrowUp: "up", ArrowDown: "down", ArrowLeft: "left", ArrowRight: "right",
};
const NSPIRE_REPEATS = new Set(["up", "down", "left", "right", "del"]);
// ctrl + these, anything else is the browser's (ctrl+r, ctrl+w ...)
const NSPIRE_CTRL = /^[zxcv17=+\-\/^]$/;

function nspireKeyboard(fx) {
    let alone = false; // ctrl's down with nothing else since

    function down(e) {
        if (e.key === "Control") {
            if (!e.repeat) alone = true;
            e.preventDefault();
            return;
        }
        alone = false;
        if (e.metaKey || e.altKey) return;
        const name = NSPIRE_KEYS[e.key] ?? (e.key.length === 1 ? e.key : null);
        if (!name || (e.repeat && !NSPIRE_REPEATS.has(name))) return;
        if (e.ctrlKey && name.length === 1 && !NSPIRE_CTRL.test(name)) return;
        e.preventDefault();
        fx.press(name, { ctrl: e.ctrlKey, shift: e.shiftKey && ["up", "down", "left", "right"].includes(name) });
        fx.light(name);
    }

    function up(e) {
        if (e.key !== "Control" || !alone) return;
        alone = false;
        fx.press("ctrl");
        fx.light("ctrl");
    }

    addEventListener("keydown", down);
    addEventListener("keyup", up);
    return () => {
        removeEventListener("keydown", down);
        removeEventListener("keyup", up);
    };
}

export function keyboard(fx) {
    if (fx.model === "nspire") return nspireKeyboard(fx);
    let alone = null; // the modifier that's down with nothing else since

    function send(name, held = {}) {
        fx.press(name, held);
        fx.light(name);
    }

    function down(e) {
        if (["Shift", "Alt", "Control"].includes(e.key)) {
            if (!e.repeat) alone = e.key;
            e.preventDefault();
            return;
        }
        alone = null;
        if (e.metaKey) return; // leave the browser's own shortcuts alone
        const held = { on: e.ctrlKey };
        let name = KEYS[e.key];
        if (e.key === "Escape") { // quit, [2nd][mode]
            send("2nd", held);
            name = "mode";
        } else if (!name && e.key === "=") {
            send("2nd", held); // = is [2nd][math] like on the TI
            name = "math";
        } else if (!name && e.key.length === 1) {
            name = e.key;
        }
        if (!name || (e.repeat && !REPEATS.has(name))) return;
        e.preventDefault();
        if (e.altKey && name !== "math") send("2nd", held);
        if (e.shiftKey && ["up", "down", "left", "right"].includes(name)) send("alpha", held); // selects
        send(name, held);
    }

    function up(e) {
        if (e.key !== alone) return;
        alone = null;
        send({ Shift: "alpha", Alt: "2nd", Control: "on" }[e.key]);
    }

    addEventListener("keydown", down);
    addEventListener("keyup", up);
    return () => {
        removeEventListener("keydown", down);
        removeEventListener("keyup", up);
    };
}
