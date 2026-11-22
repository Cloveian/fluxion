// the showcase page: fluxion running in the calc, the try it button, the feature cards
import { live, keyboard } from "./live.js";
import { autoplay, load } from "./demo.js";
import { cards } from "./features.js";
import { whatsNew } from "./whatsnew.js";

// hover text for the big button, one for each thing it says. empty = no tooltip
const TIPS = {
    try: "", // shows "Try it", the demo is playing
    demo: "", // when it says "Play demo" and the calc is theirs
};

// the cards don't wait for the wasm, a click before it's there just does nothing
let show = () => {};
cards((li, keys) => show(li, keys));
whatsNew();

// clicking the "click for example" hint itself instead of a feature. no hover, no cursor, no
// underline, it's a secret
const hint = document.querySelector(".hint");
hint.addEventListener("click", () => {
    let tip = hint.querySelector(".gotcha");
    if (!tip) {
        tip = hint.appendChild(document.createElement("span"));
        tip.className = "gotcha";
        tip.innerHTML = 'Not <em><strong>this</strong></em> dummy <span class="kao">(„&gt;‿&lt;„)</span> the <em>feature list</em>';
    }
    tip.classList.add("shown");
    clearTimeout(tip.hide);
    tip.hide = setTimeout(() => tip.classList.remove("shown"), 2500);
});

const calc = document.getElementById("calc");
// the nspire's gets made (wasm fetched) when it's first picked, then both stay running so each
// keeps its own history and settings. each has its own demo keys too
const DEMO_FILE = { ce: "demo.txt", nspire: "demo-nspire.txt" };
const models = { ce: await live(calc, "ce") };
const demos = { ce: autoplay(models.ce, await load(DEMO_FILE.ce)) };
let model = "ce";
let fx = models.ce;
let demo = demos.ce;
const button = document.getElementById("try");
let off = null; // set while it's yours, undoes the keyboard

// the big button says what it'll do, "Play demo" when the calc is theirs
function mine(yes) {
    button.textContent = yes ? "Play demo" : "Try it";
    button.classList.toggle("mine", yes);
    button.dataset.tip = yes ? TIPS.demo : TIPS.try;
}

function trying(yes) {
    if (yes === !!off) return;
    if (yes) {
        demo.stop();
        fx.wipe();
        off = keyboard(fx);
    } else {
        off();
        off = null;
        fx.wipe();
        demo.start();
    }
    mine(yes);
}

// the switch over the calc
const toggles = { ce: document.getElementById("model-ce"), nspire: document.getElementById("model-nspire") };
let switching = false;

async function pick(next) {
    if (next === model || switching) return;
    switching = true;
    try {
        const was = models[model], yours = !!off;
        demo.stop();
        off?.();
        off = null;
        was.box.hidden = true;
        try {
            models[next] ??= await live(calc, next);
            demos[next] ??= autoplay(models[next], await load(DEMO_FILE[next]));
        } catch (err) { // no wasm for it (not built), stay on the one before
            console.error(err);
            was.box.hidden = false;
            if (yours) off = keyboard(fx);
            else demo.start();
            return;
        }
        model = next;
        fx = models[next];
        demo = demos[next];
        fx.box.hidden = false;
        document.body.dataset.model = next;
        for (const [name, b] of Object.entries(toggles))
            b.setAttribute("aria-pressed", name === next);
        fx.wipe();
        demo.start();
        mine(false);
    } finally {
        switching = false;
    }
}

for (const [name, b] of Object.entries(toggles))
    b.onclick = () => pick(name);

button.dataset.tip = TIPS.try;
button.onclick = () => trying(!off);
// pressing a key on the calc while it's playing means you want it. capture, so this runs
// before the keypad sends that key and the wipe doesn't eat it
calc.addEventListener("pointerdown", () => trying(true), true);

// a feature card got picked: play that one and leave the calc with them after, the loop
// only comes back with play demo. on a phone the calc isn't sticky, so bring it on screen.
// keys is { ce, nspire }, a card with none for this calc plays on the CE
let playing = null;
show = async (li, keys) => {
    if (!keys[model]) await pick("ce");
    if (!keys[model]) return;
    playing?.classList.remove("playing");
    (playing = li).classList.add("playing");
    trying(true);
    const r = calc.getBoundingClientRect();
    if (r.top < 0 || r.bottom > innerHeight)
        calc.scrollIntoView({ behavior: "smooth", block: "center" });
    demo.show(keys[model]).then(() => li.classList.remove("playing"));
};
demo.start();
