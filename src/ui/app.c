#include "app.h"
#include "names.h"
#include "vars.h"
#include "mode.h"
#include "settings.h"
#include "geom.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "engine/ast.h"
#include "engine/dbg.h"
#include "engine/parser.h"
#include "engine/cas/cas.h"
#include "engine/cas/identities.h"
#include "editor/editor.h"
#include "render/draw.h"
#include "platform/screen.h"
#include "platform/store.h"
#include "platform/keys.h"
#include "platform/power.h"
#include "ui/catalog.h"
#include "ui/page.h"

/*
    screen, top to bottom:
        history     input left, result right, newest at the bottom
        ─────────
        input line  with the cursor
        soft keys   Algebra | Calculus | Functions | Templates | Tools
    a folder opens as a list above its soft key. keys: docs/design-choices/input.md
*/

#define MARGIN      4
#define GAP         4
#define BAR_H       15          /* soft-key labels */
#define HISTORY_MAX 16          /* enough for now, old ones just scroll off the top */

typedef struct {
    box_t *in, *out;    /* out == NULL means a message instead */
    const char *message;
    unsigned long ms;
    ed_row_t *row, *out_row;    /* both again as editor rows, for pasting back */
} entry_t;

static entry_t history[HISTORY_MAX];
static int history_len;

/*
    the history works like the nspire's. up from the line highlights the last answer, up
    again its input, up again the answer before... enter pastes it, [on]+2
    copies it. left/right goes inside it (browsing) to select just a part.
*/
static int hist = -1; /* entry highlighted, -1 = typing in the input line */
static int cleared; /* [on][clear] on an empty line scrolls everything below this up out of view */
static bool hist_out; /* its answer instead of its input */
static bool browsing; /* moving around inside it, with view */
static editor_t view;

static editor_t ed;
static editor_t *running = &ed; /* the editor whose line is being run, for push(). the form keys run from view */
static bool second, alpha;      /* [2nd] / [alpha] pressed, for the next key */
static bool second_lock, alpha_lock;    /* stay on after a key */
static bool ctl, ctl_lock;      /* [on] tapped, so the next key becomes [on]+key (Ctrl) */
static const char *status;      /* one-line message above the input, or NULL */

static int menu = -1;           /* open folder, -1 = none */
static int menu_sel, menu_top;

static unsigned short simp_flags = SIMP_ALL;
static unsigned char canon_flags = CANONICAL_ALL;

/* what enter gives. [2nd][enter] cycles through them, [alpha][enter] jumps to
   decimal and back. it's a mode (ui.md): changing it redoes the last answer and
   everything after comes out that way too */
typedef enum { FORM_HAND, FORM_EXPANDED, FORM_FACTORED, FORM_DECIMAL, FORM_COUNT } form_t;
static form_t form = FORM_HAND;
static const char *const form_names[FORM_COUNT] = { "", "expanded", "factored", "decimal" };

/* history */

/* the next free entry, the oldest one goes when it's full */
static entry_t *push_slot(void) {
    if(history_len == HISTORY_MAX) {
        layout_free(history[0].in);
        layout_free(history[0].out);
        ed_row_free(history[0].row);
        ed_row_free(history[0].out_row);
        memmove(&history[0], &history[1], (HISTORY_MAX - 1) * sizeof(entry_t));
        history_len--;
        if(cleared > 0)
            cleared--;
    }
    return &history[history_len++];
}

/* always called while the editor in running still has the line that was run */
void push(box_t *in, box_t *out, const char *message, unsigned long ms) {
    entry_t *h = push_slot();

    h->in = in;
    h->out = out;
    h->message = message;
    h->ms = ms;
    h->row = ed_copy_line(running);
    /* a stacked answer (asym) has no one-line editor form, so it can't be
       browsed into or saved. it just goes when the app does */
    h->out_row = out != NULL && out->kind != BOX_COL ? ed_from_box(out) : NULL;
}

/* an entry back from the store, just the two rows. the boxes get drawn from them
   the way browsing does it. takes the rows */
static void push_loaded(ed_row_t *row, ed_row_t *out_row) {
    entry_t *h = push_slot();

    h->in = ed_load(&view, row, true) ? ed_layout(&view, &draw_font, false) : NULL;
    h->out = ed_load(&view, out_row, true) ? ed_layout(&view, &draw_font, false) : NULL;
    h->message = h->out == NULL ? "(lost)" : NULL;
    h->ms = 0;
    h->row = row;
    h->out_row = out_row;
}

/*
    the history between runs (platform/store.h): a format byte, then for every
    entry that has an answer its input row and answer row. capped, the oldest go
    first. saved on the way out, so a crash loses the session, which is fine
*/
#define HISTORY_SLOT "FLXHIST"
#define HISTORY_SAVE_MAX (12 * 1024)
#define ENTRY_MAX 1024  /* one entry, bigger ones just don't get kept */

static void history_save(void) {
    unsigned char *buf = malloc(HISTORY_SAVE_MAX), *tmp = malloc(ENTRY_MAX);
    size_t size[HISTORY_MAX], total = 1, at = 1;
    int i, first = 0;

    if(buf == NULL || tmp == NULL) {
        free(buf);
        free(tmp);
        return;
    }
    /* sizes first so the cut comes off the old end, not the new */
    for(i = 0; i < history_len; i++) {
        size_t a = 0, b = 0;

        if(history[i].row != NULL && history[i].out_row != NULL) {
            a = ed_row_save(history[i].row, tmp, ENTRY_MAX);
            b = a > 0 ? ed_row_save(history[i].out_row, tmp, ENTRY_MAX - a) : 0;
        }
        size[i] = a > 0 && b > 0 ? a + b : 0;
    }
    for(i = history_len - 1; i >= 0; i--) {
        if(total + size[i] > HISTORY_SAVE_MAX)
            break;
        total += size[i];
        first = i;
    }
    buf[0] = ED_FORMAT;
    for(i = first; i < history_len; i++) {
        if(size[i] == 0)
            continue;
        at += ed_row_save(history[i].row, buf + at, HISTORY_SAVE_MAX - at);
        at += ed_row_save(history[i].out_row, buf + at, HISTORY_SAVE_MAX - at);
    }
    store_save(HISTORY_SLOT, buf, at);
    free(buf);
    free(tmp);
}

static void history_load(void) {
    unsigned char *buf = malloc(HISTORY_SAVE_MAX);
    size_t n, at = 1;

    if(buf == NULL)
        return;
    n = store_load(HISTORY_SLOT, buf, HISTORY_SAVE_MAX);
    if(n < 1 || n > HISTORY_SAVE_MAX || buf[0] != ED_FORMAT) {
        free(buf);
        return;
    }
    while(at < n) {
        size_t used = 0;
        ed_row_t *row = ed_row_load(buf + at, n - at, &used), *out;

        if(row == NULL)
            break;
        at += used;
        out = ed_row_load(buf + at, n - at, &used);
        if(out == NULL) {
            ed_row_free(row);
            break;
        }
        at += used;
        push_loaded(row, out);
    }
    free(buf);
}

static void clear_history(void) {
    while(history_len > 0) {
        history_len--;
        layout_free(history[history_len].in);
        layout_free(history[history_len].out);
        ed_row_free(history[history_len].row);
        ed_row_free(history[history_len].out_row);
    }
    hist = -1;
    cleared = 0;
    browsing = false;
}

/* the row that's highlighted */
static const ed_row_t *hist_row(void) {
    return hist_out ? history[hist].out_row : history[hist].row;
}

/* up off the top of the input line */
static void history_open(void) {
    if(history_len == 0)
        return;
    hist = history_len - 1;
    hist_out = history[hist].out_row != NULL;
    browsing = false;
}

/* -- undo -- */

/* the last key typed a plain character. a run of those is one undo step, else
   undoing "sin(x)+12" would take forever */
static bool typing_run;

/* call before a key changes the line */
static void will_edit(bool typing) {
    if(!(typing && typing_run))
        ed_checkpoint(&ed);
    typing_run = typing;
}

/* the line's been run, clear it (undo brings it back) */
static void line_done(void) {
    will_edit(false);
    ed_clear(&ed);
}

/* running the engine */

#define SIMP_BASIC (SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_RATIONAL)

/* how big a tree is, for "did that get simpler" */
static int nodes(const pcas_ast_t *e) {
    const pcas_ast_t *k;
    int n = 1;

    if(e->type == NODE_OPERATOR)
        for(k = e->op.operator.base; k != NULL; k = k->next)
            n += nodes(k);
    return n;
}

/* would anyone multiply this out by hand. a cube at most, and not a page of it.
   (x+1)^7 was minutes on the calculator before expand collected as it went,
   and it'd be silly to pay that just to find out it didn't get smaller */
static bool worth_expanding(const pcas_ast_t *e) {
    const pcas_ast_t *k;

    if(e->type != NODE_OPERATOR)
        return true;
    if(optype(e) == OP_POW) {
        const pcas_ast_t *base = e->op.operator.base, *exp = base != NULL ? base->next : NULL;

        if(base != NULL && base->type == NODE_OPERATOR && exp != NULL && exp->type == NODE_NUMBER
           && mp_rat_compare_value(exp->op.num, 3, 1) > 0)
            return false;
    }
    for(k = e->op.operator.base; k != NULL; k = k->next)
        if(!worth_expanding(k))
            return false;
    return true;
}

/* the form on top of a plain simplify, the same steps the commands take */
static void apply_form(pcas_ast_t *e) {
    switch(form) {
    case FORM_EXPANDED:
        expand(e, EXP_ALL);
        simplify(e, SIMP_BASIC | SIMP_EVAL | SIMP_LIKE_TERMS);
        break;
    case FORM_FACTORED:
        comdenom(e, true);
        factor(e, FAC_ALL);
        simplify(e, SIMP_BASIC | SIMP_EVAL);
        break;
    case FORM_DECIMAL:
        approx(e);
        simplify(e, SIMP_BASIC | SIMP_EVAL);
        break;
    default: {
        /* by hand. (x+1)^2 - (x-1)^2 is 4x and (x+2)(x-2) is x^2 - 4, but (x+1)^2
           stays as it is: multiply out, keep it only if it got smaller. the
           nspire does about the same thing */
        pcas_ast_t *x;

        if(nodes(e) > 60 || !worth_expanding(e))
            break;
        x = ast_Copy(e);
        if(x == NULL)
            break;
        expand(x, EXP_ALL);
        simplify(x, SIMP_BASIC | SIMP_EVAL | SIMP_LIKE_TERMS);
        if(!pcas_cancelled && nodes(x) < nodes(e))
            replace_node(e, x);
        else
            ast_Cleanup(x);
        break;
    }
    }
}

