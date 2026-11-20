/* the steps page. the engine works the steps out (cas/steps.c), this draws them */

#include "work.h"
#include "app.h"
#include "areas.h"
#include <string.h>
#include "platform/screen.h"
#include "render/draw.h"
#include "render/layout.h"
#include "editor/editor.h"
#include "engine/cas/steps.h"

#define ROWS_MAX (STEPS_MAX * 2) /* a line and a move each */
#define GAP 3

/* the soft keys, in Method order */
static const char *const method_keys[5] = { "optimal", "root", "factor", "square", "formula" };

static bool open;
static pcas_ast_t *lhs, *rhs;
static pcas_ast_t *deriv; /* d/dx node for derivative steps, else NULL */
static Symbol var;
static Method method;
static box_t *rows[ROWS_MAX];
static bool is_move[ROWS_MAX];
static int nrows, top;
static unsigned off; /* a bit per method that doesn't fit, greyed out */

static box_t *text(char c) {
    char s[2] = { c, '\0' };

    return layout_text(s, 0);
}

static void add_spaced(box_t *row, box_t *b) {
    layout_add(row, layout_space(LAYOUT_SPACE(0)));
    layout_add(row, b);
    layout_add(row, layout_space(LAYOUT_SPACE(0)));
}

/* a minus up front or a sum, so -5 and x+1 get brackets after a sign: /(-2) */
static bool needs_parens(pcas_ast_t *e) {
    if(e->type == NODE_NUMBER)
        return mp_rat_compare_zero(e->op.num) < 0;
    if(isoptype(e, OP_MULT))
        return needs_parens(ast_ChildGet(e, 0));
    return isoptype(e, OP_ADD);
}

/* right, right +- pm, or (right +- pm)/den */
static box_t *right_box(const step_eq_t *q) {
    box_t *row, *frac;

    if(q->pm == NULL)
        return layout_build(q->right, &draw_font);
    row = layout_box(BOX_ROW, 0);
    if(q->right != NULL) {
        layout_add(row, layout_build(q->right, &draw_font));
        add_spaced(row, text(CH_PM));
    } else {
        layout_add(row, text(CH_PM));
    }
    layout_add(row, layout_build(q->pm, &draw_font));
    if(q->den == NULL)
        return row;
    frac = layout_box(BOX_FRAC, 0);
    layout_add(frac, row);
    layout_add(frac, layout_build(q->den, &draw_font));
    return frac;
}

/* x+2 = 0  or  x+3 = 0, a = 1, b = 6  or  = 2x sin(x) */
static box_t *line_box(const step_t *l, bool first) {
    box_t *row = layout_box(BOX_ROW, 0);
    int j;

    if(l->eq[0].right == NULL && l->eq[0].pm == NULL) {
        if(!first) {
            layout_add(row, layout_text("=", 0));
            layout_add(row, layout_space(LAYOUT_SPACE(0)));
        }
        layout_add(row, layout_build(l->eq[0].left, &draw_font));
        layout_finish(row, &draw_font);
        return row;
    }
    for(j = 0; j < l->eqs; j++) {
        if(j > 0 && l->list) {
            layout_add(row, layout_text(",", 0));
            layout_add(row, layout_space(2 * LAYOUT_SPACE(0)));
        } else if(j > 0) {
            layout_add(row, layout_space(LAYOUT_SPACE(0)));
            add_spaced(row, layout_text("or", 0));
            layout_add(row, layout_space(LAYOUT_SPACE(0)));
        }
        layout_add(row, layout_build(l->eq[j].left, &draw_font));
        add_spaced(row, layout_text("=", 0));
        layout_add(row, right_box(&l->eq[j]));
    }
    layout_finish(row, &draw_font);
    return row;
}

/* the move between two lines: -5, /3, *2, a root sign */
static box_t *move_box(const step_t *l) {
    box_t *row = layout_box(BOX_ROW, 0), *by, *root;

    if(l->move == 'n') {
        /* product rule, chain rule */
        char names[64] = "";
        int k;

        for(k = 0; k < STEP_RULES_MAX && l->rules[k] != NULL; k++) {
            if(k > 0)
                strncat(names, ", ", sizeof(names) - strlen(names) - 1);
            strncat(names, l->rules[k], sizeof(names) - strlen(names) - 1);
        }
        layout_add(row, layout_text(names, 0));
    } else if(l->move == 'r') {
        root = layout_box(BOX_ROOT, 0);
        layout_add(root, layout_text("  ", 0)); /* a space box has no height, the sign came out flat */
        layout_add(row, root);
    } else {
        layout_add(row, text(l->move == '_' ? CH_MINUS : l->move == '/' ? CH_DIV : l->move == '*' ? CH_TIMES : l->move));
        by = layout_build(l->by, &draw_font);
        if(needs_parens(l->by)) {
            box_t *p = layout_box(BOX_PARENS, 0);

            layout_add(p, by);
            by = p;
        }
        layout_add(row, by);
    }
    layout_finish(row, &draw_font);
    return row;
}

