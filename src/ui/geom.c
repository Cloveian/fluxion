#include "geom.h"

#include "app.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "engine/ast.h"
#include "engine/parser.h"
#include "engine/cas/cas.h"
#include "render/draw.h"

/*
    coordinate geometry, run by the app like asym( and system(. the line gets cut
    at the top level commas, each piece is parsed on its own and the answer is
    put together from those. nothing here needs the engine to know any geometry,
    it's all + - * / and one sqrt
*/

#define ARGS_MAX 4

enum { G_SLOPE, G_DIST, G_MID, G_PAR, G_PERP, G_COUNT };

static const struct {
    const char *name, *usage;
    int args;
} cmds[G_COUNT] = {
    { "slope", "slope(x1, y1, x2, y2)", 4 },
    { "dist",  "dist(x1, y1, x2, y2)", 4 },
    { "mid",   "mid(x1, y1, x2, y2)", 4 },
    { "par",   "par(m, x1, y1)", 3 },
    { "perp",  "perp(m, x1, y1)", 3 },
};

/* a - b. there's no subtract node, it's a plus the negative */
static pcas_ast_t *minus(pcas_ast_t *a, pcas_ast_t *b) {
    return ast_MakeBinary(OP_ADD, a, ast_MakeBinary(OP_MULT, ast_MakeNumber(num_FromInt(-1)), b));
}

static pcas_ast_t *over_two(pcas_ast_t *e) {
    return ast_MakeBinary(OP_DIV, e, ast_MakeNumber(num_FromInt(2)));
}

static pcas_ast_t *squared(pcas_ast_t *e) {
    return ast_MakeBinary(OP_POW, e, ast_MakeNumber(num_FromInt(2)));
}

/* the argument pieces parsed, how many there are. more than ARGS_MAX parses
   nothing and says 0, which is the wrong count for all of them. -1 for a
   parse error, err says which */
static int read_args(char *src, pcas_ast_t **args, pcas_error_t *err) {
    char *piece[ARGS_MAX], *s;
    int depth = 0, pieces = 1, i;

    piece[0] = src;
    for(s = src; *s != '\0'; s++) {
        if(*s == '(')
            depth++;
        else if(*s == ')')
            depth--;
        else if(depth == 0 && *s == ',') {
            if(pieces == ARGS_MAX)
                return 0;
            *s = '\0';
            piece[pieces++] = s + 1;
        }
    }

    for(i = 0; i < pieces; i++) {
        args[i] = parse((const uint8_t *)piece[i], strlen(piece[i]), str_table, err);
        if(*err != E_SUCCESS || args[i] == NULL) {
            if(*err == E_SUCCESS)
                *err = E_GENERIC;
            while(i >= 0)
                ast_Cleanup(args[i--]);
            return -1;
        }
    }
    return pieces;
}

/* ( a, b ), the same pieces solutions_box puts between its values */
static box_t *point_box(pcas_ast_t *x, pcas_ast_t *y) {
    box_t *row = layout_box(BOX_ROW, 0);

    layout_add(row, layout_text("(", 0));
    layout_add(row, value_box(x));
    layout_add(row, layout_text(",", 0));
    layout_add(row, layout_space(2 * SPACE_BINARY));
    layout_add(row, value_box(y));
    layout_add(row, layout_text(")", 0));
    layout_finish(row, &draw_font);
    return row;
}

bool geom_run(const char *cmd, char *src, box_t *in) {
    pcas_ast_t *a[ARGS_MAX] = { NULL }, *e = NULL, *f = NULL, *m;
    pcas_error_t err = E_SUCCESS;
    const char *message = NULL;
    box_t *out = NULL;
    clock_t start;
    int c, n, i;

    if(cmd == NULL)
        return false;
    for(c = 0; c < G_COUNT; c++)
        if(strcmp(cmd, cmds[c].name) == 0)
            break;
    if(c == G_COUNT)
        return false;

    n = read_args(src, a, &err);
    if(n != cmds[c].args) {
        for(i = 0; i < n; i++)
            ast_Cleanup(a[i]);
        push(in, NULL, n < 0 ? error_text[err] : cmds[c].usage, 0);
        return true;
    }

    /* x1 y1 x2 y2 for the point ones, m x1 y1 for the line ones */
    start = clock();
    switch(c) {
    case G_SLOPE:
        /* the bottom on its own first, so a vertical line is a message and not a
           division by zero somewhere in simplify */
        f = minus(ast_Copy(a[2]), ast_Copy(a[0]));
        simplify(f, SIMP_ALL);
        if(is_ast_int(f, 0)) {
            message = "undefined, it's vertical";
            break;
        }
        e = ast_MakeBinary(OP_DIV, minus(ast_Copy(a[3]), ast_Copy(a[1])), f);
        f = NULL;
        simplify(e, SIMP_ALL);
        break;
    case G_DIST:
        e = ast_MakeBinary(OP_ROOT, ast_MakeNumber(num_FromInt(2)),
                           ast_MakeBinary(OP_ADD, squared(minus(ast_Copy(a[2]), ast_Copy(a[0]))),
                                          squared(minus(ast_Copy(a[3]), ast_Copy(a[1])))));
        simplify(e, SIMP_ALL);
        break;
    case G_MID:
        e = over_two(ast_MakeBinary(OP_ADD, ast_Copy(a[0]), ast_Copy(a[2])));
        f = over_two(ast_MakeBinary(OP_ADD, ast_Copy(a[1]), ast_Copy(a[3])));
        simplify(e, SIMP_ALL);
        simplify(f, SIMP_ALL);
        break;
    default:
        m = ast_Copy(a[0]);
        simplify(m, SIMP_ALL);
        if(c == G_PERP) {
            /* flat line, so the perpendicular one is straight up and down */
            if(is_ast_int(m, 0)) {
                ast_Cleanup(m);
                f = ast_Copy(a[1]);
                simplify(f, SIMP_ALL);
                break;
            }
            m = ast_MakeBinary(OP_DIV, ast_MakeNumber(num_FromInt(-1)), m);
            simplify(m, SIMP_ALL);
        }
        /* y = m x + b with b = y1 - m x1 */
        e = ast_MakeBinary(OP_ADD, ast_MakeBinary(OP_MULT, ast_Copy(m), ast_MakeSymbol(SYM_X)),
                           minus(ast_Copy(a[2]), ast_MakeBinary(OP_MULT, m, ast_Copy(a[1]))));
        simplify(e, SIMP_ALL);
        break;
    }

    if(pcas_cancelled)
        message = pcas_oom ? "out of memory" : "cancelled";
    else if(message == NULL) {
        if(c == G_MID)
            out = point_box(e, f);
        else if(c == G_PERP && e == NULL)
            out = solutions_box('x', &f, 1);
        else if(c == G_PAR || c == G_PERP)
            out = solutions_box('y', &e, 1);
        else
            out = value_box(e);
    }
    push(in, out, message, (unsigned long)((clock() - start) * 1000 / CLOCKS_PER_SEC));

    ast_Cleanup(e);
    ast_Cleanup(f);
    for(i = 0; i < n; i++)
        ast_Cleanup(a[i]);
    return true;
}