/* a word as a result, like true / false */
static box_t *word_box(const char *text) {
    box_t *b = layout_text(text, 0);

    if(b != NULL)
        layout_finish(b, &draw_font);
    return b;
}

/* put the plain number first in a root's sum (or the top of its fraction) so
   (-3 - 2sqrt(6))/5, (-3 + 2sqrt(6))/5 read as a pair. the layout keeps the
   order of same-degree terms, so just moving it in the tree is enough (i think) */
static void number_first(pcas_ast_t *e) {
    pcas_ast_t *k;
    unsigned i = 0;

    if(isoptype(e, OP_DIV))
        e = ast_ChildGet(e, 0);
    if(!isoptype(e, OP_ADD))
        return;
    for(k = ast_ChildGet(e, 0); k != NULL; k = k->next, i++) {
        if(k->type == NODE_NUMBER && i > 0) {
            ast_ChildInsert(e, ast_ChildRemoveIndex(e, i), 0);
            return;
        }
    }
}

static bool has_sym(const pcas_ast_t *e, Symbol s) {
    const pcas_ast_t *k;

    if(e->type == NODE_SYMBOL)
        return e->op.symbol == s;
    if(e->type != NODE_OPERATOR)
        return false;
    for(k = e->op.operator.base; k != NULL; k = k->next)
        if(has_sym(k, s))
            return true;
    return false;
}

/* one answer value, over one denominator, number first. the trig families stay
   as a sum, pi/6 + 2pi n reads better than (12pi n + pi)/6 */
box_t *value_box(pcas_ast_t *v) {
    if(!has_sym(v, SYM_N))
        comdenom(v, false);
    simplify_canonical_form(v, canon_flags);
    if(form == FORM_DECIMAL) {
        approx(v);
        simplify(v, SIMP_BASIC | SIMP_EVAL);
    }
    number_first(v);
    return layout_build(v, &draw_font);
}

/* x = -5, 6 */
box_t *solutions_box(char var, pcas_ast_t **roots, int n) {
    char name[2] = { var, '\0' };
    box_t *row = layout_box(BOX_ROW, 0);
    int i;

    layout_add(row, layout_text(name, 0));
    layout_add(row, layout_space(LAYOUT_SPACE(0)));
    layout_add(row, layout_text("=", 0));
    layout_add(row, layout_space(LAYOUT_SPACE(0)));

    for(i = 0; i < n; i++) {
        if(i > 0) {
            layout_add(row, layout_text(",", 0));
            layout_add(row, layout_space(2 * SPACE_BINARY));
        }
        layout_add(row, value_box(roots[i]));
    }

    layout_finish(row, &draw_font);
    return row;
}

/* "vertical    x = -2, 2", one line of an asym() answer. takes the values row */
static box_t *labelled(const char *label, box_t *values) {
    box_t *row = layout_box(BOX_ROW, 0);

    layout_add(row, layout_text(label, 0));
    layout_add(row, layout_space(3 * SPACE_BINARY));
    layout_add(row, values);
    return row;
}

/* the variable after the comma in solve(..., x) and asym(..., x). one letter, or theta */
static bool read_var(const char *s, Symbol *var, char *shown) {
    if(strcmp(s, "theta") == 0) {
        *var = SYM_THETA;
        *shown = CH_THETA;
        return true;
    }
    if(s[0] >= 'A' && s[0] <= 'Z' && s[1] == '\0') {
        *var = (Symbol)s[0];
        *shown = (char)(s[0] - 'A' + 'a');
        return true;
    }
    return false;
}

/*
    solve(equation, variable). the engine's parser has no =, so the line is split
    here: at the = and the last comma outside brackets. no variable: x.
    takes src apart (mutates src! careful), in is the input line's boxes for the history.
*/
static void run_solve(char *src, box_t *in) {
    pcas_ast_t *roots[SOLVE_MAX] = { NULL }, *e, *rhs = NULL;
    char *eq = NULL, *comma = NULL, *s, shown = 'x';
    Symbol var = SYM_X;
    pcas_error_t err;
    clock_t start;
    unsigned long ms;
    int depth = 0, n, i;

    for(s = src; *s != '\0'; s++) {
        if(*s == '(')
            depth++;
        else if(*s == ')')
            depth--;
        else if(depth == 0 && *s == '=' && eq == NULL)
            eq = s;
        else if(depth == 0 && *s == ',')
            comma = s;
    }

    if(comma != NULL) {
        *comma = '\0';
        if(!read_var(comma + 1, &var, &shown)) {
            push(in, NULL, "solve(equation, variable)", 0);
            return;
        }
    }

    if(eq != NULL)
        *eq = '\0';
    e = parse((const uint8_t *)src, strlen(src), str_table, &err);
    if(err == E_SUCCESS && eq != NULL)
        rhs = parse((const uint8_t *)eq + 1, strlen(eq + 1), str_table, &err);

    if(err != E_SUCCESS || e == NULL || (eq != NULL && rhs == NULL)) {
        if(e != NULL)
            ast_Cleanup(e);
        if(rhs != NULL)
            ast_Cleanup(rhs);
        push(in, NULL, err != E_SUCCESS ? error_text[err] : "solve(equation, variable)", 0);
        return;
    }

    /* a = b  ->  a - b = 0 */
    if(rhs != NULL)
        e = ast_MakeBinary(OP_ADD, e, ast_MakeBinary(OP_MULT, ast_MakeNumber(num_FromInt(-1)), rhs));

    if(setting_angle == ANGLE_DEGREES)
        e = degrees_in(e);
    start = clock();
    n = solve_ex(e, var, roots, true);
    if(n > 0 && setting_trig == TRIG_ONE_TURN) {
        pcas_ast_t *hi = setting_angle == ANGLE_DEGREES
                         ? ast_MakeNumber(num_FromInt(360))
                         : ast_MakeBinary(OP_MULT, ast_MakeNumber(num_FromInt(2)), ast_MakeSymbol(SYM_PI));

        n = solve_in_range(roots, n, hi);
        ast_Cleanup(hi);
    }
    ms = (unsigned long)((clock() - start) * 1000 / CLOCKS_PER_SEC);
    ast_Cleanup(e);

    if(pcas_cancelled)
        push(in, NULL, pcas_oom ? "out of memory" : "cancelled", ms);
    else if(n == SOLVE_UNKNOWN)
        push(in, NULL, "can't solve this one yet", ms);
    else if(n == SOLVE_ALL)
        push(in, word_box("true"), NULL, ms);
    else if(n == 0)
        push(in, word_box("false"), NULL, ms);
    else {
        if(setting_angle == ANGLE_DEGREES)
            for(i = 0; i < n; i++)
                degrees_out(roots[i]);
        push(in, solutions_box(shown, roots, n), NULL, ms);
    }

    for(i = 0; i < n; i++)
        ast_Cleanup(roots[i]);
}

/*
    asym(expression, variable). holes and asymptotes, a line for each kind it
    actually has. same comma rule as solve, and it writes into src too
*/
static void run_asym(char *src, box_t *in) {
    pcas_ast_t *e;
    asym_t a;
    char *comma = NULL, *s, shown = 'x';
    Symbol var = SYM_X;
    pcas_error_t err;
    clock_t start;
    unsigned long ms;
    int depth = 0;
    bool ok;
    box_t *col;

    for(s = src; *s != '\0'; s++) {
        if(*s == '(')
            depth++;
        else if(*s == ')')
            depth--;
        else if(depth == 0 && *s == ',')
            comma = s;
    }
    if(comma != NULL) {
        *comma = '\0';
        if(!read_var(comma + 1, &var, &shown)) {
            push(in, NULL, "asym(expression, variable)", 0);
            return;
        }
    }

    e = parse((const uint8_t *)src, strlen(src), str_table, &err);
    if(err != E_SUCCESS || e == NULL) {
        if(e != NULL)
            ast_Cleanup(e);
        push(in, NULL, error_text[err], 0);
        return;
    }
    if(setting_angle == ANGLE_DEGREES)
        e = degrees_in(e);

    start = clock();
    ok = asym(e, var, &a);
    ms = (unsigned long)((clock() - start) * 1000 / CLOCKS_PER_SEC);
    ast_Cleanup(e);

    if(pcas_cancelled) {
        push(in, NULL, pcas_oom ? "out of memory" : "cancelled", ms);
        asym_free(&a);
        return;
    }
    if(!ok) {
        push(in, NULL, "can't solve where the bottom is 0", ms);
        return;
    }
    if(a.nholes == 0 && a.nvert == 0 && a.horiz == NULL && a.slant == NULL) {
        push(in, word_box("none"), NULL, ms);
        return;
    }

    col = layout_box(BOX_COL, 0);
    if(a.nholes > 0)
        layout_add(col, labelled(a.nholes > 1 ? "holes" : "hole", solutions_box(shown, a.holes, a.nholes)));
    if(a.nvert > 0)
        layout_add(col, labelled("vertical", solutions_box(shown, a.vert, a.nvert)));
    if(a.horiz != NULL)
        layout_add(col, labelled("horizontal", solutions_box('y', &a.horiz, 1)));
    if(a.slant != NULL)
        layout_add(col, labelled("slant", solutions_box('y', &a.slant, 1)));
    layout_finish(col, &draw_font);
    push(in, col, NULL, ms);
    asym_free(&a);
}

/* x >= 2, one side of a domain() answer. rel is one character, the font has >= and <= */
static box_t *bound_box(char var, char rel, pcas_ast_t *v) {
    char name[2] = { var, '\0' }, op[2] = { rel, '\0' };
    box_t *row = layout_box(BOX_ROW, 0);

    layout_add(row, layout_text(name, 0));
    layout_add(row, layout_space(LAYOUT_SPACE(0)));
    layout_add(row, layout_text(op, 0));
    layout_add(row, layout_space(LAYOUT_SPACE(0)));
    layout_add(row, value_box(v));
    layout_finish(row, &draw_font);
    return row;
}

