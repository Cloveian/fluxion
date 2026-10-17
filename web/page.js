// the showcase page: fluxion running in the calc, the try it button, the feature cards
import { live, keyboard } from "./live.js";
import { autoplay, load } from "./demo.js";
import { cards } from "./features.js";

// hover text for the big button, one for each thing it says. empty = no tooltip
const TIPS = {
    try: "", // shows "Try it", the demo is playing
    demo: "", // when it says "Play demo" and the calc is theirs
};

// the cards don't wait for the wasm, a click before it's there just does nothing
let show = () => {};
cards((li, keys) => show(li, keys));

// clicking the "click for example" hint itself instead of a feature. no hover, no cursor, no
// underline, it's a secret
const hint = document.querySelector(".hint");
hint.addEventListener("click", () => {
    let tip = hint.querySelector(".gotcha");
    if (!tip) {
        tip = hint.appendChild(document.createElement("span"));
        tip.className = "gotcha";
        tip.innerHTML = "Not <em><strong>this</strong></em> dummy („&gt;‿&lt;„) the <em>feature list</em>";
    }
    tip.classList.add("shown");
    clearTimeout(tip.hide);
    tip.hide = setTimeout(() => tip.classList.remove("shown"), 2500);
});

const calc = document.getElementById("calc");
const fx = await live(calc);
const demo = autoplay(fx, await load());
const button = document.getElementById("try");
let off = null; // set while it's yours, undoes the keyboard

// wipes the half typed line and the screen, the history stays (up arrow) like on the calc
function wipe() {
    fx.press("clear");
    fx.press("clear", { on: true });
}

function trying(yes) {
    if (yes === !!off) return;
    if (yes) {
        demo.stop();
        wipe();
        off = keyboard(fx);
    } else {
        off();
        off = null;
        wipe();
        demo.start();
    }
    button.textContent = yes ? "Play demo" : "Try it";
    button.classList.toggle("mine", yes);
    button.dataset.tip = yes ? TIPS.demo : TIPS.try;
}

button.dataset.tip = TIPS.try;
button.onclick = () => trying(!off);
// pressing a key on the calc while it's playing means you want it. capture, so this runs
// before the keypad sends that key and the wipe doesn't eat it
calc.addEventListener("pointerdown", () => trying(true), true);

// a feature card got picked: play that one and leave the calc with them after, the loop
// only comes back with play demo. on a phone the calc isn't sticky, so bring it on screen
let playing = null;
show = (li, keys) => {
    playing?.classList.remove("playing");
    (playing = li).classList.add("playing");
    trying(true);
    const r = calc.getBoundingClientRect();
    if (r.top < 0 || r.bottom > innerHeight)
        calc.scrollIntoView({ behavior: "smooth", block: "center" });
    demo.show(keys).then(() => li.classList.remove("playing"));
};
demo.start();
