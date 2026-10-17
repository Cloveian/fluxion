#include "shapes.h"

#include "app.h"
#include "numval.h"
#include "settings.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "engine/ast.h"
#include "engine/parser.h"
#include "engine/cas/cas.h"
#include "editor/editor.h"
#include "render/draw.h"

/*
    the area / volume / whatever formulas for one shape, all of them in one go.
    the formulas are kept as engine text with $0 $1 $2 where the pieces go,
    pasted in and parsed fresh every time. a lot easier to read and fix than
    building pi r^2 out of ast_MakeBinary calls, and the parser's fast enough
    for ten lines. letters are fine, circle(r) gives the formula itself
*/

#define ARGS_MAX 3
#define LINES_MAX 4
#define EPS 1e-5F

enum { S_CIRCLE, S_RECT, S_SQUARE, S_TRAP, S_SPHERE, S_CYL, S_CONE, S_CUBE, S_BOX, S_POLYGON, S_COUNT };

/* @ is half a turn, 180 or pi by the angle mode. a line with one in it is an
   angle and gets the degree sign. and _ is minus, - is the negative sign in
   str_table */
static const struct {
    const char *name, *usage;
    int args;
    struct {
        const char *label, *formula;
    } lines[LINES_MAX];
} shapes[S_COUNT] = {
    { "circle", "circle(r)", 1,
      { { "area", "pi*$0^2" }, { "circumference", "2*pi*$0" }, { "diameter", "2*$0" } } },
    { "rect", "rect(l, w)", 2,
      { { "area", "$0*$1" }, { "perimeter", "2*$0+2*$1" }, { "diagonal", "sqrt($0^2+$1^2)" } } },
    { "square", "square(s)", 1,
      { { "area", "$0^2" }, { "perimeter", "4*$0" }, { "diagonal", "$0*sqrt(2)" } } },
    { "trap", "trap(a, b, h), a b the parallel sides", 3,
      { { "area", "($0+$1)*$2/2" } } },
    { "sphere", "sphere(r)", 1,
      { { "volume", "4/3*pi*$0^3" }, { "surface area", "4*pi*$0^2" } } },
    { "cyl", "cyl(r, h)", 2,
      { { "volume", "pi*$0^2*$1" }, { "surface area", "2*pi*$0^2+2*pi*$0*$1" }, { "side area", "2*pi*$0*$1" } } },
    /* the slant goes into the surface area whole, sqrt and all */
    { "cone", "cone(r, h)", 2,
      { { "volume", "pi*$0^2*$1/3" }, { "slant", "sqrt($0^2+$1^2)" },
        { "surface area", "pi*$0^2+pi*$0*sqrt($0^2+$1^2)" } } },
    { "cube", "cube(s)", 1,
      { { "volume", "$0^3" }, { "surface area", "6*$0^2" }, { "diagonal", "$0*sqrt(3)" } } },
    { "box", "box(l, w, h)", 3,
      { { "volume", "$0*$1*$2" }, { "surface area", "2*($0*$1+$0*$2+$1*$2)" }, { "diagonal", "sqrt($0^2+$1^2+$2^2)" } } },
    /* regular polygon for the each-angle ones, the sum and diagonals don't care */
    { "polygon", "polygon(n)", 1,
      { { "angle sum", "($0_2)*@" }, { "interior", "($0_2)*@/$0" }, { "exterior", "2*@/$0" },
        { "diagonals", "$0*($0_3)/2" } } },
};

/* cut at the top level commas. 0 when there are more than ARGS_MAX */
static int split(char *src, char **piece) {
    char *s;
    int depth = 0, pieces = 1;

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
    return pieces;
}

static pcas_ast_t *read_text(const char *s, pcas_error_t *err) {
    pcas_ast_t *e;

    *err = E_SUCCESS;
    e = parse((const uint8_t *)s, strlen(s), str_table, err);
    if(*err != E_SUCCESS || e == NULL) {
        ast_Cleanup(e);
        if(*err == E_SUCCESS)
            *err = E_GENERIC;
        return NULL;
    }
    if(setting_angle == ANGLE_DEGREES)
        e = degrees_in(e);
    return e;
}

/* each piece on its own first, so a typo gets the parser's message and not some
   mess from inside a formula. only plain numbers get judged, letters go through */