/*
    domain(expression, variable). a line per bound, then the single values that
    are out. comma like asym, writes into src too
*/
static void run_domain(char *src, box_t *in) {
    pcas_ast_t *e;
    domain_t d;
    char *comma = NULL, *s, shown = 'x';
    Symbol var = SYM_X;
    pcas_error_t err;
    clock_t start;
    unsigned long ms;
    int depth = 0;
    bool ok;
    box_t *col;

    for(s = src; *s != '\0'; s++) {
        if(*s == '(')
            depth++;
        else if(*s == ')')
            depth--;
        else if(depth == 0 && *s == ',')
            comma = s;
    }
    if(comma != NULL) {
        *comma = '\0';
        if(!read_var(comma + 1, &var, &shown)) {
            push(in, NULL, "domain(expression, variable)", 0);
            return;
        }
    }

    e = parse((const uint8_t *)src, strlen(src), str_table, &err);
    if(err != E_SUCCESS || e == NULL) {
        if(e != NULL)
            ast_Cleanup(e);
        push(in, NULL, error_text[err], 0);
        return;
    }
    if(setting_angle == ANGLE_DEGREES)
        e = degrees_in(e);

    start = clock();
    ok = domain(e, var, &d);
    ms = (unsigned long)((clock() - start) * 1000 / CLOCKS_PER_SEC);
    ast_Cleanup(e);

    if(pcas_cancelled) {
        push(in, NULL, pcas_oom ? "out of memory" : "cancelled", ms);
        domain_free(&d);
        return;
    }
    if(!ok) {
        push(in, NULL, "the bottom is 0 everywhere", ms);
        return;
    }
    if(d.empty) {
        char none[] = "no real x";

        none[8] = shown;
        push(in, word_box(none), NULL, ms);
        domain_free(&d);
        return;
    }
    if(d.lower == NULL && d.upper == NULL && d.nholes == 0) {
        /* "all reals (and maybe more)" would be a lie the other way round */
        if(d.partial)
            push(in, NULL, "can't work all of this one out yet", ms);
        else
            push(in, word_box("all reals"), NULL, ms);
        return;
    }

    col = layout_box(BOX_COL, 0);
    if(d.lower != NULL)
        layout_add(col, bound_box(shown, d.lower_open ? '>' : CH_GE, d.lower));
    if(d.upper != NULL)
        layout_add(col, bound_box(shown, d.upper_open ? '<' : CH_LE, d.upper));
    if(d.nholes > 0)
        layout_add(col, labelled("except", solutions_box(shown, d.holes, d.nholes)));
    if(d.partial)
        layout_add(col, layout_text("(and maybe more)", 0));
    layout_finish(col, &draw_font);
    push(in, col, NULL, ms);
    domain_free(&d);
}

/*
    system(eq, eq, ..., x, y). the pieces between the top level commas with an =
    are equations, the rest are the variables. no variables given, it takes the
    letters it finds, a to z. writes into src like the other two
*/
static void run_system(char *src, box_t *in) {
    pcas_ast_t *eqs[SYSTEM_MAX] = { NULL }, *out[SYSTEM_MAX] = { NULL }, *rhs;
    char *piece[2 * SYSTEM_MAX], *s, shown[SYSTEM_MAX];
    Symbol vars[SYSTEM_MAX];
    pcas_error_t err = E_SUCCESS;
    clock_t start;
    unsigned long ms;
    int depth = 0, pieces = 1, n = 0, nvars = 0, i, got;
    bool too_many = false;
    box_t *row;

    piece[0] = src;
    for(s = src; *s != '\0'; s++) {
        if(*s == '(')
            depth++;
        else if(*s == ')')
            depth--;
        else if(depth == 0 && *s == ',') {
            if(pieces == 2 * SYSTEM_MAX) {
                too_many = true;
                break;
            }
            *s = '\0';
            piece[pieces++] = s + 1;
        }
    }

    for(i = 0; i < pieces; i++) {
        char *eq = strchr(piece[i], '=');

        if(eq == NULL) {
            if(nvars == SYSTEM_MAX || !read_var(piece[i], &vars[nvars], &shown[nvars]))
                break;
            nvars++;
            continue;
        }
        if(n == SYSTEM_MAX) {
            too_many = true;
            break;
        }
        *eq = '\0';
        eqs[n] = parse((const uint8_t *)piece[i], strlen(piece[i]), str_table, &err);
        rhs = err == E_SUCCESS ? parse((const uint8_t *)eq + 1, strlen(eq + 1), str_table, &err) : NULL;
        if(err != E_SUCCESS || eqs[n] == NULL || rhs == NULL) {
            ast_Cleanup(rhs);
            break;
        }
        eqs[n] = ast_MakeBinary(OP_ADD, eqs[n], ast_MakeBinary(OP_MULT, ast_MakeNumber(num_FromInt(-1)), rhs));
        n++;
    }

    /* nothing named, so whatever letters turn up. alphabetical, not the order they're typed in */
    if(i == pieces && nvars == 0) {
        bool seen[26] = { false };
        char c;

        /* the pieces are still one buffer, just with zeros where the commas were */
        for(s = src; s < piece[pieces - 1] + strlen(piece[pieces - 1]); s++)
            if(*s >= 'A' && *s <= 'Z')
                seen[*s - 'A'] = true;
        for(c = 'A'; c <= 'Z' && nvars < SYSTEM_MAX; c++)
            if(seen[c - 'A']) {
                vars[nvars] = (Symbol)c;
                shown[nvars++] = (char)(c - 'A' + 'a');
            }
    }

    if(i < pieces || n == 0 || n != nvars) {
        for(i = 0; i < n; i++)
            ast_Cleanup(eqs[i]);
        push(in, NULL, err != E_SUCCESS ? error_text[err]
                       : too_many ? "4 equations is the most it does"
                       : "system(eq, eq, x, y), as many letters as equations", 0);
        return;
    }

    start = clock();
    got = solve_system(eqs, n, vars, out);
    ms = (unsigned long)((clock() - start) * 1000 / CLOCKS_PER_SEC);
    for(i = 0; i < n; i++)
        ast_Cleanup(eqs[i]);

    if(pcas_cancelled)
        push(in, NULL, pcas_oom ? "out of memory" : "cancelled", ms);
    else if(got == SOLVE_UNKNOWN)
        push(in, NULL, "can't solve this one yet (linear only)", ms);
    else if(got == SOLVE_ALL)
        push(in, NULL, "infinitely many, they say the same thing", ms);
    else if(got == 0)
        push(in, word_box("false"), NULL, ms);
    else {
        row = layout_box(BOX_ROW, 0);
        for(i = 0; i < n; i++) {
            char name[2] = { shown[i], '\0' };

            if(i > 0) {
                layout_add(row, layout_text(",", 0));
                layout_add(row, layout_space(2 * SPACE_BINARY));
            }
            layout_add(row, layout_text(name, 0));
            layout_add(row, layout_space(LAYOUT_SPACE(0)));
            layout_add(row, layout_text("=", 0));
            layout_add(row, layout_space(LAYOUT_SPACE(0)));
            layout_add(row, value_box(out[i]));
        }
        layout_finish(row, &draw_font);
        push(in, row, NULL, ms);
    }
    for(i = 0; i < n; i++)
        ast_Cleanup(out[i]);
}

static void busy_start(void);

/* parse what's in line, run the command or a simplify with the form on top, push
   the answer. true if something got pushed (the caller clears the line then) */
static bool run(editor_t *line) {
    const char *cmd;
    char *src;
    pcas_error_t err;
    pcas_ast_t *e;
    box_t *in;
    clock_t start;
    bool primes = false;

    if(ed_is_empty(line))
        return false;
    running = line;
    layout_decimal(form == FORM_DECIMAL);
    ed_fold_command(line);  /* solve( typed out counts the same as picked */
    cmd = ed_command(line);

    /* a line with the sto arrow only stores (or deletes), nothing gets run */
    {
        int arrow = ed_row_find_char(line->root, CH_STO);

        if(arrow >= 0) {
            bool deleted;
            const char *err = names_store_line(line->root, arrow, &deleted);

            if(err != NULL) {
                status = err;
                return false;
            }
            push(ed_layout(line, &draw_font, false), NULL, deleted ? "deleted" : "stored", 0);
            if(!names_save())
                status = "stored, but couldn't save it for next time";
            return true;
        }
    }

    /* solving for a variable that has a value makes no sense, and would quietly
       solve for 5. bold = not symbolic, consistently */
    if(cmd != NULL && strcmp(cmd, "solve") == 0) {
        const ed_row_t *arg = ed_command_row(line);
        int comma = arg != NULL ? ed_row_find_last(arg, ',') : -1;
        char var[NAME_LEN + 1];

        if(comma >= 0 && ed_row_word(arg, comma + 1, var, NAME_LEN) == 1 && names_is_stored_letter(var[0])) {
            static char msg[40];

            strcpy(msg, "x is stored, delete it to solve for x");
            msg[0] = msg[36] = var[0];
            status = msg;
            return false;
        }
    }

    src = cmd != NULL ? ed_command_arg(line) : ed_to_engine(line);
    if(src == NULL) {
        status = ed_error != NULL ? ed_error : "fill in the empty boxes first";
        return false;
    }

    /* no nesting commands yet, expand(...) has to be the whole line (todo, ugh) */
    if(strstr(src, "expand(") || strstr(src, "factor(") || strstr(src, "eval(") || strstr(src, "solve(")
       || strstr(src, "asym(") || strstr(src, "system(") || strstr(src, "slope(") || strstr(src, "dist(")
       || strstr(src, "mid(") || strstr(src, "par(") || strstr(src, "perp(") || strstr(src, "domain(")) {
        free(src);
        status = "commands have to be the whole line (for now)";
        return false;
    }

    if(cmd != NULL && strcmp(cmd, "solve") == 0) {
        run_solve(src, ed_layout(line, &draw_font, false));
        free(src);
        return true;
    }
    if(cmd != NULL && strcmp(cmd, "asym") == 0) {
        run_asym(src, ed_layout(line, &draw_font, false));
        free(src);
        return true;
    }
    if(cmd != NULL && strcmp(cmd, "domain") == 0) {
        run_domain(src, ed_layout(line, &draw_font, false));
        free(src);
        return true;
    }
    if(cmd != NULL && strcmp(cmd, "system") == 0) {
        run_system(src, ed_layout(line, &draw_font, false));
        free(src);
        return true;
    }
    if(strchr(src, '=') != NULL) {
        free(src);
        status = "= only works in solve( and system(";
        return false;
    }
    in = ed_layout(line, &draw_font, false);
    if(geom_run(cmd, src, in)) {
        free(src);
        return true;
    }

    e = parse((const uint8_t *)src, strlen(src), str_table, &err);
    free(src);

    if(err != E_SUCCESS || e == NULL) {
        push(in, NULL, error_text[err], 0);
        return true;
    }
    if(setting_angle == ANGLE_DEGREES)
        e = degrees_in(e);

    start = clock();

    /* same steps PineappleCAS's own UI does for each command, no need to be clever */
    if(cmd != NULL && strcmp(cmd, "expand") == 0) {
        simplify(e, SIMP_BASIC);
        expand(e, EXP_ALL);
        simplify(e, SIMP_BASIC | SIMP_EVAL | SIMP_LIKE_TERMS);
    } else if(cmd != NULL && strcmp(cmd, "factor") == 0) {
        pcas_ast_t *n;

        simplify(e, SIMP_BASIC);
        /* a whole number comes back as its primes, already in order. no simplify
           after, it'd multiply them straight back together, and no canonical form,
           its sort puts the 5 of 2^3*3^2*5 first and 2^2*3^2 becomes (2*3)^2.
           worked out on a copy first, -12 is still -1*12 here */
        n = ast_Copy(e);
        if(n != NULL)
            simplify(n, SIMP_BASIC | SIMP_EVAL);
        if(n != NULL && n->type == NODE_NUMBER) {
            replace_node(e, n);
            factor(e, FAC_ALL);
            primes = true;
        } else {
            if(n != NULL)
                ast_Cleanup(n);
            factor(e, FAC_ALL);
            simplify(e, SIMP_BASIC | SIMP_EVAL);
        }
    } else if(cmd != NULL && strcmp(cmd, "eval") == 0) {
        simplify(e, simp_flags);
        eval(e, EVAL_ALL);
        simplify(e, SIMP_BASIC | SIMP_EVAL | SIMP_LIKE_TERMS);
    } else {
        /* before simplify splits the fractions apart */
        cancel_fractions(e);
        simplify(e, simp_flags);
        apply_form(e);
    }
    if(!primes)
        simplify_canonical_form(e, canon_flags);
    if(setting_angle == ANGLE_DEGREES)
        degrees_out(e);
#ifdef COMPILE_PC
    /* FLX_DUMP=1 prints the tree that's about to be drawn, for poking at the pipeline */
    if(getenv("FLX_DUMP") != NULL)
        dbg_print_tree(e, 0);
#endif

    /* cancelled (or out of memory): the tree is half simplified, nobody wants to see that lol */
    push(in, pcas_cancelled ? NULL : layout_build(e, &draw_font),
         !pcas_cancelled ? NULL : pcas_oom ? "out of memory" : "cancelled",
         (unsigned long)((clock() - start) * 1000 / CLOCKS_PER_SEC));
    ast_Cleanup(e);
    return true;
}

