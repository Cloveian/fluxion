/*Fluxion: real roots of e = 0 by sign changes, the last resort when solve gives up. about 6 digits (approx is a float), double roots that only touch 0 are missed. not in upstream PineappleCAS.*/

#include <math.h>

#include "cas.h"

#define PTS_MAX 260 /*the grid below is 247 points*/
#define BISECT 40 /*halvings, a float runs out of digits long before*/

/*e with var = x as a number, false when it's not a real number there (a pole, a log of a negative, another letter)*/
static bool value_at(pcas_ast_t *e, pcas_ast_t *var, double x, double *v) {
    mp_rat r = num_FromInt(0);
    pcas_ast_t *n;
    int k = 26;
    bool ok;

    if(r == NULL)
        return false;
    while(k > 0 && fabs(x) * (double)(1L << k) >= 1e9) /*the numerator has to fit a long*/
        k--;
    mp_rat_set_value(r, (mp_small)floor(x * (double)(1L << k) + 0.5), (mp_small)(1L << k));
    n = ast_MakeNumber(r);
    ok = n != NULL && float_at(e, var, n, v) && *v == *v && fabs(*v) < 1e30;
    ast_Cleanup(n);
    return ok;
}

/*r isn't one of the n roots yet and there's room*/
static bool new_root(double *out, int n, int max, double r) {
    int i;

    for(i = 0; i < n; i++)
        if(fabs(out[i] - r) < 1e-6 * (1 + fabs(r)))
            return false;
    return n < max;
}

int solve_numeric(pcas_ast_t *e, Symbol var, double *out, int max) {
    double xs[PTS_MAX], fs[PTS_MAX];
    bool ok[PTS_MAX];
    pcas_ast_t *x = ast_MakeSymbol(var);
    int n = 0, found = 0, i, j, defined = 0;

    if(x == NULL || max < 1)
        goto done;
    for(i = -3; i <= 3; i++) /*a wide look first, then every whole number to 100 and halves to 20*/
        if(i != 0)
            xs[n++] = i < 0 ? (i == -3 ? -1000 : i == -2 ? -500 : -200) : (i == 3 ? 1000 : i == 2 ? 500 : 200);
    for(i = -100; i <= 100; i++)
        xs[n++] = i;
    for(i = -40; i <= 40; i++)
        if(i % 2 != 0)
            xs[n++] = i / 2.0;
    /*sort, the order the three loops left isn't incresing*/
    for(i = 1; i < n; i++) {
        double t = xs[i];

        for(j = i - 1; j >= 0 && xs[j] > t; j--)
            xs[j + 1] = xs[j];
        xs[j + 1] = t;
    }
    for(i = 0; i < n; i++) {
        if(pcas_check()) {
            found = -1;
            goto done;
        }
        ok[i] = value_at(e, x, xs[i], &fs[i]);
        defined += ok[i];
    }
    if(defined == 0) {
        found = -1;
        goto done;
    }
    for(i = 0; i < n && found < max; i++) {
        double lo = xs[i], hi, flo = fs[i], mid = lo, fm, root;

        if(!ok[i])
            continue;
        if(fs[i] == 0) {
            if(new_root(out, found, max, xs[i]))
                out[found++] = xs[i];
            continue;
        }
        if(i + 1 >= n || !ok[i + 1] || (fs[i + 1] > 0) == (flo > 0) || fs[i + 1] == 0)
            continue;
        hi = xs[i + 1];
        for(j = 0; j < BISECT; j++) {
            mid = (lo + hi) / 2;
            if(!value_at(e, x, mid, &fm))
                break;
            if((fm > 0) == (flo > 0)) {
                lo = mid;
                flo = fm;
            } else {
                hi = mid;
            }
        }
        root = (lo + hi) / 2;
        /*a pole flips the sign too (tan, 1/x), there the value at the end is big, not small*/
        if(value_at(e, x, root, &fm) && fabs(fm) <= 1e-3 * (fabs(fs[i]) < fabs(fs[i + 1]) ? fabs(fs[i]) : fabs(fs[i + 1])) && new_root(out, found, max, root))
            out[found++] = root;
    }
done:
    ast_Cleanup(x);
    return found;
}