static const char *check(const char *s, bool sides) {
    pcas_error_t err;
    pcas_ast_t *e = read_text(s, &err);
    const char *msg = NULL;
    float x;

    if(e == NULL)
        return error_text[err];
    simplify(e, SIMP_ALL);
    approx(e);
    if(!pcas_cancelled && num_float(e, &x)) {
        if(sides && (x < 3 - EPS || fabsf(x - floorf(x + 0.5F)) > EPS))
            msg = "n has to be a whole number, 3 or more";
        else if(!sides && x < 0)
            msg = "no negative lengths";
    }
    ast_Cleanup(e);
    return msg;
}

/* the formula with the pieces in, each in brackets so 2*$0 with a+b in it
   stays 2(a+b). malloc'd, NULL when that fails */
static char *fill(const char *f, char **piece) {
    const char *half = setting_angle == ANGLE_DEGREES ? "180" : "pi", *s;
    size_t len = 1;
    char *out, *o;

    for(s = f; *s != '\0'; s++) {
        if(*s == '$')
            len += strlen(piece[*++s - '0']) + 2;
        else if(*s == '@')
            len += strlen(half);
        else
            len++;
    }
    if((out = malloc(len)) == NULL)
        return NULL;

    for(s = f, o = out; *s != '\0'; s++) {
        if(*s == '$') {
            const char *p = piece[*++s - '0'];

            *o++ = '(';
            strcpy(o, p);
            o += strlen(p);
            *o++ = ')';
        } else if(*s == '@') {
            strcpy(o, half);
            o += strlen(half);
        } else
            *o++ = *s;
    }
    *o = '\0';
    return out;
}

/* "area = 9pi", one line of the answer, added to col */
static const char *add_line(box_t *col, const char *label, const char *formula, char **piece) {
    static const char deg[] = { CH_DEG, '\0' };
    char *text = fill(formula, piece);
    pcas_error_t err;
    pcas_ast_t *e;
    box_t *row;

    if(text == NULL)
        return "out of memory";
    e = read_text(text, &err);
    free(text);
    if(e == NULL)
        return error_text[err];
    simplify(e, SIMP_ALL);
    if(pcas_cancelled) {
        ast_Cleanup(e);
        return NULL;
    }

    row = layout_box(BOX_ROW, 0);
    layout_add(row, layout_text(label, 0));
    layout_add(row, layout_space(LAYOUT_SPACE(0)));
    layout_add(row, layout_text("=", 0));
    layout_add(row, layout_space(LAYOUT_SPACE(0)));
    layout_add(row, value_box(e));
    if(strchr(formula, '@') != NULL && setting_angle == ANGLE_DEGREES)
        layout_add(row, layout_text(deg, 0));
    layout_finish(row, &draw_font);
    layout_add(col, row);
    ast_Cleanup(e);
    return NULL;
}

bool shapes_run(const char *cmd, char *src, box_t *in) {
    char *piece[ARGS_MAX];
    const char *message = NULL;
    box_t *out = NULL;
    clock_t start;
    int c, n, i;

    if(cmd == NULL)
        return false;
    for(c = 0; c < S_COUNT; c++)
        if(strcmp(cmd, shapes[c].name) == 0)
            break;
    if(c == S_COUNT)
        return false;

    n = split(src, piece);
    if(n != shapes[c].args) {
        push(in, NULL, shapes[c].usage, 0);
        return true;
    }

    start = clock();
    for(i = 0; i < n && message == NULL && !pcas_cancelled; i++)
        message = check(piece[i], c == S_POLYGON);

    if(message == NULL && !pcas_cancelled) {
        out = layout_box(BOX_COL, 0);
        for(i = 0; i < LINES_MAX && shapes[c].lines[i].label != NULL; i++) {
            message = add_line(out, shapes[c].lines[i].label, shapes[c].lines[i].formula, piece);
            if(message != NULL || pcas_cancelled)
                break;
        }
        layout_finish(out, &draw_font);
    }

    if(pcas_cancelled)
        message = pcas_oom ? "out of memory" : "cancelled";
    if(message != NULL) {
        layout_free(out);
        out = NULL;
    }
    push(in, out, message, (unsigned long)((clock() - start) * 1000 / CLOCKS_PER_SEC));
    return true;
}
