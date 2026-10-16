#include "mode.h"

#include "app.h"
#include "page.h"
#include "settings.h"

/* rows, in the order they show */
enum { ROW_ANGLE, ROW_FORM, ROW_RATIONALIZE, ROW_COUNT };

static const char *mode_label(int i) {
    switch(i) {
    case ROW_ANGLE:       return "angle";
    case ROW_FORM:        return "simplest";
    case ROW_RATIONALIZE: return "rationalize";
    }
    return "";
}

static const char *mode_value(int i) {
    switch(i) {
    case ROW_ANGLE:       return setting_angle == ANGLE_DEGREES ? "degrees" : "radians";
    case ROW_FORM:        return app_form_name();
    case ROW_RATIONALIZE: return app_rationalize() ? "on" : "off";
    }
    return NULL;
}

static const char *mode_help(int i) {
    switch(i) {
    case ROW_ANGLE:
        return "degrees puts a pi/180 on every trig argument before it runs, so it's exact: sin(30) is still 1/2";
    case ROW_FORM:
        return "what enter gives. [2nd][enter] cycles it from the line too, [alpha][enter] is decimal";
    case ROW_RATIONALIZE:
        return "roots off the bottom, 1/sqrt(2) -> sqrt(2)/2. off keeps them where you wrote them";
    }
    return NULL;
}

/* flip it and stay, that's the whole page */
static bool mode_pick(int i) {
    switch(i) {
    case ROW_ANGLE:       setting_angle = setting_angle == ANGLE_DEGREES ? ANGLE_RADIANS : ANGLE_DEGREES; break;
    case ROW_FORM:        app_next_form(); break;
    case ROW_RATIONALIZE: app_toggle_rationalize(); break;
    }
    settings_save();    /* so a flip sticks even if the app dies before exit */
    return false;
}

static const page_t mode_page = { "mode", ROW_COUNT, mode_label, mode_value, mode_help, mode_pick };

void mode_open(void) {
    page_open(&mode_page);
}
