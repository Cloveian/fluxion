#include "outline.h"

#include "platform/screen.h"

#ifdef SCREEN_RESIZABLE

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* signed area and cover acumulated per cell, summed along each row at the end (the font-rs way). a row has two
   spare cells, a line that ends on the right edge writes one past it */
static float *acc;
static size_t acc_cap;
static int aw, ah, stride;

void outline_begin(int w, int h) {
    size_t need = (size_t)(w + 2) * (size_t)h;

    if(need > acc_cap) {
        free(acc);
        acc = malloc(need * sizeof *acc);
        acc_cap = acc != NULL ? need : 0;
    }
    aw = w;
    ah = h;
    stride = w + 2;
    if(acc != NULL)
        memset(acc, 0, need * sizeof *acc);
}

static float clampx(float x) {
    return x < 0 ? 0 : x > (float)aw - 0.001f ? (float)aw - 0.001f : x;
}

void outline_line(float x0, float y0, float x1, float y1) {
    float dir = 1, dxdy, x, t;
    int y, last;

    if(acc == NULL || y0 == y1)
        return;
    if(y0 > y1) {
        dir = -1;
        t = x0; x0 = x1; x1 = t;
        t = y0; y0 = y1; y1 = t;
    }
    dxdy = (x1 - x0) / (y1 - y0);
    x = x0;
    y = 0;
    if(y0 > 0)
        y = (int)y0;
    else
        x -= y0 * dxdy;
    last = (int)ceilf(y1);
    if(last > ah)
        last = ah;

    for(; y < last; y++) {
        float top = y0 > (float)y ? y0 : (float)y, bottom = y1 < (float)(y + 1) ? y1 : (float)(y + 1);
        float dy = bottom - top, next = x + dxdy * dy, d = dy * dir, a, b, floor_a, ceil_b;
        float *row = acc + (size_t)y * (size_t)stride;
        int ia, ib;

        if(dy <= 0) {
            x = next;
            continue;
        }
        a = clampx(x < next ? x : next);
        b = clampx(x < next ? next : x);
        floor_a = floorf(a);
        ia = (int)floor_a;
        ceil_b = ceilf(b);
        ib = (int)ceil_b;
        if(ib <= ia + 1) { /* inside one cell */
            float mid = 0.5f * (a + b) - floor_a;

            row[ia] += d - d * mid;
            row[ia + 1] += d * mid;
        } else {
            float s = 1 / (b - a), fa = a - floor_a, first = 0.5f * s * (1 - fa) * (1 - fa);
            float fb = b - ceil_b + 1, lastc = 0.5f * s * fb * fb;
            int i;

            row[ia] += d * first;
            if(ib == ia + 2) {
                row[ia + 1] += d * (1 - first - lastc);
            } else {
                float a1 = s * (1.5f - fa), a2;

                row[ia + 1] += d * (a1 - first);
                for(i = ia + 2; i < ib - 1; i++)
                    row[i] += d * s;
                a2 = a1 + (float)(ib - ia - 3) * s;
                row[ib - 1] += d * (1 - a2 - lastc);
            }
            row[ib] += d * lastc;
        }
        x = next;
    }
}

void outline_quad(float x0, float y0, float cx, float cy, float x1, float y1) {
    float dx = x0 - 2 * cx + x1, dy = y0 - 2 * cy + y1, px = x0, py = y0;
    int n = 1 + (int)sqrtf(sqrtf(dx * dx + dy * dy) * 2.5f), i;

    for(i = 1; i <= n; i++) {
        float t = (float)i / (float)n, u = 1 - t;
        float x = u * u * x0 + 2 * u * t * cx + t * t * x1, y = u * u * y0 + 2 * u * t * cy + t * t * y1;

        outline_line(px, py, x, y);
        px = x;
        py = y;
    }
}

void outline_end(unsigned char *alpha) {
    int x, y;

    if(acc == NULL) { /* out of memory, nothing to draw */
        memset(alpha, 0, (size_t)aw * (size_t)ah);
        return;
    }
    for(y = 0; y < ah; y++) {
        const float *row = acc + (size_t)y * (size_t)stride;
        float sum = 0;

        for(x = 0; x < aw; x++) {
            float v;

            sum += row[x];
            v = fabsf(sum);
            alpha[y * aw + x] = (unsigned char)((v > 1 ? 1 : v) * 255 + 0.5f);
        }
    }
}

#else
typedef int outline_unused; /* the calculators and the web page keep the bitmap fonts */
#endif
