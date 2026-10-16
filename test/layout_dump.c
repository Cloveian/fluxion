/* prints the box tree for an expression, after simplifying it.
   ./build-pc/layout-dump '(X+1)^2/sqrt(X)'
   uses an 8x8 monospace font (same size as graphx's built-in one) for measuring. */

#include <stdio.h>
#include <string.h>

#include "engine/ast.h"
#include "engine/parser.h"
#include "engine/cas/cas.h"
#include "engine/cas/identities.h"
#include "render/layout.h"

static int text_width(const char *text, unsigned char level, unsigned char style) {
    (void)style;    /* every glyph is 8 wide here anyway */
    (void)level;
    return 8 * (int)strlen(text);
}

/* 4 sizes, all the same 8x8 cell (positions are what matter here), normal math = size 2 */
static const layout_font_t font = { text_width, 4, 2, { 7, 7, 7, 7 }, { 1, 1, 1, 1 } };

static const char *kind_name(box_kind_t k) {
    static const char *names[] = { "TEXT", "SPACE", "ROW", "FRAC", "SUP", "SUB", "ROOT", "PARENS", "ABS" };
    return names[k];
}

static void print_text(const char *s) {
    for(; *s != '\0'; s++) {
        switch(*s) {
        case CH_PI:    printf("pi");    break;
        case CH_THETA: printf("theta"); break;
        case CH_MINUS: printf("-");     break;
        case CH_CDOT:  printf(".");     break;
        default:       putchar(*s);     break;
        }
    }
}

static void dump(box_t *b, int depth) {
    for(; b != NULL; b = b->next) {
        printf("%*s%-6s x=%-3d y=%-4d w=%-3d a=%-2d d=%-2d", depth * 2, "", kind_name(b->kind),
               b->x, b->y, b->width, b->ascent, b->descent);
        if(b->kind == BOX_FRAC || b->kind == BOX_ROOT)
            printf(" bar=%d", b->aux);
        if(b->level > 0)
            printf(" level=%d", b->level);
        if(b->kind == BOX_TEXT) {
            printf("  \"");
            print_text(b->text);
            printf("\"");
        }
        printf("\n");
        dump(b->child, depth + 1);
    }
}

int main(int argc, char **argv) {
    pcas_error_t err;
    pcas_ast_t *e;
    box_t *b;
    int i;

    for(i = 1; i < argc; i++) {
        e = parse((const uint8_t *)argv[i], strlen(argv[i]), str_table, &err);
        if(err != E_SUCCESS || e == NULL) {
            printf("%s: %s\n", argv[i], error_text[err]);
            continue;
        }

        simplify(e, SIMP_ALL);
        simplify_canonical_form(e, CANONICAL_ALL);

        b = layout_build(e, &font);
        printf("%s\n", argv[i]);
        dump(b, 1);
        printf("\n");

        layout_free(b);
        ast_Cleanup(e);
    }

    id_UnloadAll();
    return 0;
}
