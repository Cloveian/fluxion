#ifndef FLUXION_KEYS_PC_H
#define FLUXION_KEYS_PC_H

/* the scripted half of the PC input, keys_pc_script.c. keys_pc.c is the real keyboard as a keypad */

#include "keys.h"
#include "events_pc.h"

bool pc_script_on(void); /* keys_script() was called */

/* the keypad's presses */
keypress_t pc_script_wait(void);
keypress_t pc_script_peek(void);

/* the desktop's events */
void pc_script_event(pc_event_t *ev);
bool pc_script_busy_event(pc_event_t *ev);

#endif
