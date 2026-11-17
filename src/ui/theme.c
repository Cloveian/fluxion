#include "theme.h"

#include "platform/screen.h"

/* contrast checked against WCAG: text on bg 12+, dim on bg 3.1+, text on select 6.6+ */
/* dark and light stay 0 and 1, older saves have a 0 / 1 light flag there */
const theme_t themes[] = {
    { "dark",     {  24,  24,  32 }, { 235, 235, 240 }, { 110, 170, 255 } },
    { "light",    { 245, 245, 248 }, {  24,  24,  32 }, {  40, 110, 220 } },
    { "contrast", {   0,   0,   0 }, { 255, 255, 255 }, { 255, 220,   0 } },
    { "green",    {   8,  20,  12 }, { 150, 240, 160 }, { 220, 255, 110 } },
    { "rose",     {  32,  20,  30 }, { 245, 230, 240 }, { 255, 120, 190 } },
    { "paper",    { 240, 232, 212 }, {  32,  26,  20 }, { 170,  70,  30 } },
};
const int theme_count = sizeof themes / sizeof themes[0];

/* n/d of the way from a to b */
static void mix(unsigned char out[3], const unsigned char a[3], const unsigned char b[3], int n, int d) {
    int i;

    for(i = 0; i < 3; i++)
        out[i] = (unsigned char)(a[i] + (b[i] - a[i]) * n / d);
}

void theme_apply(int i) {
    const theme_t *t = &themes[i >= 0 && i < theme_count ? i : 0];
    unsigned char rgb[COL_COUNT][3];
    int c;

    for(c = 0; c < 3; c++) {
        rgb[COL_BG][c] = t->bg[c];
        rgb[COL_FG][c] = t->fg[c];
        rgb[COL_ACCENT][c] = t->accent[c];
    }
    mix(rgb[COL_DIM], t->bg, t->fg, 1, 2);
    mix(rgb[COL_SELECT], t->bg, t->accent, 1, 4);
    screen_palette(rgb);
}
