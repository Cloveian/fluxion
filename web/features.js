// the "what it does" cards. names and yes / no come out of docs/features.md (make web copies
// it into build/), the little note and the keys to show each one out of feature-demos.txt.
// so editing the doc is editing the page

async function text(url) {
    try {
        const r = await fetch(url);
        return r.ok ? await r.text() : "";
    } catch {
        return "";
    }
}

// ### area headings make cards, "## the rest" is the facts table
function tables(md) {
    const cards = [], facts = [];
    let card = null, rest = false;
    for (const line of md.split("\n")) {
        if (line.startsWith("## ")) {
            rest = /^## the rest/i.test(line);
            card = null;
        } else if (line.startsWith("### ")) {
            cards.push(card = { title: line.slice(4).trim(), rows: [] });
        } else if (line.startsWith("|") && !/^\|\s*-/.test(line)) {
            const [name, value] = line.split("|").slice(1, 3).map(c => c.trim());
            if (!name || /^feature$/i.test(name)) continue; // header rows
            if (rest) facts.push([name, value]);
            else card?.rows.push({ name, value });
        }
    }
    return { cards, facts };
}

// name | note | keys
function demos(txt) {
    const map = new Map();
    for (const line of txt.split("\n")) {
        if (!line.trim() || line.startsWith("#")) continue;
        const [name, note = "", keys = ""] = line.split("|").map(c => c.trim());
        map.set(name, { note, keys });
    }
    return map;
}

// *dih*rivatives keeps its italics, the rest is plain text
function label(el, name) {
    name.split(/\*([^*]+)\*/).forEach((part, i) => {
        if (i % 2) el.append(Object.assign(document.createElement("em"), { textContent: part }));
        else if (part) el.append(part);
    });
}

const tag = (name, cls, parent) => parent.appendChild(Object.assign(document.createElement(name), cls ? { className: cls } : {}));

// two columns, the cards split between them so they come out as close to the same height
// as they can (a folder's only a handful, so just try every split), each column still in
// the doc's order. the short side's cards stretch to make up the rest (style.css). one
// column when it's narrow. gives back show(cards) to swap which ones are up, for the folders
function stack(box, cards) {
    let cols = 0, shown = cards;
    const lay = again => {
        const n = box.clientWidth >= 2 * 280 + 16 ? 2 : 1;
        if (n === cols && !again) return;
        cols = n;
        box.replaceChildren();
        const columns = Array.from({ length: n }, () => tag("div", "column", box));
        // measured at the column width, all in the first one to start with
        columns[0].append(...shown);
        if (n === 2 && shown.length < 16) {
            const h = shown.map(c => c.offsetHeight + 16); // + the gap under each
            let best = 0, off = Infinity;
            // card 0 always left, so a split and its mirror only get tried once
            for (let mask = 0; mask < 1 << shown.length; mask += 2) {
                let d = 0;
                h.forEach((x, i) => d += mask >> i & 1 ? -x : x);
                if (Math.abs(d) < off) [best, off] = [mask, Math.abs(d)];
            }
            shown.forEach((c, i) => best >> i & 1 && columns[1].append(c));
        }
        tint(shown);
    };
    new ResizeObserver(() => lay()).observe(box);
    return list => { shown = list; lay(true); };
}

// the cards sorted into five folders under f1-f5, like the app's own soft keys. named
// after the app's folders where they fit (its templates folder's more the editor here).
// anything not listed ends up in the last one
const FOLDERS = [
    ["Algebra", ["Solving", "Simplifying", "Factoring", "Systems", "Fractions", "Roots, powers and logs"]],
    ["Calculus", ["Calculus", "Trig"]],
    ["Functions", ["Functions", "Geometry and lines", "Conics"]],
    ["Editor", ["The 2D editor", "History, copy and paste", "Menus and keys"]],
    ["Tools", ["Numbers", "Probability", "Variables", "Modes and settings", "On the calculator"]],
];
const folderOf = title => {
    const i = FOLDERS.findIndex(([, has]) => has.includes(title));
    return i < 0 ? FOLDERS.length - 1 : i;
};

// a pastel each, round the colour wheel. same list as TINTS in tools/frames.py
const TINTS = ["pink", "peach", "butter", "mint", "blue", "lilac"];

// which cards touch isnt known till they're dealt into columns, so colour after. a card
// can't have the colour of anything touching it (the one above, anything beside it), then
// it's whichever's been used least so they all get a go, furthest round the wheel if
// that's a tie. (ruling out the wheel neighbours too left it only 3 colours to pick from)
function tint(cards) {
    const away = (a, b) => Math.min(Math.abs(a - b), TINTS.length - Math.abs(a - b));
    const box = cards.map(c => c.getBoundingClientRect()), got = [], used = TINTS.map(() => 0);
    const order = cards.map((_, i) => i).sort((a, b) => box[a].top - box[b].top || box[a].left - box[b].left);
    for (const i of order) {
        const near = order.filter(j => got[j] !== undefined && box[j].top < box[i].bottom + 24 && box[i].top < box[j].bottom + 24)
            .map(j => got[j]);
        let best = 0, score = -Infinity;
        TINTS.forEach((_, t) => {
            const gap = Math.min(TINTS.length, ...near.map(n => away(n, t)));
            const s = (gap ? 1000 : 0) - used[t] * 10 + gap;
            if (s > score) [best, score] = [t, s];
        });
        got[i] = best;
        used[best]++;
        cards[i].classList.remove(...TINTS.map(t => `tint-${t}`));
        cards[i].classList.add(`tint-${TINTS[best]}`);
    }
}

