#ifndef FLUXION_THEME_H
#define FLUXION_THEME_H

/* the colour presets the theme row cycles through */

typedef struct {
    const char *name;
    unsigned char bg[3], fg[3], accent[3]; /* dim and select get mixed from these */
} theme_t;

extern const theme_t themes[];
extern const int theme_count;

/* hands themes[i] to the screen, 0 if it's out of range */
void theme_apply(int i);

#endif
