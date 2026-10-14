#include <ti/getcsc.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "engine/ast.h"
#include "engine/parser.h"
#include "engine/cas/cas.h"
#include "engine/cas/identities.h"
#include "render/layout.h"
#include "render/draw.h"
#include "platform/screen.h"

#define MARGIN 4
#define GAP    4

/* Placeholder until the editor exists: simplify a fixed expression and draw
   input + result like a history entry. Returns the y below the entry. */
static int show(const char *input, int y)
{
    pcas_error_t err;
    pcas_ast_t *e;
    box_t *in, *out;
    clock_t start;
    char timing[16];

    e = parse((const uint8_t *)input, strlen(input), str_table, &err);
    if (err != E_SUCCESS || e == NULL) {
        screen_color(COL_DIM);
        draw_text(MARGIN, y, error_text[err], draw_label_font());
        return y + 10 + GAP;
    }

    in = layout_build(e, &draw_font);

    start = clock();
    simplify(e, SIMP_ALL);
    simplify_canonical_form(e, CANONICAL_ALL);
    /* Temporary: how long simplifying took */
    sprintf(timing, "%lums", (unsigned long)((clock() - start) * 1000 / CLOCKS_PER_SEC));

    out = layout_build(e, &draw_font);

    if (in != NULL) {
        screen_color(COL_FG);
        draw_box(in, MARGIN, y + in->ascent);
        y += in->ascent + in->descent + GAP;
    }

    if (out != NULL) {
        const font_t *small = draw_label_font();

        screen_color(COL_DIM);
        draw_text(MARGIN, y + out->ascent - small->ascent, timing, small);
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

int main(void)
{
    int y = MARGIN;

    screen_begin();
    screen_clear();

    y = show("(X+1)^2/sqrt(X)", y);
    y = show("(5/13)/(5/12)", y);
    y = show("sin(X)^2+cos(X)^2", y);
    y = show("asin(X)+logb(X,3)", y);

    screen_show();
    while (!os_GetCSC());

    screen_end();
    id_UnloadAll();
    return 0;
}
