// the "what's new" card at the bottom. the notes come out of docs/releases.md (make web copies it
// into build/), so writing the release notes there is writing them here too
import { tag, text, TINTS } from "./features.js";

// "## v0.1.0 - 2026-11-22" is a release, the paragraph under it the lede, "### New" a section with
// its "- " bullets. bullets with no ### above them (v0.0.1's) go in one section with no title
function releases(md) {
    const out = [];
    let release = null, section = null;
    for (const line of md.split("\n")) {
        if (line.startsWith("## ")) {
            const [version, date = ""] = line.slice(3).split(" - ").map(s => s.trim());
            out.push(release = { version, date, lede: "", sections: [] });
            section = null;
        } else if (!release) {
            continue; // the page's own title and blurb
        } else if (line.startsWith("### ")) {
            release.sections.push(section = { title: line.slice(4).trim(), items: [] });
        } else if (line.startsWith("- ")) {
            if (!section) release.sections.push(section = { title: "", items: [] });
            section.items.push(line.slice(2).trim());
        } else if (line.trim() && !section && !line.startsWith("---") && !line.startsWith("Back to")) {
            release.lede += (release.lede ? " " : "") + line.trim();
        }
    }
    return out;
}

// the docs' links are relative to the docs folder, `code/log.md` is docs/code/log/ once mkdocs is done
const href = url => /^[a-z]+:|^#/i.test(url) ? url : "docs/" + url.replace(/\.md(#.*)?$/, "/$1");

// **bold**, `code` and [links](url), all the notes use. built as nodes, never as html
function inline(str, parent) {
    for (const part of str.split(/(\*\*[^*]+\*\*|\*[^*]+\*|`[^`]+`|\[[^\]]+\]\([^)]+\))/)) {
        const link = part.match(/^\[([^\]]+)\]\(([^)]+)\)$/);
        if (part.startsWith("**") && part.endsWith("**") && part.length > 4)
            tag("strong", "", parent).textContent = part.slice(2, -2);
        else if (part.startsWith("*") && part.endsWith("*") && part.length > 2)
            tag("em", "", parent).textContent = part.slice(1, -1);
        else if (part.startsWith("`") && part.endsWith("`") && part.length > 2)
            tag("code", "", parent).textContent = part.slice(1, -1);
        else if (link)
            Object.assign(tag("a", "", parent), { textContent: link[1], href: href(link[2]) });
        else if (part)
            parent.append(part);
    }
}

export async function whatsNew() {
    const section = document.getElementById("whats-new");
    const all = releases(await text("build/releases.md"));
    if (!all.length) return; // stays hidden

    const bar = document.getElementById("versions"), box = document.getElementById("release");
    const buttons = [], cards = [];
    all.forEach((r, i) => {
        const card = tag("article", `release tint-${TINTS[(i + 2) % TINTS.length]}`, box); // the newest in butter
        tag("h3", "", card).textContent = r.version;
        const stamp = r.date || (i === 0 ? "fresh" : "");
        if (stamp) tag("span", "date", card).textContent = stamp;
        if (r.lede) inline(r.lede, tag("p", "lede", card));
        for (const s of r.sections) {
            if (s.title) tag("h4", "", card).textContent = s.title;
            const list = tag("ul", "", card);
            for (const item of s.items) inline(item, tag("li", "", list));
        }
        cards.push(card);

        const button = tag("button", "", bar);
        button.textContent = r.version;
        button.onclick = () => open(i);
        buttons.push(button);
    });

    function open(i) {
        buttons.forEach((b, j) => b.setAttribute("aria-pressed", i === j));
        cards.forEach((c, j) => c.hidden = i !== j);
    }
    open(0);
    section.hidden = false;
    document.getElementById("goto-new").hidden = false; // the hero's button, no use before there's a card
}