static void evaluate(void) {
    if(run(&ed))
        line_done();
}

/* so a form switch swaps the last answer out instead of stacking a second copy */
static void pop_last(void) {
    entry_t *h = &history[--history_len];

    layout_free(h->in);
    layout_free(h->out);
    ed_row_free(h->row);
    ed_row_free(h->out_row);
    if(cleared > history_len)
        cleared = history_len;
}

/* switch form and redo the last answer in it. its input comes back out of the
   history and runs again from the scratch editor, so whatever's being typed is
   left alone */
static void form_set(form_t f) {
    form = f;
    if(history_len == 0 || hist >= 0)
        return;
    if(ed_load(&view, history[history_len - 1].row, true)) {
        pop_last();
        busy_start();
        run(&view);
    }
}

/* the mode page's side of things (app.h) */
const char *app_form_name(void) {
    return form == FORM_HAND ? "by hand" : form_names[form];
}

void app_next_form(void) {
    form_set((form_t)((form + 1) % FORM_COUNT));
}

bool app_rationalize(void) {
    return (canon_flags & CANONICAL_RATIONALIZE) != 0;
}

void app_toggle_rationalize(void) {
    canon_flags ^= CANONICAL_RATIONALIZE;
}

int app_form_index(void) {
    return (int)form;
}

void app_set_form_index(int i) {
    if(i >= 0 && i < FORM_COUNT)
        form = (form_t)i;
}

/* -- menus -- */

static const item_t *menu_item(int i) {
    return folders[menu].items[i];
}

/* Tools - which flag each setting maps to */
static bool setting(const item_t *it, unsigned short **simp, unsigned short *bit,
                    unsigned char **canon, unsigned char *cbit) {
    static const struct { const char *label; unsigned short bit; } ids[] = {
        { "basic identities",       SIMP_ID_GENERAL },
        { "trig identities",        SIMP_ID_TRIG },
        { "hyperbolic identities",  SIMP_ID_HYPERBOLIC },
        { "complex identities",     SIMP_ID_COMPLEX },
        { "trig constants",         SIMP_ID_TRIG_CONSTANTS },
        { "inverse trig constants", SIMP_ID_TRIG_INV_CONSTANTS },
    };
    unsigned i;

    *simp = NULL;
    *canon = NULL;
    for(i = 0; i < sizeof ids / sizeof ids[0]; i++) {
        if(strcmp(it->label, ids[i].label) == 0) {
            *simp = &simp_flags;
            *bit = ids[i].bit;
            return true;
        }
    }
    if(strcmp(it->label, "rationalize") == 0) {
        *canon = &canon_flags;
        *cbit = CANONICAL_RATIONALIZE;
        return true;
    }
    return false;
}

static bool setting_on(const item_t *it) {
    unsigned short *simp, bit = 0;
    unsigned char *canon, cbit = 0;

    if(!setting(it, &simp, &bit, &canon, &cbit))
        return false;
    return simp != NULL ? (*simp & bit) != 0 : (*canon & cbit) != 0;
}

/*
    what picking an item does, worked out from its engine syntax (catalog.c).
    returns false if the editor can't do it yet. with dry set, only checks.
*/
static bool pick_into(editor_t *into, const item_t *it, bool dry) {
    const char *s = it->engine;

    if(it->have == HAVE_NO)
        return false;

    if(it->kind == ITEM_SETTING) {
        unsigned short *simp, bit = 0;
        unsigned char *canon, cbit = 0;

        if(strcmp(it->label, "clear history") == 0) {
            if(!dry)
                clear_history();
            return true;
        }
        if(!setting(it, &simp, &bit, &canon, &cbit))
            return false;
        if(!dry) {
            if(simp != NULL)
                *simp ^= bit;
            else
                *canon ^= cbit;
        }
        return true;
    }

    if(s == NULL)
        return false;

    if(it->kind == ITEM_CHAR) {
        char c = strcmp(s, "pi") == 0 ? CH_PI : strcmp(s, "e") == 0 ? CH_EULER :
                 strcmp(s, "i") == 0 ? CH_IMAG : strcmp(s, "theta") == 0 ? CH_THETA :
                 strcmp(s, "!") == 0 ? '!' : 0;
        if(c == 0)
            return false;
        if(!dry)
            ed_insert_char(into, c);
        return true;
    }

    {
        static const struct { const char *engine; ed_kind_t kind; } shapes[] = {
            { "($)^($)", ED_POW }, { "sqrt($)", ED_SQRT }, { "abs($)", ED_ABS },
            { "logb($,$)", ED_LOGB }, { "($)/($)", ED_FRAC }, { "($)", ED_PARENS },
        };
        unsigned i;

        for(i = 0; i < sizeof shapes / sizeof shapes[0]; i++) {
            if(strcmp(s, shapes[i].engine) == 0) {
                if(!dry)
                    ed_insert(into, shapes[i].kind, NULL);
                return true;
            }
        }
    }

    if(strcmp(s, "($)root($)") == 0) {
        if(!dry) {
            ed_insert(into, ED_NROOT, NULL);
            if(strcmp(it->label, "cbrt") == 0) {
                ed_insert_char(into, '3');
                ed_down(into);
            }
        }
        return true;
    }
    if(strcmp(s, "e^($)") == 0 || strcmp(s, "10^($)") == 0) {
        if(!dry) {
            if(s[0] == 'e') {
                ed_insert_char(into, CH_EULER);
            } else {
                ed_insert_char(into, '1');
                ed_insert_char(into, '0');
            }
            ed_insert(into, ED_POW, NULL);
        }
        return true;
    }
    if(strcmp(s, "deriv($,X,X)") == 0) {
        if(!dry)
            ed_insert(into, ED_FUNC, "deriv");
        return true;
    }

    /* name($), the functions plus the commands the app runs itself */
    {
        const char *paren = strchr(s, '(');
        char name[8];
        size_t n = paren != NULL ? (size_t)(paren - s) : 0;

        /* gcd($,$) and co are one slot too, the comma just gets typed in it */
        if(n == 0 || n >= sizeof name || (strcmp(paren, "($)") != 0 && strcmp(paren, "($,$)") != 0))
            return false;
        if(it->kind == ITEM_COMMAND && strcmp(s, "expand($)") != 0 &&
           strcmp(s, "factor($)") != 0 && strcmp(s, "eval($)") != 0 && strcmp(s, "solve($)") != 0
           && strcmp(s, "asym($)") != 0 && strcmp(s, "system($)") != 0 && strcmp(s, "slope($)") != 0
           && strcmp(s, "dist($)") != 0 && strcmp(s, "mid($)") != 0 && strcmp(s, "par($)") != 0
           && strcmp(s, "perp($)") != 0 && strcmp(s, "domain($)") != 0)
            return false;
        if(!dry) {
            memcpy(name, s, n);
            name[n] = '\0';
            ed_insert(into, ED_FUNC, name);
        }
        return true;
    }
}

static bool pick(const item_t *it, bool dry) {
    return pick_into(&ed, it, dry);
}

/* the rows drawn as whatever picking them would put in the line. goes through
   pick_into on a scratch editor so the preview cant drift from what enter
   actually inserts, and one size under normal math so it still reads as a menu.
   built on open, freed on close */
static editor_t preview;
static box_t *menu_boxes[32];
static int menu_box_count;

static void menu_free_boxes(void) {
    int i;

    for(i = 0; i < menu_box_count; i++) {
        layout_free(menu_boxes[i]);
        menu_boxes[i] = NULL;
    }
    menu_box_count = 0;
}

static void menu_build_boxes(void) {
    const folder_t *f = &folders[menu];
    layout_font_t small = draw_font;
    unsigned i;

    if(small.base > 0)
        small.base--;
    if(preview.root == NULL && !ed_init(&preview))
        return;
    for(i = 0; i < f->count && i < sizeof menu_boxes / sizeof menu_boxes[0]; i++) {
        const item_t *it = f->items[i];

        menu_boxes[i] = NULL;
        if(it->kind == ITEM_SETTING || !pick(it, true))
            continue; /* settings and the greyed out ones stay text */
        /* the power's base is whatever's left of it, so alone it's just a
           floating slot. "x^n" says it better */
        if(strcmp(it->engine, "($)^($)") == 0)
            continue;
        ed_clear(&preview);
        if(pick_into(&preview, it, false))
            menu_boxes[i] = ed_layout(&preview, &small, false);
    }
    menu_box_count = (int)i;
}

