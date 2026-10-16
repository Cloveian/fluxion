#include "settings.h"

#include "app.h"
#include "platform/store.h"

angle_t setting_angle = ANGLE_RADIANS;  /* the engine's native one */
trig_t setting_trig = TRIG_GENERAL;

/* format byte, angle, form, rationalize, trig. nothing fancy, anything off and
   it's ignored. format 2 grew the trig byte, a 1 from before just gets dropped */
#define SLOT "FLXMODE"

void settings_save(void) {
    unsigned char b[5];

    b[0] = 2;
    b[1] = (unsigned char)setting_angle;
    b[2] = (unsigned char)app_form_index();
    b[3] = (unsigned char)app_rationalize();
    b[4] = (unsigned char)setting_trig;
    store_save(SLOT, b, sizeof b);
}

void settings_load(void) {
    unsigned char b[5];

    if(store_load(SLOT, b, sizeof b) != sizeof b || b[0] != 2)
        return;
    setting_angle = b[1] == ANGLE_DEGREES ? ANGLE_DEGREES : ANGLE_RADIANS;
    app_set_form_index(b[2]);
    if(app_rationalize() != (b[3] != 0))
        app_toggle_rationalize();
    setting_trig = b[4] == TRIG_ONE_TURN ? TRIG_ONE_TURN : TRIG_GENERAL;
}
