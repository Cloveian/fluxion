#include "draw.h"
#include "platform/screen.h"

/* fonts */

static const font_t *ladder[LAYOUT_MAX_SIZES] = { &font_5_8, &font_7_13, &font_9_15, &font_10_20 };

const font_t *draw_font_for(unsigned char size) {
    return ladder[size < draw_font.sizes ? size : draw_font.sizes - 1];
}

const font_t *draw_label_font(void) {
    return draw_font_for(draw_font.base > 0 ? draw_font.base - 1 : 0);
}

/* bold smears a pixel right so every glyph gets one more, italic hangs the top
   row out past the end. same sums draw_text_styled moves by, so the boxes fit */
int draw_text_width_styled(const char *text, const font_t *font, font_style_t style) {
    int w = 0;

    for(; *text != '\0'; text++)
        w += font->advance[(unsigned char)*text & 0x7F] + (style & STYLE_BOLD ? 1 : 0);
    if(style & STYLE_ITALIC)
        w += font->ascent / 3;
    return w;
}

int draw_text_width(const char *text, const font_t *font) {
    return draw_text_width_styled(text, font, STYLE_NORMAL);
}

static int text_width(const char *text, unsigned char size, unsigned char style) {
    return draw_text_width_styled(text, draw_font_for(size), (font_style_t)style);
}

layout_font_t draw_font = {
    text_width,
    4, 2,                   /* 4 sizes, normal math is 9x15 */
    { 7, 11, 12, 16 },      /* ascents of the default ladder, draw_set_ladder() refills these */
    { 1, 2, 3, 4 }
};

void draw_set_ladder(const font_t *const *fonts, int count, int base) {
    int i;

    if(count > LAYOUT_MAX_SIZES)
        count = LAYOUT_MAX_SIZES;

    for(i = 0; i < count; i++) {
        ladder[i] = fonts[i];
        draw_font.ascent[i] = fonts[i]->ascent;
        draw_font.descent[i] = fonts[i]->descent;
    }
    draw_font.sizes = (unsigned char)count;
    draw_font.base = (unsigned char)(base < count ? base : count - 1);
}

void draw_text(int x, int top, const char *text, const font_t *font) {
    draw_text_styled(x, top, text, font, STYLE_NORMAL);
}

/* each glyph row as runs of horizontal lines (way faster than per-pixel, dont touch).
   bold ORs each row onto itself shifted a pixel, same trick bdf2c bakes into the
   constant e/i. italic shears each row by how far it is from the baseline, so the top
   leans right and descenders lean left. bold moves on one more per glyph and
   draw_text_width_styled counts the same, so the boxes fit */
void draw_text_styled(int x, int top, const char *text, const font_t *font, font_style_t style) {
    int height = font->ascent + font->descent;

    for(; *text != '\0'; text++) {
        int code = (unsigned char)*text & 0x7F;
        const unsigned short *rows = &font->rows[code * height];
        int r;

        for(r = 0; r < height; r++) {
            unsigned short bits = rows[r];
            int col = 0;
            int lean = (style & STYLE_ITALIC) ? (font->ascent - r) / 3 : 0;

            if(style & STYLE_BOLD)
                bits |= bits >> 1;

            while(bits != 0) {
                int start, len = 0;

                while(!(bits & 0x8000)) {
                    bits <<= 1;
                    col++;
                }
                start = col;
                while(bits & 0x8000) {
                    bits <<= 1;
                    col++;
                    len++;
                }
                screen_hline(x + start + lean, top + r, len);
            }
        }

        x += font->advance[code] + (style & STYLE_BOLD ? 1 : 0);
    }
}

void draw_softkeys(const char *const names[5], int lit) {
    const font_t *font = draw_font_for(0);     /* smallest, five names have to fit somehow */
    int i, w = SCREEN_W / 5, top = SCREEN_H - BAR_H;

    for(i = 0; i < 5; i++) {
        const char *name = names[i];
        int x = i * w + (w - draw_text_width(name, font)) / 2;

        if(i == lit) {
            screen_color(COL_ACCENT);
            screen_fill(i * w + 1, top, w - 2, BAR_H);
            screen_color(COL_BG);
        } else {
            screen_color(COL_DIM);
        }
        draw_text(x, top + (BAR_H - font->ascent - font->descent) / 2, name, font);
    }
}

