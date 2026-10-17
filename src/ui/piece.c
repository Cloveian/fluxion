#include "piece.h"

#include "app.h"
#include "numval.h"
#include "settings.h"

#include <math.h>
#include <string.h>
#include <time.h>

#include "engine/ast.h"
#include "engine/parser.h"
#include "engine/cas/cas.h"
#include "editor/editor.h"
#include "render/draw.h"

/*
    piecewise(f1, c1, f2, c2, ..., x=v). the engine has no conditions and its
    parser has no < or >, so the line never goes to it whole. the arguments get
    split at the commas, then each condition again at its signs, and every bit
    that's left is plain enough to parse. no x=v and it just shows the pieces.
    with one the conditions get checked as floats in order, and the first that
    holds is worked out exactly
*/

#define PIECES_MAX 8
#define ARGS_MAX (2 * PIECES_MAX + 1)
/* 1 < x <= 3 is 3 sides, room for one more */
#define SIDES_MAX 4
#define EPS 1e-5F
/* a to z are bits 0-25 in the letter masks, theta goes after */
#define THETA_BIT 26

typedef struct {
    pcas_ast_t *side[SIDES_MAX];
    char rel[SIDES_MAX - 1]; /* the way it's drawn, < > CH_LE CH_GE CH_NE or = */
    int n;
    unsigned long letters;
} cond_t;

/* the messages are all plain strings and don't say which condition, push keeps
   the pointer so a buffer would get rewritten under the older history lines */
static const char *usage = "piecewise(f1, c1, f2, c2, ..., x=v)";

/* 0 when there are more than ARGS_MAX, and src is half cut up by then */
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

/* NULL and err set on a parse error. degrees go in straight away like the
   main line does it, so sin(30) < x means what it says */
static pcas_ast_t *read_piece(const char *s, pcas_error_t *err) {
    pcas_ast_t *e;

    *err = E_SUCCESS;
    e = parse((const uint8_t *)s, strlen(s), str_table, err);
    if(*err != E_SUCCESS || e == NULL) {
        if(e != NULL)
            ast_Cleanup(e);
        if(*err == E_SUCCESS)
            *err = E_GENERIC;
        return NULL;
    }
    if(setting_angle == ANGLE_DEGREES)
        e = degrees_in(e);
    return e;
}

/* simplified for showing, degrees back out after */
static void exact(pcas_ast_t *e) {
    simplify(e, SIMP_ALL);
    if(setting_angle == ANGLE_DEGREES)
        degrees_out(e);
}

static unsigned long letters(const pcas_ast_t *e) {
    const pcas_ast_t *k;
    unsigned long m = 0;

    if(e->type == NODE_SYMBOL) {
        if(e->op.symbol >= SYM_A && e->op.symbol <= SYM_Z)
            return 1UL << (e->op.symbol - SYM_A);
        return e->op.symbol == SYM_THETA ? 1UL << THETA_BIT : 0;
    }
    if(e->type != NODE_OPERATOR)
        return 0;
    for(k = e->op.operator.base; k != NULL; k = k->next)
        m |= letters(k);
    return m;
}

static bool is_rel(char c) {
    return c == '<' || c == '>' || c == CH_LE || c == CH_GE || c == CH_NE || c == '=';
}

/*
    s cut at its top level signs and the sides parsed into c. the [math] popup
    ones come out of the editor as their own chars, but < then = typed as two
    is read as <= too. = on its own is allowed, x = 0 is a fine condition.
    NULL when it worked, a message when not (c->n says how many got parsed)
*/
static const char *read_cond(char *s, cond_t *c) {
    char *side[SIDES_MAX];
    pcas_error_t err;
    int depth = 0, n = 1, k;

    c->n = 0;
    c->letters = 0;
    side[0] = s;
    for(; *s != '\0'; s++) {
        bool two;
        char r;

        if(*s == '(')
            depth++;
        else if(*s == ')')
            depth--;
        if(depth != 0 || !is_rel(*s))
            continue;
        two = (*s == '<' || *s == '>') && s[1] == '=';
        r = two ? (*s == '<' ? CH_LE : CH_GE) : *s;
        if(n == SIDES_MAX)
            return "chains go up to 3 signs";
        *s = '\0';
        if(two)
            *++s = '\0';
        c->rel[n - 1] = r;
        side[n++] = s + 1;
    }
    if(n == 1)
        return "a condition isn't an inequality";

    for(k = 0; k < n; k++) {
        if((c->side[k] = read_piece(side[k], &err)) == NULL)
            return error_text[err];
        c->n = k + 1;
        c->letters |= letters(c->side[k]);
    }
    if(c->letters == 0)
        return "a condition has no letter in it";
    return NULL;
}

static void cond_free(cond_t *c) {
    int k;

    for(k = 0; k < c->n; k++)
        ast_Cleanup(c->side[k]);
    c->n = 0;
}

/* 1 when c holds with var = v, 0 when it doesn't, -1 when a side isn't a plain
   number there (another letter, or i). a bit of slack on the = part, so 1/3
   against 0.333333 still counts as the edge */