static void menu_close(void) {
    menu_free_boxes();
    menu = -1;
}

static void menu_open(int folder) {
    menu_free_boxes();
    menu = folder;
    menu_sel = 0;
    menu_top = 0;
    menu_build_boxes();
}

static void menu_choose(int i) {
    const item_t *it = menu_item(i);

    if(it->kind != ITEM_SETTING && pick(it, true))
        will_edit(false);
    if(!pick(it, false)) {
        status = "not in Fluxion yet";
        return;
    }
    /* settings stay open so several can be toggled */
    if(it->kind != ITEM_SETTING)
        menu_close();
}

static void menu_key(keypress_t p) {
    int count = (int)folders[menu].count;

    switch(p.key) {
    case K_UP:    menu_sel = (menu_sel + count - 1) % count; break;
    case K_DOWN:  menu_sel = (menu_sel + 1) % count; break;
    case K_LEFT:  menu_open((menu + FOLDER_COUNT - 1) % FOLDER_COUNT); break;
    case K_RIGHT: menu_open((menu + 1) % FOLDER_COUNT); break;
    case K_ENTER: menu_choose(menu_sel); break;
    case K_CLEAR: menu_close(); break;

    case K_YEQU: case K_WINDOW: case K_ZOOM: case K_TRACE: case K_GRAPH: {
        int f = (int)(p.key - K_YEQU);
        if(f == menu)
            menu_close();
        else
            menu_open(f);
        break;
    }

    default:
        if(p.key >= K_1 && p.key <= K_9 && (int)(p.key - K_1) < count)
            menu_choose((int)(p.key - K_1));
        break;
    }
}

/* the Catalog page, [2nd][0] like the TI. everything you can type, under the
   same folders as the soft keys, then the plain keys a-z, then all of it a-z
   again. the keys for it on the right and what it does at the bottom. enter
   types it */

/* catalog indexes, plus 200+k for folder k's header, 255 for the keys one and
   254 for All. ~140 rows today, All has everything a second time */
#define CAT_FOLDER  200
#define CAT_ALL     254
#define CAT_KEYS    255

static unsigned char cat_order[160];
static int cat_count;

static bool cat_header(int i) {
    return cat_order[i] >= CAT_FOLDER;
}

static const item_t *cat_item(int i) {
    return catalog[cat_order[i]];
}

/* case doesn't matter, symbols end up first */
static int cat_cmp(const char *a, const char *b) {
    for(; *a != '\0' && *b != '\0'; a++, b++) {
        char x = (char)(*a >= 'A' && *a <= 'Z' ? *a - 'A' + 'a' : *a);
        char y = (char)(*b >= 'A' && *b <= 'Z' ? *b - 'A' + 'a' : *b);

        if(x != y)
            return x - y;
    }
    return *a - *b;
}

static const char *cat_label(int i) {
    if(cat_order[i] == CAT_KEYS)
        return "Keys";
    if(cat_order[i] == CAT_ALL)
        return "All";
    if(cat_header(i))
        return folders[cat_order[i] - CAT_FOLDER].name;
    return cat_item(i)->label;
}
static const char *cat_keys(int i)  { return cat_header(i) ? NULL : cat_item(i)->keys; }
static const char *cat_help(int i)  { return cat_header(i) ? NULL : cat_item(i)->help; }

static bool cat_pick(int i) {
    const item_t *it;

    if(cat_header(i))
        return false;   /* page.c doesnt send these, but still */
    it = cat_item(i);
    if(!pick(it, true)) {
        status = it->have == HAVE_NO ? "not in Fluxion yet" : "use the keys on the right for that";
        return true;
    }
    will_edit(false);
    pick(it, false);
    return true;
}

static page_t catalog_page = { "Catalog", 0, cat_label, cat_keys, cat_help, cat_pick, cat_header };

/* where an item sits in catalog[], folders only have the pointers */
static int cat_index(const item_t *it) {
    unsigned i;

    for(i = 0; i < catalog_count; i++)
        if(catalog[i] == it)
            return (int)i;
    return -1;
}

static bool in_a_folder(const item_t *it) {
    unsigned f, j;

    for(f = 0; f < FOLDER_COUNT; f++)
        for(j = 0; j < folders[f].count; j++)
            if(folders[f].items[j] == it)
                return true;
    return false;
}

/* a header then its items a-z, insertion sort is plenty for ~60. all of them
   for All, just the ones no folder has for Keys */
static void cat_sorted(unsigned char header, bool everything) {
    unsigned i;
    int from;

    if(cat_count < (int)sizeof cat_order)
        cat_order[cat_count++] = header;
    from = cat_count;
    for(i = 0; i < catalog_count && cat_count < (int)sizeof cat_order; i++) {
        int k = cat_count;

        /* settings go in a settings page */
        if(catalog[i]->kind == ITEM_SETTING || (!everything && in_a_folder(catalog[i])))
            continue;
        cat_count++;
        for(; k > from && cat_cmp(catalog[cat_order[k - 1]]->label, catalog[i]->label) > 0; k--)
            cat_order[k] = cat_order[k - 1];
        cat_order[k] = (unsigned char)i;
    }
}

static void catalog_open(void) {
    /* built once. the folders keep their menu order, Keys and All go a-z */
    /* order? i hardly know 'er bwhahaHAHAah */
    if(cat_count == 0) {
        unsigned f, j;

        for(f = 0; f < FOLDER_COUNT; f++) {
            int head = cat_count;

            if(cat_count < (int)sizeof cat_order)
                cat_order[cat_count++] = (unsigned char)(CAT_FOLDER + f);
            for(j = 0; j < folders[f].count && cat_count < (int)sizeof cat_order; j++) {
                int at = cat_index(folders[f].items[j]);

                /* settings show no on/off here, they have tools and mode */
                if(at >= 0 && catalog[at]->kind != ITEM_SETTING)
                    cat_order[cat_count++] = (unsigned char)at;
            }
            if(cat_count == head + 1)
                cat_count = head;   /* nothing left in it (tools), no lonely header */
        }

        cat_sorted(CAT_KEYS, false);
        cat_sorted(CAT_ALL, true);
        catalog_page.count = cat_count;
    }
    page_open(&catalog_page);
}

/* [math] pops up the plain characters with no key of their own, in a little grid that
   pops up at the cursor. 1-9 and 0 pick them straight away */

#define POPUP_COLS 5

static const char popup_chars[] = {
    '=', CH_NE, '<', '>', CH_LE,
    CH_GE, '!', CH_INF, CH_DEG, CH_THETA
};
#define POPUP_COUNT ((int)sizeof popup_chars)

static int popup = -1; /* selected one, -1 = closed */

/* where the cursor was last drawn, so the popup knows where to go */
static int cur_x, cur_top;

static void popup_pick(int i) {
    will_edit(false);
    ed_insert_char(&ed, popup_chars[i]);
    popup = -1;
}

static void popup_key(keypress_t p) {
    switch(p.key) {
    case K_LEFT:  popup = (popup + POPUP_COUNT - 1) % POPUP_COUNT; break;
    case K_RIGHT: popup = (popup + 1) % POPUP_COUNT; break;
    case K_UP: case K_DOWN:
        popup = (popup + POPUP_COLS) % POPUP_COUNT;
        break;
    case K_ENTER: popup_pick(popup); break;
    case K_CLEAR: case K_MATH: popup = -1; break;
    default:
        if(p.key >= K_1 && p.key <= K_9 && (int)(p.key - K_1) < POPUP_COUNT)
            popup_pick((int)(p.key - K_1));
        else if(p.key == K_0 && POPUP_COUNT >= 10)
            popup_pick(9);
        break;
    }
}

/* typing (key -> editor) */

/* x^2, x^-1. make an exponent with this in it, then step back out */
static void power_of(const char *what) {
    ed_insert(&ed, ED_POW, NULL);
    for(; *what != '\0'; what++)
        ed_insert_char(&ed, *what);
    ed_right(&ed);
}

/* [alpha] + key gives the letter printed above the key, like the TI */
static char alpha_letter(key_t k) {
    switch(k) {
    case K_MATH: return 'a';   case K_APPS: return 'b';   case K_PRGM: return 'c';
    case K_RECIP: return 'd';  case K_SIN: return 'e';    case K_COS: return 'f';
    case K_TAN: return 'g';    case K_POW: return 'h';    case K_SQUARE: return 'i';
    case K_COMMA: return 'j';  case K_LPAREN: return 'k'; case K_RPAREN: return 'l';
    case K_DIV: return 'm';    case K_LOG: return 'n';    case K_7: return 'o';
    case K_8: return 'p';      case K_9: return 'q';      case K_MUL: return 'r';
    case K_LN: return 's';     case K_4: return 't';      case K_5: return 'u';
    case K_6: return 'v';      case K_SUB: return 'w';    case K_STO: return 'x';
    case K_1: return 'y';      case K_2: return 'z';      case K_3: return CH_THETA;
    default: return 0;
    }
}

static bool key_plain(keypress_t p);

/* returns false to quit */
static bool key_2nd(keypress_t p) {
    switch(p.key) {
    case K_MODE:   return false;   /* quit */
    case K_LEFT:   typing_run = false; ed_home(&ed); return true;
    case K_RIGHT:  typing_run = false; ed_end(&ed); return true;
    case K_0:      typing_run = false; catalog_open(); return true;
    case K_DEL:
        return key_plain(p);
    case K_ENTER:  form_set((form_t)((form + 1) % FORM_COUNT)); return true;
    case K_CLEAR:  /* same as [on]+[clear], clear the whole line */
        will_edit(false);
        ed_clear(&ed);
        return true;
    default:
        will_edit(false);
        break;
    }

    switch(p.key) {
    case K_POW:    ed_insert_char(&ed, CH_PI); break;
    case K_DIV:    ed_insert_char(&ed, CH_EULER); break;
    case K_DOT:    ed_insert_char(&ed, CH_IMAG); break;
    case K_COMMA:  ed_insert_char(&ed, ED_SCI); break;
    case K_SQUARE: ed_insert(&ed, ED_SQRT, NULL); break;
    case K_SIN:    ed_insert(&ed, ED_FUNC, "asin"); break;
    case K_COS:    ed_insert(&ed, ED_FUNC, "acos"); break;
    case K_TAN:    ed_insert(&ed, ED_FUNC, "atan"); break;
    case K_MATH:   ed_insert_char(&ed, '='); break; /* TI's [2nd][math] is TEST, = is first */
    case K_LN:     ed_insert_char(&ed, CH_EULER); ed_insert(&ed, ED_POW, NULL); break;
    case K_LOG:    ed_insert_char(&ed, '1'); ed_insert_char(&ed, '0'); ed_insert(&ed, ED_POW, NULL); break;
    default:       break;
    }
    return true;
}