/* the shapes (brackets, root sign) */

/*
    a bracket from row top to row bottom. the ends curve in over a quarter of
    its height each, so tall brackets stay round instead of looking like [ ]:

          ..#      mirrored for the right one, the
          .#       column next to the content is a gap
          #
          #
          .#
          ..#
*/
static void bracket(int x, int top, int bottom, bool left) {
    int curve = (bottom - top + 1) / 4;
    int out = left ? x + 3 : x + 1;   /* the curved ends */
    int mid = left ? x + 1 : x + 3;

    if(curve < 1)
        curve = 1;

    screen_line(out, top, mid, top + curve);
    screen_vline(mid, top + curve, bottom - top - 2 * curve + 1);
    screen_line(mid, bottom - curve, out, bottom);
}

/*
    √ before the radicand, rising to the bar:

             .......####  <- bar (top)
        #.   .
         #. .
          ##          <- bottom
*/
static void radical(int x, int top, int bottom, int bar_end) {
    int tick = (top + 2 * bottom) / 3;
    int rise = x + ROOT_SIGN_W - 3; /* where the sign meets the bar, 2px before the radicand */

    screen_line(x, tick, x + 1, tick);
    screen_line(x + 1, tick, x + 3, bottom);
    screen_line(x + 3, bottom, rise, top);
    screen_hline(rise, top, bar_end - rise);
}

/* --- boxes --- */

void draw_box(const box_t *b, int x, int y) {
    const box_t *k;
    int top, bottom;

    x += b->x;
    y += b->y;
    top = y - b->ascent;
    bottom = y + b->descent - 1;

    switch(b->kind) {
    case BOX_TEXT: {
        const font_t *font = draw_font_for(b->size);
        draw_text_styled(x, y - font->ascent, b->text, font, (font_style_t)b->style);
        break;
    }

    case BOX_FRAC:
        screen_hline(x, y + b->aux, b->width);
        break;

    case BOX_ROOT: {
        const box_t *rad = b->child;
        radical(x + rad->x - ROOT_SIGN_W, y + b->aux, bottom, x + b->width);
        break;
    }

    case BOX_PARENS:
        bracket(x, top, bottom, true);
        /* a ghost ) the editor would close for you, so it's there but not loud */
        if(b->aux)
            screen_color(COL_DIM);
        bracket(x + b->width - PAREN_W, top, bottom, false);
        if(b->aux)
            screen_color(COL_FG);
        break;

    case BOX_ABS:
        screen_vline(x + 1, top, bottom - top + 1);
        screen_vline(x + b->width - 2, top, bottom - top + 1);
        break;

    case BOX_CURSOR:
        /* one pixel left of the gap it sits in, so it doesn't cover the next glyph */
        screen_color(COL_ACCENT);
        screen_vline(x - 1, top, bottom - top + 1);
        screen_color(COL_FG);
        break;

    case BOX_SELECT:
        /* children are drawn after this, on top */
        screen_color(COL_SELECT);
        screen_fill(x, top, b->width, bottom - top + 1);
        screen_color(COL_FG);
        break;

    case BOX_SLOT: {
        /* dotted outline, one pixel in from the box */
        int i, l = x + 1, r = x + b->width - 2, t = top + 1, bt = bottom - 1;

        screen_color(COL_DIM);
        for(i = l; i <= r; i += 2) {
            screen_hline(i, t, 1);
            screen_hline(i, bt, 1);
        }
        for(i = t; i <= bt; i += 2) {
            screen_vline(l, i, 1);
            screen_vline(r, i, 1);
        }
        screen_color(COL_FG);
        break;
    }

    default:
        break;      /* TODO other box kinds if i end up needing them */
    }

    for(k = b->child; k != NULL; k = k->next)
        draw_box(k, x, y);
}
