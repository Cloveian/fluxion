#ifndef __TICE__

/* PC side of store.h. a file per slot in build-pc/store/ (made on first save), so
   the window remembers across runs like the calculator does. relative to wherever
   it's run from, which is the repo root for everything in the makefile */

#include "store.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define DIR "build-pc/store"

static const char *path(const char *slot) {
    static char p[sizeof DIR + 16];

    snprintf(p, sizeof p, DIR "/%s", slot);
    return p;
}

bool store_save(const char *slot, const void *data, size_t n) {
    FILE *f;
    bool ok;

    mkdir("build-pc", 0755);
    mkdir(DIR, 0755);
    f = fopen(path(slot), "wb");
    if(f == NULL)
        return false;
    ok = n == 0 || fwrite(data, n, 1, f) == 1;
    ok = fclose(f) == 0 && ok;
    if(!ok)
        remove(path(slot));
    return ok;
}

size_t store_load(const char *slot, void *buf, size_t max) {
    FILE *f = fopen(path(slot), "rb");
    long size;

    if(f == NULL)
        return 0;
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    rewind(f);
    if(size > 0 && max > 0 && fread(buf, 1, (size_t)size < max ? (size_t)size : max, f) == 0)
        size = 0;
    fclose(f);
    return size > 0 ? (size_t)size : 0;
}

void store_delete(const char *slot) {
    remove(path(slot));
}

#else
typedef int store_pc_unused; /* nothing here on the calculator */
#endif