static int holds(const cond_t *c, pcas_ast_t *var, pcas_ast_t *v) {
    float x[SIDES_MAX];
    int k;

    for(k = 0; k < c->n; k++) {
        pcas_ast_t *e = ast_Copy(c->side[k]);
        bool ok;

        substitute(e, var, v);
        simplify(e, SIMP_ALL);
        approx(e);
        ok = num_float(e, &x[k]) && isfinite(x[k]);
        ast_Cleanup(e);
        if(!ok)
            return -1;
    }

    for(k = 0; k + 1 < c->n; k++) {
        float a = x[k], b = x[k + 1], big = fabsf(a) > fabsf(b) ? fabsf(a) : fabsf(b);
        bool eq = fabsf(a - b) <= EPS * (big > 1 ? big : 1), ok;

        switch(c->rel[k]) {
        case '<': ok = a < b && !eq; break;
        case '>': ok = a > b && !eq; break;
        case CH_LE: ok = a < b || eq; break;
        case CH_GE: ok = a > b || eq; break;
        case CH_NE: ok = !eq; break;
        default: ok = eq; break;
        }
        if(!ok)
            return 0;
    }
    return 1;
}

/* 1 < x <= 3 with each side simplified, the signs spaced like domain's x >= 2 */
static box_t *cond_box(cond_t *c) {
    char op[2] = { 0, '\0' };
    box_t *row = layout_box(BOX_ROW, 0);
    int k;

    for(k = 0; k < c->n; k++) {
        if(k > 0) {
            op[0] = c->rel[k - 1];
            layout_add(row, layout_space(LAYOUT_SPACE(0)));
            layout_add(row, layout_text(op, 0));
            layout_add(row, layout_space(LAYOUT_SPACE(0)));
        }
        exact(c->side[k]);
        layout_add(row, value_box(c->side[k]));
    }
    return row;
}

/* "x^2,  if  x < 0", a row each, no brace (yet) */
static box_t *pieces_box(pcas_ast_t **f, cond_t *c, int pieces) {
    box_t *col = layout_box(BOX_COL, 0);
    int i;

    for(i = 0; i < pieces; i++) {
        box_t *row = layout_box(BOX_ROW, 0);

        exact(f[i]);
        layout_add(row, value_box(f[i]));
        layout_add(row, layout_text(",", 0));
        layout_add(row, layout_space(2 * SPACE_BINARY));
        layout_add(row, labelled("if", cond_box(&c[i])));
        layout_add(col, row);
    }
    layout_finish(col, &draw_font);
    return col;
}

bool piece_run(const char *cmd, char *src, box_t *in) {
    pcas_ast_t *f[PIECES_MAX], *v = NULL, *vn = NULL, *var = NULL, *e = NULL;
    cond_t c[PIECES_MAX];
    char *arg[ARGS_MAX], *at = NULL;
    pcas_error_t err = E_SUCCESS;
    const char *message = NULL;
    box_t *out = NULL;
    clock_t start;
    unsigned long both = ~0UL, bit = 0;
    Symbol sym = SYM_X;
    int n, pieces, got, i;

    if(cmd == NULL || strcmp(cmd, "piecewise") != 0)
        return false;

    n = split(src, arg);
    if(n == 0) {
        push(in, NULL, "8 pieces at most", 0);
        return true;
    }

    /* an odd one out at the end is where to look, x=v with a letter on the left */
    if(n % 2 == 1) {
        char *p = arg[n - 1];

        if(strncmp(p, "theta=", 6) == 0) {
            sym = SYM_THETA;
            bit = 1UL << THETA_BIT;
            at = p + 6;
        } else if(p[0] >= 'A' && p[0] <= 'Z' && p[1] == '=') {
            sym = (Symbol)p[0];
            bit = 1UL << (p[0] - 'A');
            at = p + 2;
        } else {
            push(in, NULL, "every piece needs a condition after it", 0);
            return true;
        }
    }
    pieces = n / 2;
    if(pieces == 0) {
        push(in, NULL, usage, 0);
        return true;
    }

    for(got = 0; got < pieces && message == NULL; got++) {
        f[got] = read_piece(arg[2 * got], &err);
        if(f[got] == NULL) {
            message = error_text[err];
            c[got].n = 0;
        } else if((message = read_cond(arg[2 * got + 1], &c[got])) == NULL)
            both &= c[got].letters;
    }
    if(message == NULL && both == 0)
        message = "the conditions are in two different letters";
    if(message == NULL && at != NULL) {
        if((both & bit) == 0)
            message = "the conditions aren't in x=v's letter";
        else if((v = read_piece(at, &err)) == NULL)
            message = error_text[err];
    }

    start = clock();
    if(message == NULL && at == NULL)
        out = pieces_box(f, c, pieces);
    else if(message == NULL) {
        int r = 0;

        /* the conditions only get the float of v, the piece gets v itself */
        var = ast_MakeSymbol((char)sym);
        vn = ast_Copy(v);
        simplify(vn, SIMP_ALL);
        approx(vn);
        for(i = 0; i < pieces; i++) {
            r = holds(&c[i], var, vn);
            if(r != 0 || pcas_cancelled)
                break;
        }
        if(r < 0)
            message = "a condition isn't a number there";
        else if(r == 0)
            message = "undefined there";
        else {
            e = ast_Copy(f[i]);
            substitute(e, var, v);
            exact(e);
        }
    }

    if(pcas_cancelled) {
        message = pcas_oom ? "out of memory" : "cancelled";
        layout_free(out);
        out = NULL;
    } else if(message == NULL && out == NULL)
        out = value_box(e);
    push(in, out, message, (unsigned long)((clock() - start) * 1000 / CLOCKS_PER_SEC));

    ast_Cleanup(e);
    ast_Cleanup(v);
    ast_Cleanup(vn);
    ast_Cleanup(var);
    for(i = 0; i < got; i++) {
        ast_Cleanup(f[i]);
        cond_free(&c[i]);
    }
    return true;
}
