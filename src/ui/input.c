#include "input.h"

#include <string.h>

#include "platform/keys.h"
#include "bind.h"
#include "sticky.h"

input_t input_wait(void) {
    keypress_t p = keys_wait();
    input_t in;

    memset(&in, 0, sizeof in);
    in.quit = p.key == K_QUIT;
    if(p.key == K_POINT_DOWN || p.key == K_POINT_DRAG) {
        in.pointer = true;
        in.drag = p.key == K_POINT_DRAG;
        in.shift = (p.held & M_ALPHA) != 0;
        in.x = p.x;
        in.y = p.y;
    } else {
        in.a = bind_key(p.key, sticky_on() | p.held);
        in.plain = bind_key(p.key, 0);
    }
    return in;
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
