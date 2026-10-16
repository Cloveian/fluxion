#ifndef FLUXION_SETTINGS_H
#define FLUXION_SETTINGS_H

/*
    the things the mode page changes, in one place so it and the app agree. the
    form (by hand / expanded / ...) and the identity toggles still live in app.c
    behind the app_* calls in app.h, this is just whats new. not saved between
    runs yet (todo, it'd go with the names appvar)
*/

typedef enum { ANGLE_RADIANS, ANGLE_DEGREES } angle_t;

extern angle_t setting_angle;

#endif
