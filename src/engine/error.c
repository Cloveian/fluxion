#include "error.h"

#include <stdlib.h>

const char *error_text[AMOUNT_ERRORS] = {
    "Success",

    "Generic failure",

    "AST operation not allowed",
    "AST index out of bounds",

    "Invalid token",

    "Bad operator",
    "Unmatched closing parentheses",
    "Bad comma",

    "No mapping for symbol",

    "Out of memory",
};

/*Fluxion: see error.h*/
bool pcas_oom = false;
bool pcas_cancelled = false;

#ifdef COMPILE_PC
/*fault injection for the tests. PCAS_FAIL_ALLOC=n makes the nth allocation fail,
  so a script can walk n up and find every site that can't take a NULL*/
static unsigned long alloc_count = 0, fail_at = 0;
static bool read_env = false;

static bool fail_this_one(void) {
    if(!read_env) {
        const char *s = getenv("PCAS_FAIL_ALLOC");
        fail_at = s != NULL ? strtoul(s, NULL, 10) : 0;
        read_env = true;
    }
    return fail_at != 0 && ++alloc_count == fail_at;
}
#else
#define fail_this_one() false
#endif

static void *hit(void *p) {
    if(p == NULL)
        pcas_oom = pcas_cancelled = true;
    return p;
}

void *pcas_malloc(size_t size) {
    return hit(fail_this_one() ? NULL : malloc(size));
}

void *pcas_calloc(size_t count, size_t size) {
    return hit(fail_this_one() ? NULL : calloc(count, size));
}

void *pcas_realloc(void *old, size_t size) {
    return hit(fail_this_one() ? NULL : realloc(old, size));
}