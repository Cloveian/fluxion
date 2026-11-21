/* key + modifiers -> what it does, off the keypad's table */

#include "bind.h"

static bool lookup(keycode_t key, unsigned mods, action_t *a) {
    unsigned i;

    for(i = 0; i < keypad.count; i++)
        if(keypad.binds[i].key == key && keypad.binds[i].mods == mods) {
            *a = keypad.binds[i].a;
            return true;
        }

    /* digits and letters type themselves on every keypad */
    a->op = A_CHAR;
    a->s = NULL;
    if(mods == 0 && key >= K_0 && key <= K_9)
        a->n = '0' + (key - K_0);
    else if(mods == 0 && key >= K_LETTER_A && key <= K_LETTER_Z)
        a->n = 'a' + (key - K_LETTER_A);
    else if(mods == 0 && key >= K_CAPITAL_A && key <= K_CAPITAL_Z)
        a->n = 'A' + (key - K_CAPITAL_A);
    else
        return false;
    return true;
}

action_t bind_key(keycode_t key, unsigned mods) {
    action_t a = { A_NONE, 0, NULL };
    unsigned i;

    /* a capital is whatever letter the key makes, made big. [2nd][alpha][math] is A */
    if((mods & keypad.caps) == keypad.caps) {
        a = bind_key(key, (mods & ~keypad.caps) | keypad.caps_with);
        if(a.op == A_CHAR && a.n >= 'a' && a.n <= 'z') {
            a.n += 'A' - 'a';
            return a;
        }
    }

    for(i = 0; ; i++) {
        if(lookup(key, mods, &a))
            return a;
        if(i == sizeof keypad.drop / sizeof keypad.drop[0] || keypad.drop[i] == 0)
            break;
        mods &= ~keypad.drop[i];
    }
    a.op = A_NONE;
    return a;
}
