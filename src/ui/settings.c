#include "settings.h"

#include "app.h"
#include "platform/screen.h"
#include "platform/store.h"

angle_t setting_angle = ANGLE_RADIANS;  /* the engine's native one */
trig_t setting_trig = TRIG_GENERAL;
int setting_brightness = 2;
bool setting_light = false;

/* format byte, angle, form, rationalize, trig, brightness, light. nothing fancy,
   anything off and it's ignored. format 2 grew the trig byte, 3 the two screen
   ones, anything older just gets dropped */
#define SLOT "FLXMODE"

void settings_save(void) {
    unsigned char b[7];

    b[0] = 3;
    b[1] = (unsigned char)setting_angle;
    b[2] = (unsigned char)app_form_index();
    b[3] = (unsigned char)app_rationalize();
    b[4] = (unsigned char)setting_trig;
    b[5] = (unsigned char)setting_brightness;
    b[6] = (unsigned char)setting_light;
    store_save(SLOT, b, sizeof b);
}

void settings_load(void) {
    unsigned char b[7];

    if(store_load(SLOT, b, sizeof b) == sizeof b && b[0] == 3) {
        setting_angle = b[1] == ANGLE_DEGREES ? ANGLE_DEGREES : ANGLE_RADIANS;
        app_set_form_index(b[2]);
        if(app_rationalize() != (b[3] != 0))
            app_toggle_rationalize();
        setting_trig = b[4] == TRIG_ONE_TURN ? TRIG_ONE_TURN : TRIG_GENERAL;
        setting_brightness = b[5] <= 7 ? b[5] : screen_brightness_current();
        setting_light = b[6] != 0;
    } else {
        setting_brightness = screen_brightness_current();  /* nothing saved yet, or an older format. start where the OS was */
    }

    /* outside the if, a first run with nothing saved still wants the defaults on screen */
    screen_theme(setting_light);
    screen_brightness(setting_brightness);
}
