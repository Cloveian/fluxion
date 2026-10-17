// the autoplay. types problems on the keypad like someone would, sits on the answer for a bit,
// then the next one. the problems are in demo.txt, same key names as web/wall/scripts.txt,
// alpha+ means alpha's held down for that key

// a lone lowercase letter gets the alpha key it's printed above (x has its own)
const ALPHA = {
    a: "math", b: "apps", c: "prgm", d: "x^-1", e: "sin", f: "cos", g: "tan", h: "^",
    i: "x^2", j: ",", k: "(", l: ")", m: "/", n: "log", o: "7", p: "8", q: "9", r: "*",
    s: "ln", t: "4", u: "5", v: "6", w: "-", y: "1", z: "2",
};
const HELD = { "on+": "on", "2nd+": "second", "alpha+": "alpha" };
const KEY_OF = { on: "on", second: "2nd", alpha: "alpha" };

export async function load(url = "demo.txt") {
    const text = await (await fetch(url)).text();
    return text.split("\n").filter(l => l.trim() && !l.startsWith("#")).map(l => l.slice(l.indexOf(":") + 1).trim());
}

function presses(script) {
    return script.split(/\s+/).map(token => {
        if (token === "x") return { name: "xt", held: {} };
        if (ALPHA[token]) return { name: ALPHA[token], held: { alpha: true } };
        const held = {};
        for (let more = true; more;) {
            more = false;
            for (const [prefix, mod] of Object.entries(HELD))
                if (token.startsWith(prefix) && token.length > prefix.length) {
                    held[mod] = true;
                    token = token.slice(prefix.length);
                    more = true;
                }
        }
        return { name: token, held };
    });
}

const sleep = ms => new Promise(done => setTimeout(done, ms));
const between = (lo, hi) => lo + Math.random() * (hi - lo);

export function autoplay(fx, problems) {
    let run = 0;

    // types one script, false if something else took over halfway
    async function type(script, id) {
        const keys = presses(script);
        for (const [i, { name, held }] of keys.entries()) {
            if (id !== run) return false;
            if (name === "enter") await sleep(600); // a beat to see the whole thing typed
            const gap = name === "alpha" || name === "2nd" ? between(110, 160) : between(150, 280);
            fx.press(name, held);
            fx.light(name, 140);
            // held ones stay lit until the next key, so a run of alpha+ looks like one long hold
            for (const mod of Object.keys(held))
                fx.light(KEY_OF[mod], keys[i + 1]?.held[mod] ? gap + 400 : 140);
            await sleep(gap);
        }
        await sleep(2900);
        return id === run;
    }

    // [on][clear] clears the screen and keeps the history, like it would for you
    async function clear() {
        fx.press("clear", { on: true });
        fx.light("on", 160);
        fx.light("clear", 140);
        await sleep(500);
    }

    async function play(id) {
        for (let n = 0; id === run; n++) {
            if (!await type(problems[n % problems.length], id)) return;
            if (n % 3 === 2) await clear();
        }
    }

    return {
        start() { play(++run); },
        stop() { run++; },
        // one script right now (a feature someone clicked), and it stays on the answer after
        async show(script) {
            const id = ++run;
            fx.press("clear"); // whatever was half typed
            await clear();
            await type(script, id);
        },
    };
}