static bool key_plain(keypress_t p) {
    if(p.key >= K_0 && p.key <= K_9) {
        will_edit(true);
        ed_insert_char(&ed, (char)('0' + (p.key - K_0)));
        return true;
    }
    if(p.key >= K_LETTER_A && p.key <= K_LETTER_Z) {
        will_edit(true);
        ed_insert_char(&ed, (char)('a' + (p.key - K_LETTER_A)));
        return true;
    }
    if(p.key >= K_CAPITAL_A && p.key <= K_CAPITAL_Z) {
        will_edit(true);
        ed_insert_char(&ed, (char)('A' + (p.key - K_CAPITAL_A)));
        return true;
    }

    switch(p.key) {
    case K_LEFT: case K_RIGHT: case K_UP: case K_DOWN: case K_ENTER:
    case K_YEQU: case K_WINDOW: case K_ZOOM: case K_TRACE: case K_GRAPH:
    case K_QUIT: case K_MATH: case K_VARS: case K_MODE:
        typing_run = false;     /* not an edit (enter saves its own step) */
        break;
    case K_DOT: case K_NEG: case K_ADD: case K_SUB: case K_MUL: case K_DIV:
    case K_COMMA: case K_XT:
        will_edit(true);
        break;
    default:
        will_edit(false);
        break;
    }

    switch(p.key) {
    case K_QUIT:   return false;
    case K_DOT:    ed_insert_char(&ed, '.'); break;
    case K_NEG:    ed_insert_char(&ed, ED_NEG); break;
    case K_ADD:    ed_insert_char(&ed, ED_PLUS); break;
    case K_SUB:    ed_insert_char(&ed, ED_SUB); break;
    case K_MUL:    ed_insert_char(&ed, ED_TIMES); break;
    case K_DIV:    /* with something selected it goes on top of a fraction (nspire) */
        if(ed_has_selection(&ed))
            ed_insert(&ed, ED_FRAC, NULL);
        else
            ed_insert_char(&ed, ED_DIVIDE);
        break;
    case K_POW:    ed_insert(&ed, ED_POW, NULL); break;
    case K_SQUARE: power_of("2"); break;
    case K_RECIP: {
        char minus_one[3] = { ED_NEG, '1', '\0' };
        power_of(minus_one);
        break;
    }
    case K_LPAREN: ed_insert_char(&ed, '('); break;
    case K_RPAREN: ed_insert_char(&ed, ')'); break;
    case K_COMMA:  ed_insert_char(&ed, ','); break;
    case K_XT:     ed_insert_char(&ed, 'x'); break;
    case K_STO:    ed_insert_char(&ed, CH_STO); break;  /* value -> name, see names.c */
    case K_SIN:    ed_insert(&ed, ED_FUNC, "sin"); break;
    case K_COS:    ed_insert(&ed, ED_FUNC, "cos"); break;
    case K_TAN:    ed_insert(&ed, ED_FUNC, "tan"); break;
    case K_LN:     ed_insert(&ed, ED_FUNC, "ln"); break;
    case K_LOG:    ed_insert(&ed, ED_FUNC, "log"); break;

    /* with something selected, left/right just drop it on that side (like any
       text box) */
    case K_LEFT:
        if(ed_has_selection(&ed))
            ed_deselect(&ed, true);
        else
            ed_left(&ed);
        break;
    case K_RIGHT:
        if(ed_has_selection(&ed))
            ed_deselect(&ed, false);
        else
            ed_right(&ed);
        break;
    case K_UP:
        if(!ed_up(&ed))
            history_open();
        break;
    case K_DOWN:   ed_down(&ed); break;

    case K_CLEAR:  ed_backspace(&ed); break;    /* [on]+[clear] is in key_ctl */
    case K_DEL:    ed_delete(&ed); break;
    case K_ENTER:  evaluate(); break;

    case K_YEQU: case K_WINDOW: case K_ZOOM: case K_TRACE: case K_GRAPH:
        menu_open((int)(p.key - K_YEQU));
        break;
    case K_MATH:   popup = 0; break;
    case K_VARS:   vars_open(); break;  /* the stored names, like the TI's vars */
    case K_MODE:   mode_open(); break;

    default:
        break;
    }
    return true;
}

/* two taps of [2nd] or [alpha] this close together lock it */
#define DOUBLE_TAP (CLOCKS_PER_SEC * 2 / 5)

/* [2nd] / [alpha] / [on]: on for the next key, a double tap locks, one more tap
   unlocks. they're separate so any of them can be on together ([2nd][alpha] + a
   letter is a capital). a quick second tap locks it whatever the first one did
   (it may have just turned it off), a tap while locked unlocks */
static void tap(bool *on, bool *lock, bool again) {
    if(again)
        *on = *lock = true;
    else if(*lock)
        *on = *lock = false;
    else
        *on = !*on;
}

/* the last modifier tapped, K_NONE after any other key. a double tap is two
   taps with nothing in between (alpha a alpha b isn't one) */
static key_t last;
static clock_t last_time;

static void modifier(key_t k) {
    /* modifier? i hardly know 'er bahahwahaa */
    bool again = k == last && clock() - last_time < DOUBLE_TAP;

    last = k;
    last_time = clock();

    if(k == K_ON)
        tap(&ctl, &ctl_lock, again);
    else if(k == K_2ND)
        tap(&second, &second_lock, again);
    else
        tap(&alpha, &alpha_lock, again);

    /* locking ate the double tap, so a third quick tap starts a fresh one */
    if(again)
        last = K_NONE;
}

static ed_row_t *clipboard;

/* [on] + key is Ctrl on a computer. 1 2 3 are cut copy paste since they're next
   to each other, like X C V. false if it isn't one of these */
static bool key_ctl(keypress_t p) {
    switch(p.key) {
    case K_1:
    case K_2: {
        ed_row_t *copy = ed_copy(&ed);

        if(copy == NULL) {
            status = ed_has_selection(&ed) ? "out of memory" : "nothing selected ([alpha] + arrows)";
            return true;
        }
        ed_row_free(clipboard);
        clipboard = copy;
        if(p.key == K_1) {
            will_edit(false);
            ed_erase_selection(&ed);
        }
        return true;
    }
    case K_3:
        if(clipboard == NULL) {
            status = "nothing copied yet";
            return true;
        }
        will_edit(false);
        ed_paste(&ed, clipboard);
        return true;
    case K_STO:
        typing_run = false;
        if(!ed_undo(&ed))
            status = "nothing to undo";
        return true;
    case K_CLEAR: /* whole line, Ctrl+Backspace */
        /* on an empty line it clears the screen instead. nothing gets deleted,
           the history just scrolls up out of view and up brings it back */
        if(ed_is_empty(&ed)) {
            cleared = history_len;
            return true;
        }
        will_edit(false);
        ed_clear(&ed);
        return true;
    /* the reciprocals, [on] like the catalog says */
    case K_SIN: will_edit(false); ed_insert(&ed, ED_FUNC, "csc"); return true;
    case K_COS: will_edit(false); ed_insert(&ed, ED_FUNC, "sec"); return true;
    case K_TAN: will_edit(false); ed_insert(&ed, ED_FUNC, "cot"); return true;
    default:
        return false;
    }
}

/* one step up / down through answer, input, answer before... */
static void hist_up(void) {
    if(hist_out) {
        hist_out = false;
    } else if(hist > 0) {
        hist--;
        hist_out = history[hist].out_row != NULL;
    }
}

static void hist_down(void) {
    if(!hist_out && history[hist].out_row != NULL) {
        hist_out = true;
    } else if(hist < history_len - 1) {
        hist++;
        hist_out = false;
    } else {
        hist = -1;  /* past the newest, back to typing */
    }
}

/* what enter and copy take, just the selected part when browsing, else teh whole thing */
static ed_row_t *hist_take(void) {
    if(browsing)
        return ed_has_selection(&view) ? ed_copy(&view) : ed_copy_line(&view);
    return ed_row_copy(hist_row());
}

/* keys while the history's highlighted. true if the key was used up, else it
   goes back to the input line and gets handled there like normal */
static bool history_key(keypress_t p, bool alp) {
    if(p.on && p.key == K_2) {
        ed_row_t *copy = hist_take();

        if(copy != NULL) {
            ed_row_free(clipboard);
            clipboard = copy;
        }
        return true;
    }

    if(browsing) {
        switch(p.key) {
        case K_LEFT:
            if(alp)
                ed_select_left(&view);
            else if(ed_has_selection(&view))
                ed_deselect(&view, true);
            else
                ed_left(&view);
            return true;
        case K_RIGHT:
            if(alp)
                ed_select_right(&view);
            else if(ed_has_selection(&view))
                ed_deselect(&view, false);
            else
                ed_right(&view);
            return true;
        case K_UP:
            if(!ed_up(&view)) {
                browsing = false;
                hist_up();
            }
            return true;
        case K_DOWN:
            if(!ed_down(&view)) {
                browsing = false;
                hist_down();
            }
            return true;
        case K_CLEAR:
            browsing = false;   /* back to the whole thing highlighted */
            return true;
        default:
            break;
        }
    } else {
        switch(p.key) {
        case K_UP:    hist_up(); return true;
        case K_DOWN:  hist_down(); return true;
        case K_LEFT:
        case K_RIGHT:
            /* inside it, the cursor coming in from that side */
            if(hist_row() != NULL && ed_load(&view, hist_row(), p.key == K_LEFT))
                browsing = true;
            return true;
        case K_CLEAR:
            hist = -1;
            return true;
        default:
            break;
        }
    }

    if(p.key == K_ENTER) {
        ed_row_t *row = hist_take();

        /* into the line at the cursor, like the nspire */
        if(row != NULL) {
            will_edit(false);
            ed_paste(&ed, row);
            ed_row_free(row);
        }
        hist = -1;
        browsing = false;
        return true;
    }

    hist = -1;
    browsing = false;
    return false;
}

