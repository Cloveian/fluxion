/* draws expressions and their simplified results the way the calculator will,
   history style (input left, result right).

   ./build-pc/view 'X^2/2' '(X+1)^2/sqrt(X)'              window (Esc to close)
   ./build-pc/view --png out.png 'X^2/2' '(X+1)^2/sqrt(X)'  just save a PNG
   ./build-pc/view --scale 3 --png out.png ...            3x bigger, to see pixels
   ./build-pc/view --ladder 5x7,6x10,8x13 ...             fonts, smallest first */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine/ast.h"
#include "engine/parser.h"
#include "engine/cas/cas.h"
#include "engine/cas/identities.h"
#include "render/layout.h"
#include "render/draw.h"
#include "platform/screen.h"
#include "platform/window_pc.h"

#define MARGIN 4
#define GAP    4    /* between input and result, and between entries */

static bool raw;    /* --raw draws what was typed, no simplify (for layout work) */

static const font_t *all_fonts[] = {
    &font_4_6, &font_5_7, &font_5_8, &font_6_9, &font_6_10, &font_6_12,
    &font_6_13, &font_7_13, &font_7_14, &font_8_13, &font_9_15, &font_10_20
};

/* --ladder 5x8,7x13,*9x15,10x20, smallest first, the * is normal math */
static bool set_ladder(const char *spec) {
    const font_t *picked[LAYOUT_MAX_SIZES];
    char buf[96], *name;
    int n = 0, base = -1;
    unsigned i;

    strncpy(buf, spec, sizeof buf - 1);
    buf[sizeof buf - 1] = '\0';
    for(name = strtok(buf, ","); name != NULL && n < LAYOUT_MAX_SIZES; name = strtok(NULL, ",")) {
        if(name[0] == '*') {
            base = n;
            name++;
        }
        for(i = 0; i < sizeof all_fonts / sizeof all_fonts[0]; i++)
            if(strcmp(all_fonts[i]->name, name) == 0)
                break;
        if(i == sizeof all_fonts / sizeof all_fonts[0]) {
            fprintf(stderr, "no font %s\n", name);
            return false;
        }
        picked[n++] = all_fonts[i];
    }
    if(n == 0 || base < 0) {
        fprintf(stderr, "--ladder: fonts smallest first, mark normal math with *\n");
        return false;
    }

    draw_set_ladder(picked, n, base);
    return true;
}

/* draws one entry with its top at y. returns the y below it. */
static int draw_entry(const char *input, int y) {
    pcas_error_t err;
    pcas_ast_t *e = parse((const uint8_t *)input, strlen(input), str_table, &err);
    box_t *in, *out;

    if(err != E_SUCCESS || e == NULL) {
        screen_color(COL_DIM);
        draw_text(MARGIN, y, error_text[err], draw_label_font());
        return y + 10 + GAP;
    }

    in = layout_build(e, &draw_font);
    out = NULL;
    if(!raw) {
        simplify(e, SIMP_ALL);
        simplify_canonical_form(e, CANONICAL_ALL);
        out = layout_build(e, &draw_font);
    }

    if(in != NULL) {
        screen_color(COL_FG);
        draw_box(in, MARGIN, y + in->ascent);
        y += in->ascent + in->descent + GAP;
    }

    if(out != NULL) {
        screen_color(COL_FG);
        draw_box(out, SCREEN_W - MARGIN - out->width, y + out->ascent);
        y += out->ascent + out->descent + GAP;
    }

    screen_color(COL_DIM);
    screen_hline(MARGIN, y, SCREEN_W - 2 * MARGIN);

    layout_free(in);
    layout_free(out);
    ast_Cleanup(e);
    return y + 1 + GAP;
}

int main(int argc, char **argv) {
    const char *png = NULL;
    int i = 1, y = MARGIN, scale = 1;

    for(; i + 1 < argc && argv[i][0] == '-' && argv[i][1] == '-'; i += 2) {
        if(strcmp(argv[i], "--raw") == 0) {
            raw = true;
            i--;    /* takes no value */
        } else if(strcmp(argv[i], "--png") == 0)
            png = argv[i + 1];
        else if(strcmp(argv[i], "--scale") == 0)
            scale = atoi(argv[i + 1]);
        else if(strcmp(argv[i], "--ladder") == 0 && !set_ladder(argv[i + 1]))
            return 1;
    }

    screen_begin();
    screen_clear();
    for(; i < argc; i++)
        y = draw_entry(argv[i], y);
    id_UnloadAll();

    if(png != NULL)
        return screen_save_png(png, scale) == 0 ? 0 : 1;
    return window_show("Fluxion") == 0 ? 0 : 1;
}
