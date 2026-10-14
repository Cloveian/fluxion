#ifdef __TICE__

/* calculator version of power.h */

#include "power.h"

#include <sys/power.h>

int power_battery(void) {
    int level = boot_GetBatteryStatus();
    /* supposed to be 0-4 but i swear it handed me a 5 once?? clamp it */
    return level > 4 ? 4 : level;
}

#else
typedef int power_ti_unused; /* nothing here on the PC */
#endif