static bool handle(keypress_t p) {
    bool sec, alp;
    char c;

    status = NULL;

    if(p.key == K_QUIT)
        return false;

    if(menu >= 0) {
        menu_key(p);
        return true;
    }
    if(popup >= 0) {
        popup_key(p);
        return true;
    }


    /* [2nd][alpha][on] is lights out. both are already on by the time the on tap
       comes, so that's the whole check */
    if(p.key == K_ON && second && alpha) {
        second = alpha = second_lock = alpha_lock = false;
        keys_sleep();
        return true;
    }

    if(p.key == K_2ND || p.key == K_ALPHA || p.key == K_ON) {
        modifier(p.key);
        return true;
    }
    last = K_NONE;

    /* every modifier that's on (tapped or held down) goes with this key, then
       the tapped ones turn off unless locked */
    p.on = p.on || ctl;
    sec = second || p.second;
    alp = alpha || p.alpha;
    ctl = ctl_lock;
    second = second_lock;
    alpha = alpha_lock;

    c = alp ? alpha_letter(p.key) : 0;
    if(p.key >= K_LETTER_A && p.key <= K_LETTER_Z)
        c = (char)('a' + (p.key - K_LETTER_A));

    if(page_is_open()) {
        if(sec && p.key == K_MODE)  /* [2nd][mode] is quit, out of the page here */
            page_close();
        else
            page_key(p, c);
        return true;
    }

    /* [2nd] up/down is the brightness, like the TI's own. before the history so
       it works with an answer highlighted too. the 2nd's already used up above */
    if(sec && (p.key == K_UP || p.key == K_DOWN)) {
        setting_brightness += p.key == K_UP ? -1 : 1;
        if(setting_brightness < 0) setting_brightness = 0;
        if(setting_brightness > 7) setting_brightness = 7;
        screen_brightness(setting_brightness);
        settings_save();
        return true;
    }

    if(hist >= 0 && history_key(p, alp))
        return true;

    if(p.on && key_ctl(p))
        return true;

    /* [2nd][alpha] + letter, a capital */
    if(sec && alp && c >= 'a' && c <= 'z') {
        will_edit(true);
        ed_insert_char(&ed, (char)(c - 'a' + 'A'));
        return true;
    }

    if(sec)
        return key_2nd(p);

    if(alp) {
        switch(p.key) {
        case K_LEFT:  typing_run = false; ed_select_left(&ed); return true;
        case K_RIGHT: typing_run = false; ed_select_right(&ed); return true;
        case K_ENTER: form_set(form == FORM_DECIMAL ? FORM_HAND : FORM_DECIMAL); return true;
        case K_YEQU: case K_XT:  /* [alpha][y=] is n/d like the TI, and [alpha][x,t,θ,n] since it's right there */
            will_edit(false);
            ed_insert(&ed, ED_FRAC, NULL);
            return true;
        default:
            break;
        }
        if(c != 0) {
            will_edit(true);
            ed_insert_char(&ed, c);
            return true;
        }
    }

    return key_plain(p);
}

/* drawing */

/* screen x of the cursor inside b drawn at x, or -1 */
static int cursor_x(const box_t *b, int x) {
    const box_t *k;

    x += b->x;
    if(b->kind == BOX_CURSOR)
        return x;
    for(k = b->child; k != NULL; k = k->next) {
        int found = cursor_x(k, x);
        if(found >= 0)
            return found;
    }
    return -1;
}

static void draw_bar(void) {
    const font_t *font = draw_font_for(0);     /* smallest, five names have to fit somehow */
    int i, w = SCREEN_W / FOLDER_COUNT, top = SCREEN_H - BAR_H;

    for(i = 0; i < FOLDER_COUNT; i++) {
        const char *name = folders[i].name;
        int x = i * w + (w - draw_text_width(name, font)) / 2;

        if(i == menu) {
            screen_color(COL_ACCENT);
            screen_fill(i * w + 1, top, w - 2, BAR_H);
            screen_color(COL_BG);
        } else {
            screen_color(COL_DIM);
        }
        draw_text(x, top + (BAR_H - font->ascent - font->descent) / 2, name, font);
    }
}

static void draw_menu(void) {
    const font_t *font = draw_label_font();
    const folder_t *f = &folders[menu];
    int row_h = font->ascent + font->descent + 2;
    int bottom = SCREEN_H - BAR_H - 1;
    int rows = (int)f->count, max_rows = (bottom - 4) / row_h;
    int width = 0, x, top, i;

    /* rows are as wide and as tall as the biggest preview (a fraction makes the
       whole folder taller, fine) */
    for(i = 0; i < rows; i++) {
        const box_t *b = i < menu_box_count ? menu_boxes[i] : NULL;
        int w = b != NULL ? b->width : draw_text_width(f->items[i]->label, font);
        int h = b != NULL ? b->ascent + b->descent + 2 : 0;

        if(f->items[i]->kind == ITEM_SETTING)
            w += draw_text_width("[x] ", font); /* its drawn with the box in front so measure that too */
        if(w > width)
            width = w;
        if(h > row_h)
            row_h = h;
    }
    width += 34;    /* for the "1:" plus room for a * or [x] */
    max_rows = (bottom - 4) / row_h;

    if(rows > max_rows)
        rows = max_rows;
    if(menu_sel < menu_top)
        menu_top = menu_sel;
    if(menu_sel >= menu_top + rows)
        menu_top = menu_sel - rows + 1;

    x = menu * (SCREEN_W / FOLDER_COUNT);
    if(x + width > SCREEN_W - 1)
        x = SCREEN_W - 1 - width;
    top = bottom - rows * row_h - 2;

    screen_color(COL_BG);
    screen_fill(x, top, width, bottom - top + 1);
    screen_color(COL_DIM);
    screen_hline(x, top, width);
    screen_vline(x, top, bottom - top + 1);
    screen_vline(x + width - 1, top, bottom - top + 1);

    for(i = 0; i < rows; i++) {
        int n = menu_top + i, y = top + 2 + i * row_h;
        const item_t *it = f->items[n];
        bool can = pick(it, true);
        char label[48];

        if(it->kind == ITEM_SETTING && strcmp(it->label, "clear history") != 0)
            snprintf(label, sizeof label, "%s %s", setting_on(it) ? "[x]" : "[ ]", it->label);
        else
            snprintf(label, sizeof label, "%s%s", it->label, can ? "" : " *");

        if(n == menu_sel) {
            screen_color(COL_ACCENT);
            screen_fill(x + 1, y - 1, width - 2, row_h);
            screen_color(COL_BG);
        } else {
            screen_color(can ? COL_FG : COL_DIM);
        }

        if(n < 9) {
            char num[3] = { (char)('1' + n), ':', '\0' };
            draw_text(x + 3, y + (row_h - 2 - font->ascent - font->descent) / 2, num, font);
        }
        if(n < menu_box_count && menu_boxes[n] != NULL) {
            const box_t *b = menu_boxes[n];

            draw_box(b, x + 18, y + (row_h - 2 - b->ascent - b->descent) / 2 + b->ascent);
        } else {
            draw_text(x + 18, y + (row_h - 2 - font->ascent - font->descent) / 2, label, font);
        }
    }
}

static int top_h(void);

/* the [math] grid, just above the cursor, each cell numbered in the corner */
static void draw_popup(void) {
    const font_t *font = draw_font_for(draw_font.base), *tiny = draw_font_for(0);
    int cell_w = 22, cell_h = font->ascent + font->descent + 6;
    int rows = (POPUP_COUNT + POPUP_COLS - 1) / POPUP_COLS;
    int w = POPUP_COLS * cell_w + 1, h = rows * cell_h + 1;
    int x = cur_x - w / 2, y = cur_top - h - 2, i;

    if(x < 1)
        x = 1;
    if(x + w > SCREEN_W - 1)
        x = SCREEN_W - 1 - w;
    if(y < top_h())
        y = top_h();

    screen_color(COL_BG);
    screen_fill(x, y, w, h);
    screen_color(COL_DIM);
    screen_hline(x, y, w);
    screen_hline(x, y + h - 1, w);
    screen_vline(x, y, h);
    screen_vline(x + w - 1, y, h);

    for(i = 0; i < POPUP_COUNT; i++) {
        int cx = x + (i % POPUP_COLS) * cell_w, cy = y + (i / POPUP_COLS) * cell_h;
        char s[2] = { popup_chars[i], '\0' }, num[2] = { (char)(i < 9 ? '1' + i : '0'), '\0' };

        if(i == popup) {
            screen_color(COL_ACCENT);
            screen_fill(cx + 1, cy + 1, cell_w - 1, cell_h - 1);
        }
        screen_color(i == popup ? COL_BG : COL_DIM);
        draw_text(cx + 2, cy + 1, num, tiny);
        screen_color(i == popup ? COL_BG : COL_FG);
        draw_text(cx + (cell_w - draw_text_width(s, font)) / 2 + 2, cy + 4, s, font);
    }
}

/* the status bar */

/* along the top: a padlock each for 2nd / alpha / ctl, the working dots,
   the battery */

#define BATTERY_W   13
#define BATTERY_READ (CLOCKS_PER_SEC * 10)     /* the level is read this often (reading every frame was slow) */
#define LOCK_SLOT   13 /* per padlock, fixed so one turning on doesn't shove the others along */

static int battery = -1;
static clock_t battery_next;

static int top_h(void) {
    const font_t *small = draw_label_font();

    return small->ascent + small->descent + 3;
}

/* 3x5 letters punched out of the padlocks, top bit on the left: 2, A, C */
static const unsigned char lock_letters[3][5] = {
    { 6, 1, 2, 4, 7 },
    { 2, 5, 7, 5, 5 },
    { 3, 4, 4, 4, 3 }
};

/* 9 x 11, top left at x, y. a lock is the padlock with the letter punched out
   of it, a plain sticky press is just the letter on its own, bigger, so you can
   tell the two apart at a glance (the open-shackle version looked locked too) */
static void draw_padlock(int x, int y, int letter, bool locked) {
    int r, c;

    screen_color(COL_ACCENT);
    if(!locked) {
        for(r = 0; r < 5; r++)
            for(c = 0; c < 3; c++)
                if(lock_letters[letter][r] & (4 >> c))
                    screen_fill(x + 1 + 2 * c, y + 1 + 2 * r, 2, 2);
        return;
    }
    screen_hline(x + 3, y, 3);
    screen_vline(x + 2, y + 1, 3);
    screen_vline(x + 6, y + 1, 3);
    screen_fill(x, y + 4, 9, 7);

    /* the letter is just holes in the body, it still reads at 1x */
    screen_color(COL_BG);
    for(r = 0; r < 5; r++)
        for(c = 0; c < 3; c++)
            if(lock_letters[letter][r] & (4 >> c))
                screen_hline(x + 3 + c, y + 5 + r, 1);
}

