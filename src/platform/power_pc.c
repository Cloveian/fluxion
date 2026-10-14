#ifndef __TICE__

/* PC version of power.h, no battery to read */

#include "power.h"

int power_battery(void) {
    return 4;
}

#else
typedef int power_pc_unused; /* nothing here on the calculator */
#endif
