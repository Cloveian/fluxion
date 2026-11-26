#include "settings.h"

#include "app.h"
#include "theme.h"
#include "platform/screen.h"
#include "platform/store.h"

angle_t setting_angle = ANGLE_RADIANS;  /* the engine's native one */
trig_t setting_trig = TRIG_GENERAL;
int setting_brightness = 2;
int setting_theme = 0;

/* format byte, angle, form, rationalize, trig, brightness, theme, then the custom theme's six steps. nothing fancy, anything off and it's ignored. format 2 grew the trig byte, 3 the two screen ones, 4 the custom theme, anything older just gets dropped */
#define SLOT "FLXMODE"
#define SIZE (7 + CUS_COUNT)

void settings_save(void) {
    unsigned char b[SIZE];
    int i;

    b[0] = 4;
    b[1] = (unsigned char)setting_angle;
    b[2] = (unsigned char)app_form_index();
    b[3] = (unsigned char)app_rationalize();
    b[4] = (unsigned char)setting_trig;
    b[5] = (unsigned char)setting_brightness;
    b[6] = (unsigned char)setting_theme;
    for(i = 0; i < CUS_COUNT; i++)
        b[7 + i] = custom_theme[i];
    store_save(SLOT, b, sizeof b);
}

void settings_load(void) {
    unsigned char b[SIZE];
    size_t got = store_load(SLOT, b, sizeof b);
    int i;

    if(got == 7 && b[0] == 3) { /* the same without the custom steps, which stay at their defaults. green, rose and paper moved up one for contrast light */
        b[0] = 4;
        b[6] = b[6] >= 3 ? (unsigned char)(b[6] + 1) : b[6];
    } else if(got != SIZE)
        b[0] = 0;
    if(b[0] == 4) {
        setting_angle = b[1] == ANGLE_DEGREES ? ANGLE_DEGREES : ANGLE_RADIANS;
        app_set_form_index(b[2]);
        if(app_rationalize() != (b[3] != 0))
            app_toggle_rationalize();
        setting_trig = b[4] == TRIG_ONE_TURN ? TRIG_ONE_TURN : TRIG_GENERAL;
        setting_brightness = b[5] <= 7 ? b[5] : screen_brightness_current();
        setting_theme = b[6] < theme_total() ? b[6] : 0;
        if(got == SIZE)
            for(i = 0; i < CUS_COUNT; i++)
                custom_theme[i] = b[7 + i] < custom_steps(i) ? b[7 + i] : custom_theme[i];
    } else {
        setting_brightness = screen_brightness_current();  /* nothing saved yet, or an older format. start where the OS was */
    }

    /* outside the if, a first run with nothing saved still wants the defaults on screen */
    theme_apply(setting_theme);
    screen_brightness(setting_brightness);
}
