#include "sticky.h"

#include <stdbool.h>
#include <time.h>

/* two taps of [2nd] or [alpha] this close together lock it */
#define DOUBLE_TAP (CLOCKS_PER_SEC * 2 / 5)

static unsigned mods, mods_lock;    /* M_ bits tapped for the next key, and the ones that stay on after it */

/* the last modifier tapped, 0 after any other key. a double tap is two
   taps with nothing in between (alpha a alpha b isn't one) */
static unsigned last;
static clock_t last_time;

unsigned sticky_on(void) { return mods; }
unsigned sticky_locked(void) { return mods_lock; }

/* any of them can be on together ([2nd][alpha] + a letter is a capital). a quick
   second tap locks it whatever the first one did (it may have just turned it off),
   a tap while locked unlocks */
static void tap(unsigned bit, bool again) {
    if(again) {
        mods |= bit;
        mods_lock |= bit;
    } else if(mods_lock & bit) {
        mods &= ~bit;
        mods_lock &= ~bit;
    } else {
        mods ^= bit;
    }
}

void sticky_tap(unsigned bit) {
    /* modifier? i hardly know 'er bahahwahaa */
    bool again = bit == last && clock() - last_time < DOUBLE_TAP;

    last = bit;
    last_time = clock();
    tap(bit, again);

    /* locking ate the double tap, so a third quick tap starts a fresh one */
    if(again)
        last = 0;
}

void sticky_used(void) {
    last = 0;
    mods = mods_lock;   /* the tapped ones turn off unless locked */
}

void sticky_clear(void) {
    mods = mods_lock = 0;
}
