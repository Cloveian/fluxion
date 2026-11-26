#include "input.h"

#include <string.h>

#include "platform/keys.h"
#include "bind.h"
#include "sticky.h"

void input_begin(void (*on_sleep)(bool asleep)) {
    keys_begin();
    keys_on_sleep = on_sleep;
}

void input_end(void) {
    keys_end();
}

input_t input_wait(in_ctx_t ctx) {
    keypress_t p = keys_wait();
    input_t in;

    memset(&in, 0, sizeof in);
    in.quit = p.key == K_QUIT;
    if(p.key == K_POINT_DOWN || p.key == K_POINT_DRAG || p.key == K_POINT_MOVE) {
        in.pointer = true;
        in.drag = p.key == K_POINT_DRAG;
        in.hover = p.key == K_POINT_MOVE;
        in.shift = (p.held & M_ALPHA) != 0;
        in.x = p.x;
        in.y = p.y;
        if(!in.hover && ctx != IN_POPUP)
            sticky_used(); /* a click spends a tapped modifier too */
        return in;
    }
    in.a = bind_key(p.key, sticky_on() | p.held);
    in.plain = bind_key(p.key, 0);
    if(in.quit || ctx == IN_POPUP) /* a popup leaves the modifiers alone */
        return in;

    /* [2nd][alpha][on] is lights out. both are already on by the time the on tap comes */
    if(in.a.op == A_SCREEN_OFF) {
        sticky_clear();
        keys_sleep();
        in.eaten = true;
        return in;
    }
    if(in.plain.op == A_MOD) {
        sticky_tap((unsigned)in.plain.n);
        in.eaten = true;
        return in;
    }
    sticky_used();

    /* locked alpha leaves 1-9 as numbers in a menu, a tapped one still makes 7-9 o p q */
    if(ctx == IN_MENU && (sticky_locked() & M_ALPHA) && in.plain.op == A_CHAR && in.plain.n >= '1' && in.plain.n <= '9')
        in.a = in.plain;
    return in;
}

void input_mods(input_mod_t out[3]) {
    int i;

    for(i = 0; i < 3; i++) {
        unsigned m = keypad.shown[i];
        char letter = m == M_2ND ? '2' : m == M_ALPHA ? 'A' : m == M_SHIFT ? 'S' : 'C'; /* [on] and ctrl are both C */

        out[i].letter = (m & sticky_on()) ? letter : '\0';
        out[i].locked = (m & sticky_locked()) != 0;
    }
}

const char *input_cancel_hint(void) {
    return keypad.cancel;
}

unsigned input_binding_count(void) {
    return keypad.count;
}

action_t input_binding(unsigned i, bool *plain) {
    *plain = keypad.binds[i].mods == 0;
    return keypad.binds[i].a;
}

bool input_binding_text(unsigned i, char *buf, int n) {
    return bind_text(keypad.binds[i].key, keypad.binds[i].mods, buf, n);
}

const char *input_digit_keys(void) {
    return keypad.digits;
}

const char *input_letter_keys(void) {
    return keypad.letters;
}

/* what the keys looked like at the last poll, for edges */
static keycode_t poll_key;
static unsigned poll_held;
static bool poll_chord;

void input_poll_reset(void) {
    poll_key = K_NONE;
    poll_held = 0;
    poll_chord = false;
}

bool input_poll(void) {
    keypress_t p;
    action_t a;

    /* a modifier tapped while it works still counts for the next key (keys_peek marks
       whatever's down as held). the ones peek sees as keys (2nd, alpha) on the press,
       the held ones ([on], ctrl, shift) on the release and only if nothing came with
       them, since [on]+[mode] is the cancel */
    p = keys_peek();
    a = bind_key(p.key, 0);
    if(p.key != poll_key && a.op == A_MOD && !p.held)
        sticky_tap((unsigned)a.n);
    if(p.held && !poll_held)
        poll_chord = false;
    if(p.held && p.key != K_NONE)
        poll_chord = true;
    if((poll_held & ~p.held) && !poll_chord)
        sticky_tap(poll_held & ~p.held);
    poll_key = p.key;
    poll_held = p.held;

    a = bind_key(p.key, p.held);
    return a.op == A_CANCEL || a.op == A_ESC || a.op == A_QUIT;
}
