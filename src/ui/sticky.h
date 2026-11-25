#ifndef FLUXION_STICKY_H
#define FLUXION_STICKY_H

/* the keypad's [2nd] / [alpha] / [on] (ctrl / shift on the nspire): tapped, on for the next key, a quick second tap locks it. input state, the app only asks */

/* M_ bits on now, tapped or locked */
unsigned sticky_on(void);
/* the ones that stay on after a key */
unsigned sticky_locked(void);
/* one of them was tapped */
void sticky_tap(unsigned bit);
/* a key that isn't one of them went by, the tapped ones turn off */
void sticky_used(void);
/* all off, locks too */
void sticky_clear(void);

#endif
