/* runs a script of editor steps, prints what the engine gets, and draws the editor
   (with its cursor), the engine string and the simplified result.

   ./build-pc/editor-test [--png out.png] [--scale 2] step step step ...

   steps:
     t:2x+1     type these characters (- is subtract, ~ is negate, * is times)
     frac pow sqrt nroot abs logb       insert that template
     f:sin      insert a function (any engine name: sin, asin, ln, log, int, ...)
     pi E I     the constants pi, e, i
     L R U D    arrows        bs  backspace     del  delete     clear */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine/ast.h"
#include "engine/parser.h"
#include "engine/cas/cas.h"
#include "engine/cas/identities.h"
#include "editor/editor.h"
#include "render/draw.h"
#include "platform/screen.h"

#define MARGIN 4

static bool step(editor_t *ed, const char *s) {
    if(strncmp(s, "t:", 2) == 0) {
        for(s += 2; *s != '\0'; s++) {
            char c = *s == '-' ? ED_SUB : *s == '~' ? ED_NEG : *s;
            if(!ed_insert_char(ed, c))
                fprintf(stderr, "  couldn't type '%c'\n", *s);
        }
        return true;
    }
    if(strncmp(s, "f:", 2) == 0)       return ed_insert(ed, ED_FUNC, s + 2);
    if(strcmp(s, "frac") == 0)         return ed_insert(ed, ED_FRAC, NULL);
    if(strcmp(s, "pow") == 0)          return ed_insert(ed, ED_POW, NULL);
    if(strcmp(s, "sqrt") == 0)         return ed_insert(ed, ED_SQRT, NULL);
    if(strcmp(s, "nroot") == 0)        return ed_insert(ed, ED_NROOT, NULL);
    if(strcmp(s, "abs") == 0)          return ed_insert(ed, ED_ABS, NULL);
    if(strcmp(s, "logb") == 0)         return ed_insert(ed, ED_LOGB, NULL);
    if(strcmp(s, "pi") == 0)           return ed_insert_char(ed, CH_PI);
    if(strcmp(s, "E") == 0)            return ed_insert_char(ed, CH_EULER);
    if(strcmp(s, "I") == 0)            return ed_insert_char(ed, CH_IMAG);
    if(strcmp(s, "L") == 0)            { ed_left(ed); return true; }
    if(strcmp(s, "R") == 0)            { ed_right(ed); return true; }
    if(strcmp(s, "U") == 0)            { ed_up(ed); return true; }
    if(strcmp(s, "D") == 0)            { ed_down(ed); return true; }
    if(strcmp(s, "bs") == 0)           { ed_backspace(ed); return true; }
    if(strcmp(s, "del") == 0)          { ed_delete(ed); return true; }
    if(strcmp(s, "clear") == 0)        { ed_clear(ed); return true; }

    fprintf(stderr, "unknown step %s\n", s);
    return false;
}

int main(int argc, char **argv) {
    const char *png = NULL;
    int i = 1, scale = 2, y = MARGIN;
    editor_t ed;
    box_t *b;
    char *engine;

    for(; i + 1 < argc && strncmp(argv[i], "--", 2) == 0; i += 2) {
        if(strcmp(argv[i], "--png") == 0)
            png = argv[i + 1];
        else if(strcmp(argv[i], "--scale") == 0)
            scale = atoi(argv[i + 1]);
    }

    ed_init(&ed);
    for(; i < argc; i++)
        if(!step(&ed, argv[i]))
            return 1;

    engine = ed_to_engine(&ed);
    printf("%s\n", engine != NULL ? engine : "(unfinished)");

    screen_begin();
    screen_clear();

    /* the input line, with its cursor */
    b = ed_layout(&ed, &draw_font, true);
    screen_color(COL_FG);
    draw_box(b, MARGIN, y + b->ascent);
    y += b->ascent + b->descent + 6;
    layout_free(b);

    /* what the engine gets */
    screen_color(COL_DIM);
    draw_text(MARGIN, y, engine != NULL ? engine : "(unfinished)", draw_label_font());
    y += 16;

    /* and the simplified result */
    if(engine != NULL) {
        pcas_error_t err;
        pcas_ast_t *e = parse((const uint8_t *)engine, strlen(engine), str_table, &err);

        if(err == E_SUCCESS && e != NULL) {
            simplify(e, SIMP_ALL);
            simplify_canonical_form(e, CANONICAL_ALL);
            b = layout_build(e, &draw_font);
            screen_color(COL_FG);
            draw_box(b, SCREEN_W - MARGIN - b->width, y + b->ascent);
            layout_free(b);
            ast_Cleanup(e);
        } else {
            draw_text(MARGIN, y, error_text[err], draw_label_font());
        }
    }

    free(engine);
    ed_free(&ed);
    id_UnloadAll();

    return png != NULL && screen_save_png(png, scale) != 0 ? 1 : 0;
}
