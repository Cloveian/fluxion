#ifdef __TICE__

/* calculator side of store.h. a RAM appvar per slot, "w" throws the old one away
   (archived or not) and makes a fresh one in RAM, which is the only kind we write */

#include "store.h"

#include <fileioc.h>

bool store_save(const char *slot, const void *data, size_t n) {
    uint8_t h = ti_Open(slot, "w");
    bool ok;

    if(h == 0)
        return false;
    ok = n == 0 || ti_Write(data, n, 1, h) == 1;
    ti_Close(h);
    if(!ok)
        ti_Delete(slot);    /* half a save is worse than none */
    return ok;
}

size_t store_load(const char *slot, void *buf, size_t max) {
    uint8_t h = ti_Open(slot, "r");
    size_t size;

    if(h == 0)
        return 0;
    size = ti_GetSize(h);
    if(size > 0 && max > 0)
        ti_Read(buf, size < max ? size : max, 1, h);
    ti_Close(h);
    return size;
}

void store_delete(const char *slot) {
    ti_Delete(slot);
}

#else
typedef int store_ti_unused; /* nothing here on the PC */
#endif
