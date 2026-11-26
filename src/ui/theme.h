#ifndef FLUXION_THEME_H
#define FLUXION_THEME_H

/* the colour presets the theme row cycles through, and one more after them that's the user's */

typedef struct {
    const char *name;
    unsigned char bg[3], fg[3], accent[3]; /* dim and select get mixed from these */
} theme_t;

extern const theme_t themes[];
extern const int theme_count;

/* the user's own, picked as steps on the mode page: a background and an accent, each hue / saturation / lightness. text is worked out from the background, the accent pushed lighter or darker till it reads on it */
enum { CUS_BG_H, CUS_BG_S, CUS_BG_L, CUS_AC_H, CUS_AC_S, CUS_AC_L, CUS_COUNT };
extern unsigned char custom_theme[CUS_COUNT];

/* how many steps a field has, and the value of one in its own unit (degrees, percent) */
int custom_steps(int field);
int custom_value(int field);
/* one step on, wrapping */
void custom_step(int field, int dir);

/* the presets and the custom one, themes[] and then custom */
int theme_total(void);
const char *theme_name(int i);

/* hands theme i to the screen, 0 if it's out of range */
void theme_apply(int i);

#endif
