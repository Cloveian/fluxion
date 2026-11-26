#ifndef FLUXION_TOPBAR_H
#define FLUXION_TOPBAR_H

/* the bar along the top: modifier padlocks from input, battery from power */

/* form is the name of a non-normal answer form, NULL for none */
void topbar_draw(const char *form);

/* the dot going round while the engine works, one frame per call, drawn straight on the screen */
void topbar_working(unsigned frame);

/* "12s, [on]+[mode] stops it" after the lights */
void topbar_hint(unsigned long seconds);

#endif
