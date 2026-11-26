#include "mode.h"

#include <stdio.h>

#include "app.h"
#include "page.h"
#include "settings.h"
#include "theme.h"

/* rows, in the order they show. the last six are the custom theme, same order as the CUS_ ones in theme.h */
enum { ROW_ANGLE, ROW_TRIG, ROW_FORM, ROW_RATIONALIZE, ROW_THEME, ROW_CUSTOM, ROW_COUNT = ROW_CUSTOM + CUS_COUNT };

static const char *mode_label(int i) {
    static const char *const custom[CUS_COUNT] = { "bg hue", "bg saturation", "bg lightness", "accent hue", "accent saturation", "accent lightness" };

    switch(i) {
    case ROW_ANGLE:       return "angle";
    case ROW_TRIG:        return "trig answers";
    case ROW_FORM:        return "simplest";
    case ROW_RATIONALIZE: return "rationalize";
    case ROW_THEME:       return "theme";
    }
    return i >= ROW_CUSTOM ? custom[i - ROW_CUSTOM] : "";
}

static const char *mode_value(int i) {
    static char shown[8];

    switch(i) {
    case ROW_ANGLE:       return setting_angle == ANGLE_DEGREES ? "degrees" : "radians";
    case ROW_TRIG:
        if(setting_trig == TRIG_GENERAL)
            return "general";
        return setting_angle == ANGLE_DEGREES ? "0 to 360" : "0 to 2pi";
    case ROW_FORM:        return app_form_name();
    case ROW_RATIONALIZE: return app_rationalize() ? "on" : "off";
    case ROW_THEME:       return theme_name(setting_theme);
    }
    if(i < ROW_CUSTOM)
        return NULL;
    snprintf(shown, sizeof shown, (i - ROW_CUSTOM) % 3 == 0 ? "%d" : "%d%%", custom_value(i - ROW_CUSTOM));
    return shown;
}

static const char *mode_help(int i) {
    switch(i) {
    case ROW_ANGLE:
        return "degrees puts a pi/180 on every trig argument before it runs, so it's exact: sin(30) is still 1/2";
    case ROW_TRIG:
        return "solve(sin(x)=1/2,x). general is pi/6 + 2pi n and 5pi/6 + 2pi n, the other keeps the ones in one turn";
    case ROW_FORM:
        return "what enter gives. [2nd][enter] cycles it from the line too, [alpha][enter] is a decimal just once";
    case ROW_RATIONALIZE:
        return "roots off the bottom, 1/sqrt(2) -> sqrt(2)/2. off keeps them where you wrote them";
    case ROW_THEME:
        return "left / right or enter goes through them. light or paper is easier outside or in a bright room. [2nd] up/down is the brightness, from anywhere but here";
    }
    return "yours: the colours under this row. the text colour is worked out from the background and the accent is moved until it reads on it, so nothing here makes it unreadable. left / right changes it";
}

/* the custom theme in use and shown, the caller saves */
static void custom_changed(void) {
    setting_theme = theme_count;
    theme_apply(setting_theme);
}

/* left / right (dir -1, 1): the theme row and the custom ones move a step, the rest leave it to enter */
static bool mode_adjust(int i, int dir) {
    if(i == ROW_THEME) {
        setting_theme = (setting_theme + theme_total() + dir) % theme_total();
        theme_apply(setting_theme);
    } else if(i >= ROW_CUSTOM) {
        custom_step(i - ROW_CUSTOM, dir);
        custom_changed();
    } else {
        return false;
    }
    settings_save();
    return true;
}

/* flip it and stay, that's the whole page */
static bool mode_pick(int i) {
    switch(i) {
    case ROW_ANGLE:       setting_angle = setting_angle == ANGLE_DEGREES ? ANGLE_RADIANS : ANGLE_DEGREES; break;
    case ROW_TRIG:        setting_trig = setting_trig == TRIG_GENERAL ? TRIG_ONE_TURN : TRIG_GENERAL; break;
    case ROW_FORM:        app_next_form(); break;
    case ROW_RATIONALIZE: app_toggle_rationalize(); break;
    case ROW_THEME:       setting_theme = (setting_theme + 1) % theme_total(); theme_apply(setting_theme); break;
    default:
        custom_step(i - ROW_CUSTOM, 1);
        custom_changed();
    }
    settings_save();    /* so a flip sticks even if the app dies before exit */
    return false;
}

static const page_t mode_page = { "mode", ROW_COUNT, mode_label, mode_value, mode_help, mode_pick, NULL, NULL, NULL, mode_adjust };

void mode_open(void) {
    page_open(&mode_page);
}
