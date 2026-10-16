#ifndef FLUXION_SETTINGS_H
#define FLUXION_SETTINGS_H

/*
    the things the mode page changes, in one place so it and the app agree. the
    form (by hand / expanded / ...) and the identity toggles still live in app.c
    behind the app_* calls in app.h, this is just whats new
*/

typedef enum { ANGLE_RADIANS, ANGLE_DEGREES } angle_t;

extern angle_t setting_angle;

/* what solve gives for sin(x) = 1/2. the whole family (pi/6 + 2pi n, like the
   nspire) or just the ones in one turn, 0 up to 2pi (360 in degrees) */
typedef enum { TRIG_GENERAL, TRIG_ONE_TURN } trig_t;

extern trig_t setting_trig;

/* to and from the store (platform/store.h), a few bytes in their own slot. the
   mode page saves after every flip, the app loads at start. the form and
   rationalize go through app.h */
void settings_save(void);
void settings_load(void);

#endif