static void free_rows(void) {
    int i;

    for(i = 0; i < nrows; i++)
        layout_free(rows[i]);
    nrows = top = 0;
}

/* the steps into rows, false if it doesn't do this one */
static bool build(void) {
    static steps_t s; /* 24 lines, ~1 KB on the CE */
    int i;

    if(deriv != NULL ? !steps_derivative(deriv, &s) : !steps_solve(lhs, rhs, var, method, &s))
        return false;
    free_rows();
    for(i = 0; i < s.n && nrows < ROWS_MAX; i++) {
        is_move[nrows] = false;
        rows[nrows++] = line_box(&s.line[i], i == 0);
        if(s.line[i].move != 0 && nrows < ROWS_MAX) {
            is_move[nrows] = true;
            rows[nrows++] = move_box(&s.line[i]);
        }
    }
    steps_free(&s);
    return true;
}

const char *work_open(char *src) {
    pcas_ast_t *l, *r;
    char shown, rel;
    const char *why = read_equation(src, &l, &r, &var, &shown, &rel);
    int m;

    if(why != NULL)
        return why;
    if(rel != 0) {
        ast_Cleanup(l);
        ast_Cleanup(r);
        return "no steps for inequalities yet";
    }
    lhs = l;
    rhs = r != NULL ? r : ast_MakeNumber(num_FromInt(0)); /* solve(x^2-4) is = 0 */
    method = METHOD_OPTIMAL;
    if(!build()) {
        work_close();
        return "no steps for this one yet";
    }
    off = 0;
    for(m = 0; m < METHOD_COUNT; m++)
        if(!steps_fit(lhs, rhs, var, (Method)m))
            off |= 1u << m;
    open = true;
    return NULL;
}

const char *work_open_deriv(pcas_ast_t *d) {
    deriv = d;
    if(!build()) {
        work_close();
        return "no steps for this one yet";
    }
    open = true;
    return NULL;
}

bool work_is_open(void) {
    return open;
}

void work_close(void) {
    free_rows();
    ast_Cleanup(lhs);
    ast_Cleanup(rhs);
    ast_Cleanup(deriv);
    lhs = rhs = deriv = NULL;
    open = false;
}

void work_key(keypress_t p) {
    if(p.key >= K_YEQU && p.key <= K_GRAPH && deriv == NULL) {
        Method m = (Method)(p.key - K_YEQU);

        /* one that doesn't fit leaves the steps as they are */
        if(m != method && !(off & (1u << m))) {
            Method was = method;

            method = m;
            if(!build())
                method = was;
        }
        return;
    }
    switch(p.key) {
    case K_UP:
        if(top > 0)
            top--;
        break;
    case K_DOWN:
        if(top < nrows - 1)
            top++;
        break;
    case K_CLEAR:
        work_close();
        break;
    default:
        break;
    }
}

void work_draw(void) {
    int x0 = areas.body.x, w = areas.body.w, y0 = areas.body.y + GAP, y = y0, bottom = areas.keys.y - GAP;
    int i, x;

    for(i = top; i < nrows; i++) {
        box_t *b = rows[i];

        if(b == NULL)
            continue;
        if(y + b->ascent + b->descent > bottom)
            break;
        /* centered, a line too wide for it starts at the left edge */
        x = b->width < w - 2 * GAP ? x0 + (w - b->width) / 2 : x0 + GAP;
        screen_color(is_move[i] ? COL_ACCENT : COL_FG);
        draw_box(b, x, y + b->ascent);
        y += b->ascent + b->descent + GAP;
    }

    /* the scrolbar when it doesn't all fit, by rows */
    if(top > 0 || i < nrows) {
        int bar_h = (bottom - y0) * (i - top) / nrows;

        screen_color(COL_DIM);
        screen_vline(x0 + w - 2, y0 + (bottom - y0) * top / nrows, bar_h > 2 ? bar_h : 2);
    }
    if(deriv == NULL)
        draw_softkeys(&areas.keys, method_keys, (int)method, off);
}
