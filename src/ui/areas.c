#include "areas.h"

#include "platform/screen.h"

#define KEYS_H 15

areas_t areas;

void areas_init(void) {
    const font_t *small = draw_label_font();
    int top_h = small->ascent + small->descent + 3;

    areas.top = (rect_t){ 0, 0, SCREEN_W, top_h };
    areas.keys = (rect_t){ 0, SCREEN_H - KEYS_H, SCREEN_W, KEYS_H };
    areas.body = (rect_t){ 0, top_h, SCREEN_W, areas.keys.y - top_h };
}

int areas_body_bottom(void) {
    return NSPIRE ? areas.keys.y + areas.keys.h : areas.body.y + areas.body.h;
}
