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

// two columns that each stack tight. each card goes to whichever column is shorter so far,
// so the top still reads left to right in the doc's order. one column when it's narrow
function stack(box) {
    const cards = [...box.children];
    let cols = 0;
    const lay = () => {
        const n = box.clientWidth >= 2 * 280 + 16 ? 2 : 1;
        if (n === cols) return;
        cols = n;
        box.replaceChildren();
        const columns = Array.from({ length: n }, () => tag("div", "column", box));
        for (const card of cards)
            columns.reduce((a, b) => b.offsetHeight < a.offsetHeight ? b : a).append(card);
    };
    lay();
    new ResizeObserver(lay).observe(box);
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
        tag("h3", "", card).textContent = c.title;
        const list = tag("ul", "", card);
        for (const r of yes) {
            const li = tag("li", "", list), more = extra.get(r.name);
            const name = tag("span", "name", li);
            label(name, r.name);
            // the yes* ones get a little ? with the limit in a tooltip. focusable so a tap shows it
            // on a phone, and it doesn't count as clicking the row
            if (more?.note) {
                const why = tag("span", "why", name); // in the name, so it can sit up like a footnote
                why.textContent = "?";
                why.tabIndex = 0;
                why.dataset.tip = more.note;
                why.setAttribute("aria-label", more.note);
                why.onclick = e => e.stopPropagation();
                why.onkeydown = e => e.stopPropagation();
            }
            if (!more?.keys) continue;
            li.classList.add("demo");
            li.tabIndex = 0;
            li.setAttribute("role", "button");
            li.onclick = () => play(li, more.keys);
            li.onkeydown = e => (e.key === "Enter" || e.key === " ") && (e.preventDefault(), play(li, more.keys));
        }
    }
    stack(box);

    // the no's go in one line at the end instead of crossing things out all over the cards
    if (nope.length) document.getElementById("not-yet").textContent = `Not yet: ${nope.map(n => n[0].toLowerCase() + n.slice(1)).join(", ")}.`; // mid sentence, so not Capitalised

    const dl = document.getElementById("facts");
    for (const [k, v] of facts) {
        tag("dt", "", dl).textContent = k;
        label(tag("dd", "", dl), v.replace(/`/g, ""));
    }
}