/* BATTERY_W x 7, top left at x, y, two columns of fill per bar (0-4) */
static void draw_battery(int x, int y, int level) {
    screen_hline(x, y, 12);
    screen_hline(x, y + 6, 12);
    screen_vline(x, y, 7);
    screen_vline(x + 11, y, 7);
    screen_vline(x + 12, y + 2, 3);
    if(level > 0)
        screen_fill(x + 2, y + 2, level * 2, 3);
}

static void draw_top_bar(void) {
    int h = top_h(), y = (h - 1 - 11) / 2;

    if(battery < 0 || clock() >= battery_next) {
        battery = power_battery();
        battery_next = clock() + BATTERY_READ;
    }

    screen_color(COL_BG);
    screen_fill(0, 0, SCREEN_W, h);
    screen_color(COL_DIM);
    screen_hline(MARGIN, h - 1, SCREEN_W - 2 * MARGIN);
    draw_battery(SCREEN_W - MARGIN - BATTERY_W, (h - 1 - 7) / 2, battery);

    if(second)
        draw_padlock(MARGIN, y, 0, second_lock);
    if(alpha)
        draw_padlock(MARGIN + LOCK_SLOT, y, 1, alpha_lock);
    if(ctl)
        draw_padlock(MARGIN + 2 * LOCK_SLOT, y, 2, ctl_lock);

    /* the form, when it isn't the normal one, over by the battery */
    if(form != FORM_HAND) {
        const font_t *small = draw_label_font();

        screen_color(COL_DIM);
        draw_text(SCREEN_W - MARGIN - BATTERY_W - 6 - draw_text_width(form_names[form], small), 1,
                  form_names[form], small);
    }
}

/* behind the highlighted half of a history entry. browsing gets just an outline,
   the selection inside it is the highlight then */
static void highlight(int x, int base, const box_t *b) {
    int top = base - b->ascent - 1, h = b->ascent + b->descent + 2;

    if(browsing) {
        screen_color(COL_DIM);
        screen_hline(x - 2, top, b->width + 4);
        screen_hline(x - 2, top + h - 1, b->width + 4);
        screen_vline(x - 2, top, h);
        screen_vline(x + b->width + 1, top, h);
    } else {
        screen_color(COL_SELECT);
        screen_fill(x - 2, top, b->width + 4, h);
    }
}

/* how far up one history entry goes, the same steps draw_screen takes */
static int entry_h(const entry_t *h) {
    const font_t *small = draw_label_font();
    int n = h->out != NULL ? h->out->ascent + h->out->descent + 2 : small->ascent + small->descent + 2;

    if(h->in != NULL)
        n += h->in->ascent + h->in->descent + GAP;
    return n + GAP;
}

static void draw_screen(void) {
    const font_t *small = draw_label_font();
    box_t *in = ed_layout(&ed, &draw_font, true);
    int line_y, y, i, avail = SCREEN_W - 2 * MARGIN, first;

    screen_clear();

    if(page_is_open()) {
        layout_free(in);
        page_draw(top_h() + 2);
        draw_top_bar();
        screen_show();
        return;
    }

    draw_bar();

    /* input line, just above the soft keys. scrolls sideways to keep the cursor in view. */
    if(in != NULL) {
        int x = MARGIN, base = SCREEN_H - BAR_H - 3 - in->descent;

        if(in->width > avail) {
            int cx = cursor_x(in, 0), shift = cx - avail * 3 / 4;

            if(shift > in->width - avail)
                shift = in->width - avail;
            if(shift > 0)
                x -= shift;
        }
        screen_color(COL_FG);
        draw_box(in, x, base);
        cur_x = cursor_x(in, x);
        cur_top = base - in->ascent;
        line_y = base - in->ascent - 3;
        layout_free(in);
    } else {
        line_y = SCREEN_H - BAR_H - 20;
    }

    screen_color(COL_DIM);
    screen_hline(MARGIN, line_y, SCREEN_W - 2 * MARGIN);

    /* messages at the right of the line */
    if(status != NULL) {
        screen_color(COL_DIM);
        draw_text(SCREEN_W - MARGIN - draw_text_width(status, small),
                  line_y - small->ascent - small->descent - 1, status, small);
    }

    /* history, newest at the bottom, going up to the status bar (drawn over
       the top of the last one). scrolled so the highlighted one fits */
    i = history_len - 1;
    if(hist >= 0) {
        int below = 0, k;

        for(k = hist; k <= i; k++)
            below += entry_h(&history[k]);
        for(; i > hist && below > line_y - GAP - top_h(); i--)
            below -= entry_h(&history[i]);
    }
    /* the status line takes the bottom row, else it lands on the last message */
    y = line_y - GAP - (status != NULL ? small->ascent + small->descent + 2 : 0);
    /* the cleared ones stay hidden unless you've gone up into them */
    first = hist >= 0 && hist < cleared ? 0 : cleared;
    for(; i >= first && y > top_h(); i--) {
        entry_t *h = &history[i];
        box_t *out = h->out, *inb = h->in, *browse = NULL;

        /* when browsing, the editor's version (cursor, selection and all) goes in
           place of the highlighted half */
        if(i == hist && browsing && (browse = ed_layout(&view, &draw_font, true)) != NULL) {
            if(hist_out)
                out = browse;
            else
                inb = browse;
        }

        if(out != NULL) {
            int base = y - out->descent, x = SCREEN_W - MARGIN - out->width;

            if(i == hist && hist_out)
                highlight(x, base, out);
            screen_color(COL_FG);
            draw_box(out, x, base);
            if(h->ms >= 100) {
                char t[32];

                snprintf(t, sizeof t, "%lu.%lus", h->ms / 1000, h->ms % 1000 / 100);
                screen_color(COL_DIM);
                draw_text(MARGIN, base - small->ascent, t, small);
            }
            y -= out->ascent + out->descent + 2;
        } else {
            screen_color(COL_DIM);
            draw_text(SCREEN_W - MARGIN - draw_text_width(h->message, small),
                      y - small->ascent - small->descent, h->message, small);
            y -= small->ascent + small->descent + 2;
        }

        if(inb != NULL) {
            if(i == hist && !hist_out)
                highlight(MARGIN, y - inb->descent, inb);
            screen_color(COL_FG);
            draw_box(inb, MARGIN, y - inb->descent);
            y -= inb->ascent + inb->descent + GAP;
        }
        layout_free(browse);

        screen_color(COL_DIM);
        screen_hline(MARGIN + 20, y + 1, SCREEN_W - 2 * MARGIN - 40);
        y -= GAP;
    }
    draw_top_bar();

    if(menu >= 0)
        draw_menu();
    if(popup >= 0)
        draw_popup();

    screen_show();
}

/* [on]+this stops a slow answer, like [on] breaks a program on the TI */
/* #define CANCEL_KEY K_ON   // plain [on] is the TI's own break key, dont */
#define CANCEL_KEY K_MODE

static clock_t busy_next;
static unsigned busy_frame;

/* a dot going round in the status bar (left of the battery) while the engine
   runs, so a slow simplify doesn't look frozen. drawn straight onto the screen,
   swapping buffers made the cursor flash */
static void draw_working(void) {
    /* 2x2 dots round an 8x8 box, clockwise from the top */
    static const unsigned char dots[8][2] = {
        { 3, 0 }, { 5, 1 }, { 6, 3 }, { 5, 5 }, { 3, 6 }, { 1, 5 }, { 0, 3 }, { 1, 1 }
    };
    int x = SCREEN_W - MARGIN - BATTERY_W - 6 - 8, y = (top_h() - 1 - 8) / 2;
    unsigned i;

    /* a head and two dimmer dots behind it, going round */
    screen_direct(true);
    for(i = 0; i < 8; i++) {
        unsigned behind = (busy_frame - i) % 8;

        screen_color(behind == 0 ? COL_ACCENT : behind <= 2 ? COL_DIM : COL_BG);
        screen_fill(x + dots[i][0], y + dots[i][1], 2, 2);
    }
    screen_direct(false);
}

/* what the keys looked like at the last poll, for edges */
static clock_t busy_keys_next;
static key_t busy_key;
static bool busy_on, busy_on_chord;

/* pcas_poll, the engine calls this constantly, so check the clock and only read
   the keys 40 times a second and draw a frame 10 times a second.

   a modifier tapped while it's working still counts for the next key (they used
   to just vanish, keys_peek marks whatever's down as held). 2nd / alpha on the
   press, [on] on the release and only if nothing came with it, since [on]+[mode]
   is the cancel. 40 Hz so a quick tap doesn't fall between two polls */
static bool busy_poll(void) {
    keypress_t p;

    if(clock() < busy_keys_next)
        return false;
    busy_keys_next = clock() + CLOCKS_PER_SEC / 40;
    if(clock() >= busy_next) {
        busy_next = clock() + CLOCKS_PER_SEC / 10;
        busy_frame++;
        draw_working();
    }

    p = keys_peek();
    if(p.key != busy_key && (p.key == K_2ND || p.key == K_ALPHA) && !p.on)
        modifier(p.key);
    if(p.on && !busy_on)
        busy_on_chord = false;
    if(p.on && p.key != K_NONE)
        busy_on_chord = true;
    if(!p.on && busy_on && !busy_on_chord)
        modifier(K_ON);
    busy_key = p.key;
    busy_on = p.on;

    return p.on && p.key == CANCEL_KEY;
}

static void busy_start(void) {
    pcas_cancelled = false;
    pcas_oom = false;
    busy_frame = 0;
    busy_next = clock() + CLOCKS_PER_SEC / 10;
    busy_keys_next = busy_next;
    busy_key = K_NONE;
    busy_on = busy_on_chord = false;
    draw_working();
}

/* keys_wait calls this when it goes dark or wakes up. the waking key already got
   eaten down there, so all that's left here is a redraw */
static void on_sleep(bool asleep) {
    screen_power(!asleep);
    if(!asleep)
        draw_screen();
}

/* main loop */

void app_run(void) {
    if(!ed_init(&ed))
        return;

    names_init();
    history_load();
    screen_begin();
    settings_load();    /* after, it sets the palette and gfx_Begin would put its own back */
    keys_begin();
    draw_screen();
    pcas_poll = busy_poll;
    keys_on_sleep = on_sleep;

    for(;;) {
        keypress_t p = keys_wait();

        if(p.key == K_ENTER && menu < 0 && !second && !alpha && !ed_is_empty(&ed))
            busy_start(); /* the form keys start it themselves */
        if(!handle(p))
            break;
        draw_screen();
    }

    keys_end();
    screen_end();
    history_save();
    names_save();
    settings_save();
    clear_history();
    menu_free_boxes();
    ed_free(&ed);
    if(preview.root != NULL)
        ed_free(&preview);
    id_UnloadAll();
}
