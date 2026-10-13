/* checks that every identity's function mask read from its text (text_func_mask)
   is never stricter than the mask of the parsed identity (func_mask).
   if it were, id_ExecuteTable would skip an identity that should apply.
   run by: make -f pc.makefile test */

#include <stdio.h>
#include "cas/cas.h"
#include "cas/identities.h"
#include "cas/derivative.h"

static int check(const char *name, pcas_id_t *t, unsigned n) {
    unsigned i; int bad = 0, equal = 0;
    for (i = 0; i < n; i++) {
        unsigned long text = text_func_mask(t[i].from_text), parsed;
        if (!id_Load(&t[i])) { printf("%s[%u] failed to load: %s\n", name, i, t[i].from_text); continue; }
        parsed = func_mask(t[i].from);
        if (text & ~parsed) { printf("UNSAFE %s[%u] %s  text=%lx parsed=%lx\n", name, i, t[i].from_text, text, parsed); bad++; }
        else if (text == parsed) equal++;
        else printf("weaker (safe, skips less) %s[%u] %s  text=%lx parsed=%lx\n", name, i, t[i].from_text, text, parsed);
    }
    printf("%-22s %3u identities, %3d exact, %d unsafe\n", name, n, equal, bad);
    return bad;
}

int main(void) {
    int bad = 0;
    bad += check("general", id_general, ID_NUM_GENERAL);
    bad += check("trig_identities", id_trig_identities, ID_NUM_TRIG_IDENTITIES);
    bad += check("trig_inv_constants", id_trig_inv_constants, ID_NUM_TRIG_INV_CONSTANTS);
    bad += check("trig_constants", id_trig_constants, ID_NUM_TRIG_CONSTANTS);
    bad += check("hyperbolic", id_hyperbolic, ID_NUM_HYPERBOLIC);
    bad += check("complex", id_complex, ID_NUM_COMPLEX);
    bad += check("derivative", id_derivative, ID_NUM_DERIV);
    id_UnloadAll();
    return bad != 0;
}
