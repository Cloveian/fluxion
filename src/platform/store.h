#ifndef FLUXION_STORE_H
#define FLUXION_STORE_H

/*
    somewhere to keep bytes between runs, the same on both:
      store_ti.c  calculator: appvars in RAM (fileioc). they survive closing the
                  app, not a RAM reset, and that's fine (roadmap: Storage strategy)
      store_pc.c  PC: files in build-pc/store/, so a run on the PC remembers too

    a slot is a name up to 8 letters, like an appvar's. whats in it is up to the
    caller (the app saves the history and the stored names, each with a little
    header of its own). sizes are capped by the caller, an appvar can't go past
    64 KB anyway and user RAM is shared with everything else on the calculator
*/

#include <stdbool.h>
#include <stddef.h>

/* writes the whole slot. false if it couldn't (no room) */
bool store_save(const char *slot, const void *data, size_t n);

/* reads up to max bytes into buf, returns how many there were (0 = no such slot,
   or empty). more than max means it got cut, so make buf big enough */
size_t store_load(const char *slot, void *buf, size_t max);

void store_delete(const char *slot);

#endif
