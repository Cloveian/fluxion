#ifndef FLUXION_POWER_H
#define FLUXION_POWER_H

/*
    the battery, the same on both:
      power_ti.c  calculator: the OS's battery level
      power_pc.c  PC: always full, so the status bar looks like the calculator's
*/

/* battery level in bars, 0 (empty) to 4 (full) */
int power_battery(void);

#endif
