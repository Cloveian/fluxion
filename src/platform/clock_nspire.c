#ifdef _TINSPIRE

/* clock() for the nspire, off the first timer. newlib's goes through Ndless's
   _times(), which fills nothing in, so it summed stack junk (an answer "took" 20083 s).
   this one wins at link time over libc's */

#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

#define TIMER   ((volatile unsigned *)0x900C0000)  /* SP804, 0 load, 1 value, 2 control */
#define SOURCE  (*(volatile unsigned *)0x900C0080)  /* 0xA picks 32768 Hz on the CX, nSDL does the same */
#define BUS     (*(volatile unsigned *)0x900B0018)  /* bit 11 off lets me at the timer */
#define HZ      32768

static bool running;
static unsigned start, was_load, was_control, was_source;

/* the OS gets its timer back how it had it */
static void give_back(void) {
    TIMER[2] = 0;
    TIMER[0] = was_load;
    SOURCE = was_source;
    TIMER[2] = was_control;
}

static void begin(void) {
    BUS &= ~(1u << 11);
    was_load = TIMER[0];
    was_control = TIMER[2];
    was_source = SOURCE;
    atexit(give_back);

    SOURCE = 0xA;
    TIMER[2] = 0x82;    /* on, 32 bit, no prescale, free running, no interrupt. counts down */
    start = TIMER[1];
    running = true;
}

clock_t clock(void) {
    if(!running)
        begin();
    /* wraps every 36 h, the subtraction doesn't care */
    return (clock_t)((unsigned long long)(start - TIMER[1]) * CLOCKS_PER_SEC / HZ);
}

#else
typedef int clock_nspire_unused; /* nothing here off the nspire */
#endif