// play(li, keys) is called when someone picks one
export async function cards(play) {
    const [{ cards, facts }, extra] = await Promise.all([
        text("build/features.md").then(tables),
        text("feature-demos.txt").then(demos),
    ]);
    const box = document.getElementById("cards"), nope = [];

    for (const c of cards) {
        const yes = c.rows.filter(r => !r.value.startsWith("no"));
        nope.push(...c.rows.filter(r => r.value.startsWith("no")).map(r => r.name));
        if (!yes.length) continue;
        const card = tag("div", "card", box);
        card.dataset.folder = folderOf(c.title);
        tag("h3", "", card).textContent = c.title;
        const list = tag("ul", "", card);
        yes.forEach((r, i) => {
            const li = tag("li", "", list), more = extra.get(r.name);
            // numbered like a TI menu, 1-9 then 0 then letters
            tag("span", "num", li).textContent = (i < 9 ? i + 1 : i === 9 ? 0 : String.fromCharCode(55 + i)) + ":";
            const name = tag("span", "name", li);
            label(name, r.name);
            // the yes* ones get a little ? with the limit in a tooltip. focusable so a tap shows it
            // on a phone, and it doesn't count as clicking the row
            if (more?.note) {
                const why = tag("span", "why", name); // in the name, so it can sit up like a footnote
                why.append(Object.assign(document.createElement("i"), { textContent: "?" })); // the badge, see style.css
                why.tabIndex = 0;
                why.dataset.tip = more.note;
                why.setAttribute("aria-label", more.note);
                why.onclick = e => e.stopPropagation();
                why.onkeydown = e => e.stopPropagation();
            }
            if (!more?.keys) return;
            li.classList.add("demo");
            li.tabIndex = 0;
            li.setAttribute("role", "button");
            li.onclick = () => play(li, more.keys);
            li.onkeydown = e => (e.key === "Enter" || e.key === " ") && (e.preventDefault(), play(li, more.keys));
        });
    }
    // the soft key bar over the cards, f1-f5 with a folder each. one open at a time
    const all = [...box.children], show = stack(box, all);
    const bar = document.getElementById("folders"), keys = [];
    const open = i => {
        keys.forEach((k, j) => k.setAttribute("aria-pressed", i === j));
        show(all.filter(c => +c.dataset.folder === i));
    };
    FOLDERS.forEach(([name], i) => {
        const key = tag("button", "", bar);
        key.innerHTML = `<small>f${i + 1}</small><span>${name}</span>`;
        key.onclick = () => open(i);
        keys.push(key);
    });
    open(0);

    // the no's go at the end instead of crossing things out all over the cards, as a TI
    // error screen. 2:Goto goes to the roadmap. 1:Quit asks are you sure like a ram reset
    // does, and if they really want it gone it goes, sulking
    if (nope.length) {
        const err = document.getElementById("not-yet");
        const list = nope.map(n => n[0].toLowerCase() + n.slice(1)).join(", "); // a list, not Capitalised
        // items are [label, what to do] and the what's a function or a link
        const screen = (title, text, items) => {
            const had = err.contains(document.activeElement); // so the number keys keep working
            err.replaceChildren();
            tag("div", "err-title", err).textContent = title;
            if (text) tag("p", "", err).textContent = text;
            const menu = tag("div", "err-menu", err);
            items.forEach(([name, does], i) => {
                const item = tag(typeof does === "string" ? "a" : "button", "", menu);
                item.innerHTML = `<span class="num">${i + 1}:</span>${name}`;
                if (typeof does === "string") item.href = does;
                else item.onclick = does;
            });
            if (had) menu.firstChild.focus();
        };
        const error = () => screen("ERR:NOT DONE YET", list, [["Quit", sure], ["Goto", "docs/roadmap/"]]);
        const sure = () => screen("Are you sure?", "", [["No", error], ["Yes", quit]]);
        function quit() {
            err.replaceChildren();
            tag("div", "err-done", err).textContent = "Done"; // what the TI says, over on the right
            setTimeout(() => {
                err.replaceChildren();
                err.className = "err-after";
                // the face in its own span, the mono font put the eyebrows over its mouth
                err.innerHTML = 'quit. they\'re still not done tho <span class="kao">( •᷄ࡇ•᷅)</span>';
            }, 1100);
        }
        // the number keys pick, like on the calc
        err.addEventListener("keydown", e => err.querySelectorAll(".err-menu > *")[e.key - 1]?.click());
        error();
        err.hidden = false;
    }

    const dl = document.getElementById("facts");
    for (const [k, v] of facts) {
        tag("dt", "", dl).textContent = k;
        label(tag("dd", "", dl), v.replace(/`/g, ""));
    }
}
