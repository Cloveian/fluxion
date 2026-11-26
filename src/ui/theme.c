#include "theme.h"

#include <math.h>
#include <string.h>

#include "platform/screen.h"

/* contrast checked against WCAG: text on bg 12+, dim on bg 3.1+, text on select 6.6+ */
/* dark and light stay 0 and 1, older saves have a 0 / 1 light flag there */
const theme_t themes[] = {
    { "dark",           {  24,  24,  32 }, { 235, 235, 240 }, { 110, 170, 255 } },
    { "light",          { 245, 245, 248 }, {  24,  24,  32 }, {  40, 110, 220 } },
    { "contrast",       {   0,   0,   0 }, { 255, 255, 255 }, { 255, 220,   0 } },
    { "contrast light", { 255, 255, 255 }, {   0,   0,   0 }, {   0,  70, 200 } },
    { "green",          {   8,  20,  12 }, { 150, 240, 160 }, { 220, 255, 110 } },
    { "rose",           {  32,  20,  30 }, { 245, 230, 240 }, { 255, 120, 190 } },
    { "paper",          { 240, 232, 212 }, {  32,  26,  20 }, { 170,  70,  30 } },
};
const int theme_count = sizeof themes / sizeof themes[0];

unsigned char custom_theme[CUS_COUNT] = { 8, 2, 1, 6, 5, 7 }; /* a dark blue grey and a pale cyan */

/* hue in 30 degree steps, the others in 20 and 10 percent ones */
static const unsigned char steps[CUS_COUNT] = { 12, 6, 11, 12, 6, 11 };
static const unsigned char unit[CUS_COUNT] = { 30, 20, 10, 30, 20, 10 };

int custom_steps(int field) { return steps[field]; }
int custom_value(int field) { return custom_theme[field] * unit[field]; }

void custom_step(int field, int dir) {
    custom_theme[field] = (unsigned char)((custom_theme[field] + steps[field] + dir) % steps[field]);
}

int theme_total(void) { return theme_count + 1; }

const char *theme_name(int i) {
    return i >= 0 && i < theme_count ? themes[i].name : "custom";
}

/* h in degrees, s and l 0 to 1 */
static void hsl(float h, float s, float l, unsigned char out[3]) {
    float c = (1 - fabsf(2 * l - 1)) * s, hp = h / 60, x = c * (1 - fabsf(fmodf(hp, 2) - 1)), m = l - c / 2, r = 0, g = 0, b = 0;

    if(hp < 1) { r = c; g = x; }
    else if(hp < 2) { r = x; g = c; }
    else if(hp < 3) { g = c; b = x; }
    else if(hp < 4) { g = x; b = c; }
    else if(hp < 5) { r = x; b = c; }
    else { r = c; b = x; }
    out[0] = (unsigned char)lroundf((r + m) * 255);
    out[1] = (unsigned char)lroundf((g + m) * 255);
    out[2] = (unsigned char)lroundf((b + m) * 255);
}

static float linear(unsigned char c) {
    float v = (float)c / 255;

    return v <= 0.04045f ? v / 12.92f : powf((v + 0.055f) / 1.055f, 2.4f);
}

static float luminance(const unsigned char c[3]) {
    return 0.2126f * linear(c[0]) + 0.7152f * linear(c[1]) + 0.0722f * linear(c[2]);
}

static float contrast(const unsigned char a[3], const unsigned char b[3]) {
    float x = luminance(a), y = luminance(b);

    return x > y ? (x + 0.05f) / (y + 0.05f) : (y + 0.05f) / (x + 0.05f);
}

/* off white or near black on bg, whichever reads better, pure white or black if neither gets to 12 */
static void text_on(const unsigned char bg[3], unsigned char fg[3]) {
    static const unsigned char pale[3] = { 235, 235, 240 }, ink[3] = { 24, 24, 32 }, white[3] = { 255, 255, 255 }, black[3] = { 0, 0, 0 };
    const unsigned char *pick = contrast(bg, pale) >= contrast(bg, ink) ? pale : ink;

    if(contrast(bg, pick) < 12)
        pick = contrast(bg, white) >= contrast(bg, black) ? white : black;
    memcpy(fg, pick, 3);
}

/* the accent, its lightness moved away from the backround's till it's 4.5 to 1 on it */
static void accent_on(const unsigned char bg[3], float h, float s, float l, unsigned char out[3]) {
    float step = luminance(bg) < 0.18f ? 0.03f : -0.03f;
    int i;

    for(i = 0; i < 34; i++) {
        hsl(h, s, l, out);
        if(contrast(bg, out) >= 4.5f)
            return;
        l += step;
        l = l < 0 ? 0 : l > 1 ? 1 : l;
    }
}

/* n/d of the way from a to b */
static void mix(unsigned char out[3], const unsigned char a[3], const unsigned char b[3], int n, int d) {
    int i;

    for(i = 0; i < 3; i++)
        out[i] = (unsigned char)(a[i] + (b[i] - a[i]) * n / d);
}

void theme_apply(int i) {
    theme_t mine;
    const theme_t *t;
    unsigned char rgb[COL_COUNT][3];
    int c;

    if(i == theme_count) {
        hsl((float)custom_value(CUS_BG_H), (float)custom_value(CUS_BG_S) / 100, (float)custom_value(CUS_BG_L) / 100, mine.bg);
        text_on(mine.bg, mine.fg);
        accent_on(mine.bg, (float)custom_value(CUS_AC_H), (float)custom_value(CUS_AC_S) / 100, (float)custom_value(CUS_AC_L) / 100, mine.accent);
        t = &mine;
    } else {
        t = &themes[i >= 0 && i < theme_count ? i : 0];
    }
    for(c = 0; c < 3; c++) {
        rgb[COL_BG][c] = t->bg[c];
        rgb[COL_FG][c] = t->fg[c];
        rgb[COL_ACCENT][c] = t->accent[c];
    }
    /* halfway to the text colour, closer while the dim text woudl be under 3.1 to 1 */
    for(c = 4; c <= 7; c++) {
        mix(rgb[COL_DIM], t->bg, t->fg, c, 8);
        if(contrast(rgb[COL_DIM], rgb[COL_BG]) >= 3.1f)
            break;
    }
    /* a quarter of the accent behind the selected text, less of it while the text would be under 6.6 to 1 on it */
    for(c = 4; c <= 16; c += c < 8 ? 2 : 4) {
        mix(rgb[COL_SELECT], t->bg, t->accent, 1, c);
        if(contrast(t->fg, rgb[COL_SELECT]) >= 6.6f)
            break;
    }
    screen_palette(rgb);
}
