#ifndef FLUXION_KEYS_PC_H
#define FLUXION_KEYS_PC_H

/* the scripted half of the PC keys, keys_pc_script.c. keys_pc.c is the real keyboard */

#include "keys.h"

bool pc_script_on(void); /* keys_script() was called */
keypress_t pc_script_wait(void);
keypress_t pc_script_peek(void);

#endif
