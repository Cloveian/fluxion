#include "app.h"
#include "areas.h"
#include "args.h"
#include "names.h"
#include "vars.h"
#include "mode.h"
#include "settings.h"
#include "geom.h"
#include "polyfn.h"
#include "calc1.h"
#include "series.h"
#include "platform/clipboard.h"
#include "tri.h"
#include "ineq.h"
#include "conic.h"
#include "piece.h"
#include "shapes.h"
#include "range.h"
#include "work.h"
#include "bind.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "engine/ast.h"
#include "engine/dbg.h"
#include "engine/parser.h"
#include "engine/cas/cas.h"
#include "engine/cas/identities.h"
#include "engine/cas/steps.h"
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
#define HISTORY_MAX 16          /* enough for now, old ones just scroll off the top */

/* the nspire has no keys under its screen: no folder bar, [menu] drops a list of the five down from the top like its own */
#ifdef _TINSPIRE
#define NSPIRE 1
#else
#define NSPIRE 0
#endif

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
static unsigned mods, mods_lock;    /* M_ bits tapped for the next key, and the ones that stay on after it */
static const char *status;      /* one-line message above the input, or NULL */

static int menu = -1;           /* open folder, -1 = none */
static const folder_t *menu_in; /* folders[menu] or one of its subfolders */
static int menu_sel, menu_top;
/* the folders above the one showing and the row picked in each, for going back up.
   the nspire's [menu] starts a level higher, at the list of the five */
#define MENU_DEPTH_MAX 4
static const folder_t *menu_back[MENU_DEPTH_MAX];
static int menu_back_sel[MENU_DEPTH_MAX], menu_depth;
static int menu_x, menu_y, menu_w, menu_row_h, menu_rows; /* the open menu's box, where draw_menu put it, for clicks */
#define HITS_MAX 24 /* history halves on screen at once, for clicks */
static struct { int y0, y1, id; } hits[HITS_MAX];
static int hit_n;
static int in_left, in_base, in_top; /* where draw_screen put the input line (left edge, baseline) and the band it's in, for mouse clicks */
static int menu_from = -1;      /* the answer highlighted when it opened, for show steps */

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

#ifdef COMPILE_PC
    if(getenv("FLX_WORK") != NULL) /* for fitting the cost estimate */
        fprintf(stderr, "work %lu ms %lu\n", pcas_work, ms);
#endif
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

/* ans, for the editor. the newest answer there is, an error or a "stored"
   doesn't wipe it (the TI's Ans doesn't either) */
static const ed_row_t *last_answer(void) {
    int i;

    for(i = history_len - 1; i >= 0; i--)
        if(history[i].out_row != NULL)
            return history[i].out_row;
    return NULL;
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

/* a root anywhere, a ^ with a fraction up top or a root node. comdenom rationalizes
   those into a mess, so the by hand fraction only keeps them if they got smaller */
static bool has_root(const pcas_ast_t *e) {
    const pcas_ast_t *k;

    if(e->type != NODE_OPERATOR)
        return false;
    if(optype(e) == OP_ROOT)
        return true;
    if(optype(e) == OP_POW && e->op.operator.base != NULL) {
        const pcas_ast_t *exp = e->op.operator.base->next;

        if(exp != NULL && (exp->type != NODE_NUMBER || !mp_rat_is_integer(exp->op.num)))
            return true;
    }
    for(k = e->op.operator.base; k != NULL; k = k->next)
        if(has_root(k))
            return true;
    return false;
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

        if(steps_size(e) > 60 || !worth_expanding(e))
            break;
        x = ast_Copy(e);
        if(x == NULL)
            break;
        expand(x, EXP_ALL);
        simplify(x, SIMP_BASIC | SIMP_EVAL | SIMP_LIKE_TERMS);
        if(!pcas_cancelled && steps_size(x) < steps_size(e))
            replace_node(e, x);
        else
            ast_Cleanup(x);
        break;
    }
    }

    /* and one fraction, x/2 + 1/3 is (3x+2)/6 and 1/x + 1/(x+1) is (2x+1)/(x(x+1)),
       bottom factored and top multiplied out like the nspire. kept only if it's not
       bigger, sqrt(2)/2 + 1/2 came out as ((sqrt(2)+2)sqrt(2))/4 otherwise. not a trig
       family either, pi/6 + 2pi n reads better as a sum */
    if(form == FORM_HAND && !pcas_cancelled && !has_symbol(e, SYM_N)) {
        pcas_ast_t *x = ast_Copy(e);

        if(x == NULL)
            return;
        comdenom(x, true);
        if(isoptype(x, OP_DIV)) {
            expand(ast_ChildGet(x, 0), EXP_ALL);
            simplify(ast_ChildGet(x, 0), SIMP_BASIC | SIMP_EVAL | SIMP_LIKE_TERMS);
        }
        if(!pcas_cancelled && (steps_size(x) <= steps_size(e) || !has_root(x)))
            replace_node(e, x);
        else
            ast_Cleanup(x);
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

/* one answer value, over one denominator, number first. the trig families stay
   as a sum, pi/6 + 2pi n reads better than (12pi n + pi)/6 */
box_t *value_box(pcas_ast_t *v) {
    if(!has_symbol(v, SYM_N))
        comdenom(v, false);
    simplify_canonical_form(v, canon_flags);
    if(form == FORM_DECIMAL) {
        approx(v);
        simplify(v, SIMP_BASIC | SIMP_EVAL);
    }
    number_first(v);
    return layout_build(v, &draw_font);
}

static void add_values(box_t *row, pcas_ast_t **v, int n) {
    int i;

    for(i = 0; i < n; i++) {
        if(i > 0) {
            layout_add(row, layout_text(",", 0));
            layout_add(row, layout_space(2 * SPACE_BINARY));
        }
        layout_add(row, value_box(v[i]));
    }
}

/* -5, 6 */
box_t *values_box(pcas_ast_t **v, int n) {
    box_t *row = layout_box(BOX_ROW, 0);

    add_values(row, v, n);
    layout_finish(row, &draw_font);
    return row;
}

/* x = -5, 6 */
box_t *solutions_box(char var, pcas_ast_t **roots, int n) {
    char name[2] = { var, '\0' };
    box_t *row = layout_box(BOX_ROW, 0);

    layout_add(row, layout_text(name, 0));
    layout_add(row, layout_space(LAYOUT_SPACE(0)));
    layout_add(row, layout_text("=", 0));
    layout_add(row, layout_space(LAYOUT_SPACE(0)));
    add_values(row, roots, n);
    layout_finish(row, &draw_font);
    return row;
}

/* ( a, b ) */
box_t *point_box(pcas_ast_t *x, pcas_ast_t *y) {
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

/* "vertical    x = -2, 2", one line of an asym() answer. takes the values row */
box_t *labelled(const char *label, box_t *values) {
    box_t *row = layout_box(BOX_ROW, 0);

    layout_add(row, layout_text(label, 0));
    layout_add(row, layout_space(3 * SPACE_BINARY));
    layout_add(row, values);
    return row;
}

/*
    the line inside solve(equation, variable). the engine's parser has no =, so the line is split
    here: at the = and the last comma outside brackets. no variable: x. *rhs is NULL
    with no =, *rel the < > <= >= when it's one of those instead. takes src apart
    (mutates src! careful). NULL when it read, or what to say
*/
const char *read_equation(char *src, pcas_ast_t **lhs, pcas_ast_t **rhs, Symbol *var, char *shown, char *rel) {
    char *eq = NULL, *comma = NULL, *s;
    pcas_error_t err;
    int depth = 0;

    *lhs = *rhs = NULL;
    *var = SYM_X;
    *shown = 'x';
    *rel = 0;
    for(s = src; *s != '\0'; s++) {
        if(*s == '(')
            depth++;
        else if(*s == ')')
            depth--;
        else if(depth == 0 && eq == NULL && (*s == '<' || *s == '>' || *s == CH_LE || *s == CH_GE)) {
            /* an inequality, ineq.c does those. < then = typed as two counts as <= */
            *rel = *s;
            if((*s == '<' || *s == '>') && s[1] == '=') {
                *rel = *s == '<' ? CH_LE : CH_GE;
                *s++ = ' '; /* the left side gets a space at the end, the = is where it splits */
            }
            eq = s;
        } else if(depth == 0 && *s == '=' && eq == NULL)
            eq = s;
        else if(depth == 0 && *s == ',')
            comma = s;
    }

    if(comma != NULL) {
        *comma = '\0';
        if(!args_var(comma + 1, var, shown))
            return "solve(equation, variable)";
    }

    if(eq != NULL)
        *eq = '\0';
    *lhs = parse((const uint8_t *)src, strlen(src), str_table, &err);
    if(err == E_SUCCESS && eq != NULL)
        *rhs = parse((const uint8_t *)eq + 1, strlen(eq + 1), str_table, &err);

    if(err != E_SUCCESS || *lhs == NULL || (eq != NULL && *rhs == NULL)) {
        ast_Cleanup(*lhs);
        ast_Cleanup(*rhs);
        *lhs = *rhs = NULL;
        return err != E_SUCCESS ? error_text[err] : "solve(equation, variable)";
    }
    return NULL;
}

static void run_solve(char *src, box_t *in) {
    pcas_ast_t *roots[SOLVE_MAX] = { NULL }, *e, *rhs;
    char shown, rel;
    Symbol var;
    const char *why = read_equation(src, &e, &rhs, &var, &shown, &rel);
    clock_t start;
    unsigned long ms;
    int n, i;

    if(why != NULL) {
        push(in, NULL, why, 0);
        return;
    }

    /* a = b  ->  a - b = 0 */
    if(rhs != NULL)
        e = ast_MakeBinary(OP_ADD, e, ast_MakeBinary(OP_MULT, ast_MakeInt(-1), rhs));

    if(setting_angle == ANGLE_DEGREES)
        e = degrees_in(e);
    if(rel != 0) {
        ineq_run(e, var, shown, rel, in);
        ast_Cleanup(e);
        return;
    }
    start = clock();
    n = solve_ex(e, var, roots, true);
    if(n > 0 && setting_trig == TRIG_ONE_TURN) {
        pcas_ast_t *hi = setting_angle == ANGLE_DEGREES
                         ? ast_MakeInt(360)
                         : ast_MakeBinary(OP_MULT, ast_MakeInt(2), ast_MakeSymbol(SYM_PI));

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
    the f and the variable of asym(f, x) and domain(f, x): split at the last comma
    outside brackets (writes into src), parsed, in radians. NULL once it's pushed
    what went wrong, usage is the line for a bad variable
*/
pcas_ast_t *read_f(char *src, box_t *in, const char *usage, Symbol *var, char *shown) {
    pcas_ast_t *e;
    pcas_error_t err;
    char *comma = NULL, *s;
    int depth = 0;

    *var = SYM_X;
    *shown = 'x';
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
        if(!args_var(comma + 1, var, shown)) {
            push(in, NULL, usage, 0);
            return NULL;
        }
    }

    e = parse((const uint8_t *)src, strlen(src), str_table, &err);
    if(err != E_SUCCESS || e == NULL) {
        if(e != NULL)
            ast_Cleanup(e);
        push(in, NULL, error_text[err], 0);
        return NULL;
    }
    if(setting_angle == ANGLE_DEGREES)
        e = degrees_in(e);
    return e;
}

/*
    asym(expression, variable). holes and asymptotes, a line for each kind it
    actually has. writes into src, see read_f
*/
static void run_asym(char *src, box_t *in) {
    pcas_ast_t *e;
    asym_t a;
    char shown;
    Symbol var;
    clock_t start;
    unsigned long ms;
    bool ok;
    box_t *col;

    e = read_f(src, in, "asym(expression, variable)", &var, &shown);
    if(e == NULL)
        return;
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
    are out. read_f like asym
*/
static void run_domain(char *src, box_t *in) {
    pcas_ast_t *e;
    domain_t d;
    char shown;
    Symbol var;
    clock_t start;
    unsigned long ms;
    bool ok;
    box_t *col;

    if((e = read_f(src, in, "domain(expression, variable)", &var, &shown)) == NULL)
        return;

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
    if(d.lower != NULL && d.upper != NULL && ast_Compare(d.lower, d.upper)) {
        /* sqrt(x-2)+sqrt(2-x), one point. open on either side and domain() already said empty */
        layout_add(col, bound_box(shown, '=', d.lower));
    } else {
        if(d.lower != NULL)
            layout_add(col, bound_box(shown, d.lower_open ? '>' : CH_GE, d.lower));
    }
    if(d.upper != NULL && !(d.lower != NULL && ast_Compare(d.lower, d.upper)))
        layout_add(col, bound_box(shown, d.upper_open ? '<' : CH_LE, d.upper));
    if(d.nholes > 0)
        layout_add(col, labelled("except", solutions_box(shown, d.holes, d.nholes)));
    if(d.partial)
        layout_add(col, layout_text("(and maybe more)", 0));
    layout_finish(col, &draw_font);
    push(in, col, NULL, ms);
    domain_free(&d);
}

/* 12 = 6*2, 23x = 22x, x + 1 > x: true when it holds for every x, with a point it fails at when it doesn't */
static void run_truth(char *src, box_t *in) {
    pcas_ast_t *l, *r;
    Symbol var;
    char shown, sign, rel, detail[64] = "";
    const char *why = read_equation(src, &l, &r, &var, &shown, &sign);
    box_t *col;
    truth_t t;
    clock_t start;
    int res, j, used = 0;

    if(why != NULL || r == NULL) {
        ast_Cleanup(l);
        push(in, NULL, why != NULL ? why : "= goes in solve(, system(, piecewise(, implicit( or a conic", 0);
        return;
    }
    if(setting_angle == ANGLE_DEGREES) {
        l = degrees_in(l);
        r = degrees_in(r);
    }
    rel = sign == CH_LE ? 'l' : sign == CH_GE ? 'g' : sign == 0 ? '=' : sign;
    start = clock();
    res = truth_of(l, r, rel, &t);
    ast_Cleanup(l);
    ast_Cleanup(r);
    if(pcas_cancelled) {
        push(in, NULL, pcas_oom ? "out of memory" : "cancelled", 0);
        return;
    }
    if(res == TRUTH_FALSE && t.nvars > 0) { /* x = 1, y = 1/2 */
        for(j = 0; j < t.nvars && used < (int)sizeof detail - 12; j++)
            used += snprintf(detail + used, sizeof detail - (size_t)used, t.den[j] == 1 ? "%s%c = %d" : "%s%c = %d/%d", j > 0 ? ", " : "",
                             (char)(t.var[j] - 'A' + 'a'), t.num[j], t.den[j]);
    } else if(res == TRUTH_TRUE && t.points > 0) {
        snprintf(detail, sizeof detail, "checked at %d points, not a proof", t.points);
    }
    col = layout_box(BOX_COL, 0);
    layout_add(col, word_box(res == TRUTH_TRUE ? "true" : res == TRUTH_FALSE ? "false" : "can't tell"));
    if(detail[0] != '\0')
        layout_add(col, word_box(detail));
    layout_finish(col, &draw_font);
    push(in, col, NULL, (unsigned long)((clock() - start) * 1000 / CLOCKS_PER_SEC));
}

/*
    system(eq, eq, ..., x, y). the pieces between the top level commas with an =
    are equations, the rest are the variables. no variables given, it takes the
    letters it finds, a to z. writes into src like the other two. NULL when eqs
    (each lhs - rhs), vars and shown are filled and count is how many, else what to say
*/
const char *read_system(char *src, pcas_ast_t **eqs, Symbol *vars, char *shown, int *count) {
    pcas_ast_t *rhs;
    char *piece[2 * SYSTEM_MAX], *s;
    pcas_error_t err = E_SUCCESS;
    int depth = 0, pieces = 1, n = 0, nvars = 0, i;
    bool too_many = false;

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
            if(nvars == SYSTEM_MAX || !args_var(piece[i], &vars[nvars], &shown[nvars]))
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
        eqs[n] = ast_MakeBinary(OP_ADD, eqs[n], ast_MakeBinary(OP_MULT, ast_MakeInt(-1), rhs));
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
        return err != E_SUCCESS ? error_text[err]
               : too_many ? "4 equations is the most it does"
               : "system(eq, eq, x, y), as many letters as equations";
    }
    *count = n;
    return NULL;
}

static void run_system(char *src, box_t *in) {
    pcas_ast_t *eqs[SYSTEM_MAX] = { NULL }, *out[SYSTEM_SETS][SYSTEM_MAX] = { { NULL } };
    char shown[SYSTEM_MAX];
    Symbol vars[SYSTEM_MAX];
    clock_t start;
    unsigned long ms;
    int n = 0, i, k, got;
    box_t *row, *col;
    const char *why = read_system(src, eqs, vars, shown, &n);

    if(why != NULL) {
        push(in, NULL, why, 0);
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
        push(in, NULL, "can't solve this one yet", ms);
    else if(got == SOLVE_ALL)
        push(in, NULL, "infinitely many, they say the same thing", ms);
    else if(got == 0)
        push(in, word_box("false"), NULL, ms);
    else {
        /* one line per solution set, an "or" line between them like the nspire */
        col = layout_box(BOX_COL, 0);
        for(k = 0; k < got && k < SYSTEM_SETS; k++) {
            if(k > 0)
                layout_add(col, layout_text("or", 0));
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
                layout_add(row, value_box(out[k][i]));
            }
            layout_add(col, row);
        }
        layout_finish(col, &draw_font);
        push(in, col, NULL, ms);
    }
    for(k = 0; k < SYSTEM_SETS; k++)
        for(i = 0; i < n; i++)
            ast_Cleanup(out[k][i]);
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
    if(cmd != NULL && (strcmp(cmd, "solve") == 0 || strcmp(cmd, "roots") == 0)) {
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
    if(ed_has_command(src)) {
        free(src);
        status = "commands have to be the whole line (for now)";
        return false;
    }

    /* roots(p, x) is solve(p = 0, x), which is what solve does without an = anyway */
    if(cmd != NULL && (strcmp(cmd, "solve") == 0 || strcmp(cmd, "roots") == 0)) {
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
    if(cmd == NULL && (strpbrk(src, "=<>") != NULL || strchr(src, CH_LE) != NULL || strchr(src, CH_GE) != NULL)) {
        run_truth(src, ed_layout(line, &draw_font, false));
        free(src);
        return true;
    }
    /* ellipse( and hyperbola( take one too, they're let through to conic_run.
       piecewise( for its x=v (and x = 0 as a condition), implicit( for the equation */
    if(strchr(src, '=') != NULL
       && (strcmp(cmd, "ellipse") != 0 && strcmp(cmd, "hyperbola") != 0
           && strcmp(cmd, "piecewise") != 0 && strcmp(cmd, "implicit") != 0)) {
        free(src);
        status = "= goes in solve(, system(, piecewise(, implicit( or a conic";
        return false;
    }
    in = ed_layout(line, &draw_font, false);
    if(geom_run(cmd, src, in) || polyfn_run(cmd, src, in) || tri_run(cmd, src, in)
       || conic_run(cmd, src, in) || piece_run(cmd, src, in) || shapes_run(cmd, src, in)
       || range_run(cmd, src, in) || calc1_run(cmd, src, in) || series_run(cmd, src, in)) {
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
        /* primes come back in order, no canonical form. its sort puts the 5 of 2^3*3^2*5 first and 2^2*3^2 becomes (2*3)^2 */
        primes = factor_command(e);
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

/* the Tools rows that do something, no [x] in front */
static bool is_action(const item_t *it) {
    return strcmp(it->label, "clear history") == 0 || strcmp(it->label, "show steps") == 0;
}

/* the steps page for the answer highlighted when the menu opened, or the last one. solve(, system(, expand(, factor(, tangent( and its kin, d/dx, limit( and integral( */
static void show_steps(void) {
    int at = menu_from >= 0 ? menu_from : history_len - 1;
    const char *cmd;
    char *src;

    if(at < 0 || !ed_load(&view, history[at].row, true)) {
        status = "nothing to show the work for";
        return;
    }
    cmd = ed_command(&view);
    if(cmd != NULL && (strcmp(cmd, "expand") == 0 || strcmp(cmd, "factor") == 0)) {
        pcas_error_t err;
        pcas_ast_t *e;

        src = ed_command_arg(&view);
        e = src != NULL ? args_parse_deg(src, &err) : NULL;
        free(src);
        if(e == NULL) {
            status = "nothing to show the work for";
            return;
        }
        status = work_open_expr(strcmp(cmd, "expand") == 0 ? 'e' : 'f', e);
        return;
    }
    if(cmd != NULL && (strcmp(cmd, "tangent") == 0 || strcmp(cmd, "critical") == 0 || strcmp(cmd, "extrema") == 0
                       || strcmp(cmd, "inflect") == 0 || strcmp(cmd, "implicit") == 0)) {
        src = ed_command_arg(&view);
        status = src != NULL ? calc1_work(cmd, src) : "nothing to show the work for";
        free(src);
        return;
    }
    if(cmd == NULL) {
        /* d/dx(...), limit(...) or integral(...) is the whole line, or it isn't one */
        pcas_error_t err;
        pcas_ast_t *e;

        src = ed_to_engine(&view);
        e = src != NULL ? args_parse_deg(src, &err) : NULL; /* like run(), so the steps end at its answer */
        free(src);
        if(e != NULL) { /* anything but those 3 gets simplified a phase at a time */
            status = isoptype(e, OP_DERIV) || isoptype(e, OP_LIMIT) || isoptype(e, OP_INTEGRAL) ? work_open_calc(e) : work_open_expr('s', e);
            return;
        }
    }
    if(cmd == NULL || (strcmp(cmd, "solve") != 0 && strcmp(cmd, "roots") != 0 && strcmp(cmd, "system") != 0)) {
        status = "steps are for solve(, system(, expand(, factor(, d/dx, limit( and integral(";
        return;
    }
    src = ed_command_arg(&view);
    if(src == NULL) {
        status = "nothing to show the work for";
        return;
    }
    status = strcmp(cmd, "system") == 0 ? work_open_system(src) : work_open(src);
    free(src);
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

/* [alpha][enter]. the last answer as a decimal just this once, the next one comes
   out in whatever form was on before (or exact once, if decimal is the form) */
static void decimal_once(void) {
    form_t keep = form;

    form_set(keep == FORM_DECIMAL ? FORM_HAND : FORM_DECIMAL);
    form = keep;
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
    return menu_in->items[i];
}

/* Tools - which simp_flags bit each setting maps to, 0 if it isnt one */
static unsigned short setting(const item_t *it) {
    static const struct { const char *label; unsigned short bit; } ids[] = {
        { "basic identities",       SIMP_ID_GENERAL },
        { "trig identities",        SIMP_ID_TRIG },
        { "hyperbolic identities",  SIMP_ID_HYPERBOLIC },
        { "complex identities",     SIMP_ID_COMPLEX },
        { "trig constants",         SIMP_ID_TRIG_CONSTANTS },
        { "inverse trig constants", SIMP_ID_TRIG_INV_CONSTANTS },
    };
    unsigned i;

    for(i = 0; i < sizeof ids / sizeof ids[0]; i++)
        if(strcmp(it->label, ids[i].label) == 0)
            return ids[i].bit;
    return 0;
}

static bool setting_on(const item_t *it) {
    return (simp_flags & setting(it)) != 0;
}

static void type_text(editor_t *e, const char *s) {
    for(; *s != '\0'; s++)
        ed_insert_char(e, *s);
}

/* x^2, x^-1. make an exponent with this in it, then step back out */
static void power_of(editor_t *e, const char *what) {
    ed_insert(e, ED_POW, NULL);
    type_text(e, what);
    ed_right(e);
}

/* the typing ones into e, false if a isn't typing */
static bool type_action(editor_t *e, action_t a) {
    switch(a.op) {
    case A_CHAR:     ed_insert_char(e, (char)a.n); break;
    case A_FUNC:     ed_insert(e, ED_FUNC, a.s); break;
    case A_TEMPLATE: ed_insert(e, (ed_kind_t)a.n, NULL); break;
    case A_POWER:    power_of(e, a.s); break;
    case A_TYPE:
        type_text(e, a.s);
        if(a.n != 0)
            ed_insert(e, (ed_kind_t)a.n, NULL);
        break;
    default:
        return false;
    }
    return true;
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
        unsigned short bit = setting(it);

        if(strcmp(it->label, "clear history") == 0) {
            if(!dry)
                clear_history();
            return true;
        }
        if(strcmp(it->label, "show steps") == 0) {
            if(!dry)
                show_steps();
            return true;
        }
        if(bit == 0)
            return false;
        if(!dry)
            simp_flags ^= bit;
        return true;
    }

    if(s == NULL)
        return false;

    if(it->kind == ITEM_CHAR) {
        static const struct { const char *engine; char c; } chars[] = {
            { "pi", CH_PI }, { "e", CH_EULER }, { "i", CH_IMAG }, { "theta", CH_THETA }, { "!", '!' }, { "%", '%' },
            { ".", '.' }, { "-", ED_NEG }, { "[E]", ED_SCI }, { "+", ED_PLUS }, { "_", ED_SUB },
            { "*", ED_TIMES }, { "/", ED_DIVIDE }, { "(", '(' }, { ")", ')' }, { ",", ',' },
            { "=", '=' }, { "X", 'x' },
        };
        static const char neg_one[] = { ED_NEG, '1', '\0' };
        unsigned i;

        if(strcmp(s, "^2") == 0 || strcmp(s, "^(-1)") == 0) {
            if(!dry)
                power_of(into, s[1] == '2' ? "2" : neg_one);
            return true;
        }
        for(i = 0; i < sizeof chars / sizeof chars[0]; i++)
            if(strcmp(s, chars[i].engine) == 0) {
                if(!dry)
                    ed_insert_char(into, chars[i].c);
                return true;
            }
        return false;   /* 0-9 and A-Z, a whole row of keys */
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
    /* no template for the at one, a limit or an integral, they'd need a second slot in the editor. so
       they go in typed out, deriv(f,x,3), limit(f,x,a) and integral(f,x,a,b) already work */
    if(strcmp(s, "deriv($,X,$)") == 0 || strcmp(s, "limit($,X,$)") == 0 || strcmp(s, "integral($,X)") == 0) {
        if(!dry) {
            const char *c;

            for(c = s; *c != '$'; c++) /* deriv(, limit( or integral( */
                ed_insert_char(into, *c);
        }
        return true;
    }

    /* name($), the functions plus the commands the app runs itself */
    {
        const char *paren = strchr(s, '(');
        char name[12];
        size_t n = paren != NULL ? (size_t)(paren - s) : 0;

        /* gcd($,$) and co are one slot too, the comma just gets typed in it */
        if(n == 0 || n >= sizeof name || (strcmp(paren, "($)") != 0 && strcmp(paren, "($,$)") != 0))
            return false;
        memcpy(name, s, n);
        name[n] = '\0';
        if(it->kind == ITEM_COMMAND && (strcmp(paren, "($)") != 0 || !ed_is_command(name)))
            return false;
        if(!dry)
            ed_insert(into, ED_FUNC, name);
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
    const folder_t *f = menu_in;
    layout_font_t small = draw_font;
    unsigned i;

    if(small.base > 0)
        small.base--;
    if(preview.root == NULL && !ed_init(&preview))
        return;
    for(i = 0; i < f->count && i < sizeof menu_boxes / sizeof menu_boxes[0]; i++) {
        const item_t *it = f->items[i];

        menu_boxes[i] = NULL;
        if(it->kind == ITEM_FOLDER || it->kind == ITEM_SETTING || it->kind == ITEM_SOFTKEY || !pick(it, true))
            continue; /* subfolders, settings, keys and the greyed out ones stay text */
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

static void menu_show(const folder_t *f, int sel) {
    menu_free_boxes();
    menu_in = f;
    menu_sel = sel;
    menu_top = 0;
    menu_build_boxes();
}

static void menu_open(int folder) {
    menu = folder;
    menu_depth = 0;
    menu_show(&folders[folder], 0);
}

/* nspire: the list of the five, its folders open like subfolders */
static void menu_open_top(void) {
    menu = 0;
    menu_depth = 0;
    menu_show(&menu_top_folder, 0);
}

/* nspire: [menu] on a page with keys under the screen lists those five */
static item_t soft_items[5];
static const item_t *soft_list[5];
static const folder_t soft_folder = { NULL, "keys", soft_list, 5 };

static void menu_open_soft(const char *const *names) {
    int i;

    for(i = 0; i < 5; i++) {
        soft_items[i].label = names[i];
        soft_items[i].kind = ITEM_SOFTKEY;
        soft_items[i].have = HAVE_YES;
        soft_list[i] = &soft_items[i];
    }
    menu = 0;
    menu_depth = 0;
    menu_show(&soft_folder, 0);
}

/* back up one, false at the top already */
static bool menu_up(void) {
    if(menu_depth == 0)
        return false;
    menu_depth--;
    menu_show(menu_back[menu_depth], menu_back_sel[menu_depth]);
    return true;
}

static void menu_choose(int i) {
    const item_t *it = menu_item(i);

    if(it->kind == ITEM_FOLDER) {
        if(menu_depth < MENU_DEPTH_MAX) {
            menu_back[menu_depth] = menu_in;
            menu_back_sel[menu_depth++] = i;
        }
        menu_show(it->sub, 0);
        return;
    }
    if(it->kind == ITEM_SOFTKEY) {  /* as if it was pressed under the page */
        action_t a = { A_SOFT, i, NULL };

        menu_close();
        if(page_is_open())
            page_act(a);
        else
            work_act(a);
        return;
    }
    if(it->kind != ITEM_SETTING && pick(it, true))
        will_edit(false);
    if(!pick(it, false)) {
        status = "not in Fluxion yet";
        return;
    }
    /* settings stay open so several can be toggled. show steps opened its page, hidden under it the menu ate the keys */
    if(it->kind != ITEM_SETTING || work_is_open() || status != NULL) /* the menu sits over the status message */
        menu_close();
}

/* left / right go round the five on the CE. the nspire has them in a list, and a page's keys aren't folders */
static bool round_five(void) {
    return !NSPIRE && menu_in == &folders[menu];
}

/* false if it means nothing in a menu. rows past 9 go A, B, C like the nspire */
static bool menu_act(action_t a) {
    int count = (int)menu_in->count, n = -1;

    switch(a.op) {
    case A_UP:    menu_sel = (menu_sel + count - 1) % count; break;
    case A_DOWN:  menu_sel = (menu_sel + 1) % count; break;
    case A_LEFT:
        if(!menu_up() && round_five())
            menu_open((menu + FOLDER_COUNT - 1) % FOLDER_COUNT);
        break;
    case A_RIGHT:
        if(menu_item(menu_sel)->kind == ITEM_FOLDER)
            menu_choose(menu_sel);
        else if(round_five())
            menu_open((menu + 1) % FOLDER_COUNT);
        break;
    case A_ENTER: menu_choose(menu_sel); break;
    case A_CLEAR: case A_MENU: menu_close(); break;
    case A_ESC:   /* one level up, out from the top */
        if(!menu_up())
            menu_close();
        break;
    case A_SOFT:
        if(menu_in == &soft_folder)     /* a page's keys listed, so that one */
            menu_choose(a.n);
        else if(a.n == menu)
            menu_close();
        else
            menu_open(a.n);
        break;
    case A_CHAR:
        if(a.n >= '1' && a.n <= '9')
            n = a.n - '1';
        else if(a.n >= 'a' && a.n <= 'z')
            n = 9 + (a.n - 'a');
        else if(a.n >= 'A' && a.n <= 'Z')
            n = 9 + (a.n - 'A');
        else
            return false;
        if(n < count)
            menu_choose(n);
        break;
    default:
        return false;
    }
    return true;
}

/* the Catalog page, [2nd][0] like the TI. everything you can type, under the
   same folders as the soft keys, then the plain keys a-z, then all of it a-z
   again. the keys for it on the right and what it does at the bottom. enter
   types it */

/* catalog indexes, plus 200+k for folder k's header, 255 for the keys one and
   254 for All. ~190 rows today, All has everything a second time */
#define CAT_FOLDER  200
#define CAT_ALL     254
#define CAT_KEYS    255

static unsigned char cat_order[224];
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
static const char *cat_help(int i)  { return cat_header(i) ? NULL : cat_item(i)->help; }

/* the key column. which row of the keypad's table types what picking the item
   would, both typed on a scratch line and compaired. found on the first open,
   a byte per item */
#define NO_KEY     255
#define LINE_BYTES 48
static unsigned char cat_key[256];

static size_t line_bytes(const editor_t *e, unsigned char *buf) {
    ed_row_t *r = ed_copy_line(e);
    size_t n = r != NULL ? ed_row_save(r, buf, LINE_BYTES) : 0;

    ed_row_free(r);
    return n;
}

static void cat_find_keys(void) {
    struct { size_t n; unsigned char b[LINE_BYTES]; } *typed = malloc(keypad.count * sizeof *typed); /* ~5 KB, just while it looks */
    unsigned char want[LINE_BYTES];
    editor_t s;
    unsigned k, b;
    size_t n;

    memset(cat_key, NO_KEY, sizeof cat_key);
    if(typed == NULL || !ed_init(&s)) {
        free(typed);
        return;
    }
    /* both sides start inside a ( so a ) has something to close */
    for(b = 0; b < keypad.count; b++) {
        ed_clear(&s);
        ed_insert_char(&s, '(');
        typed[b].n = type_action(&s, keypad.binds[b].a) ? line_bytes(&s, typed[b].b) : 0;
    }

    for(k = 0; k < catalog_count && k < sizeof cat_key; k++) {
        const item_t *it = catalog[k];
        bool simplify = strcmp(it->label, "simplify") == 0;     /* that one's enter */

        if(it->kind == ITEM_SETTING || it->kind == ITEM_FOLDER)
            continue;
        ed_clear(&s);
        ed_insert_char(&s, '(');
        n = pick_into(&s, it, false) ? line_bytes(&s, want) : 0;
        for(b = 0; b < keypad.count && b < NO_KEY; b++) {
            const action_t *a = &keypad.binds[b].a;

            if((a->op == A_ITEM && names_builtin(a->s, (int)strlen(a->s)) == it)
               || (simplify && a->op == A_ENTER && keypad.binds[b].mods == 0)
               || (n > 0 && typed[b].n == n && memcmp(typed[b].b, want, n) == 0)) {
                cat_key[k] = (unsigned char)b;
                break;
            }
        }
    }
    ed_free(&s);
    free(typed);
}

static const char *cat_keys(int i) {
    static char text[24];
    const char *engine;
    unsigned b;

    if(cat_header(i))
        return NULL;
    engine = cat_item(i)->engine;
    if(engine != NULL && strcmp(engine, "0-9") == 0)
        return keypad.digits;
    if(engine != NULL && strcmp(engine, "A-Z") == 0)
        return keypad.letters;
    if((b = cat_key[cat_order[i]]) == NO_KEY)
        return NULL;
    return bind_text(keypad.binds[b].key, keypad.binds[b].mods, text, sizeof text) ? text : NULL;
}

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

/* the soft keys jump to a section. Keys gets no key, five's all there is, and
   it's the one you'd scroll through anyway */
static const char *const cat_softkeys[5] = { "Algebra", "Calculus", "Functions", "Templates", "All" };

/* first row of the k-th section, Keys not counted */
static int cat_jump(int k) {
    int i, seen = 0;

    for(i = 0; i < cat_count; i++) {
        if(!cat_header(i) || cat_order[i] == CAT_KEYS)
            continue;
        if(seen++ == k)
            return i + 1 < cat_count && !cat_header(i + 1) ? i + 1 : -1;
    }
    return -1;
}

static page_t catalog_page = {
    "Catalog", 0, cat_label, cat_keys, cat_help, cat_pick, cat_header, cat_softkeys, cat_jump
};

/* where an item sits in catalog[], folders only have the pointers */
static int cat_index(const item_t *it) {
    unsigned i;

    for(i = 0; i < catalog_count; i++)
        if(catalog[i] == it)
            return (int)i;
    return -1;
}

static bool folder_has(const folder_t *f, const item_t *it) {
    unsigned j;

    for(j = 0; j < f->count; j++)
        if(f->items[j] == it || (f->items[j]->kind == ITEM_FOLDER && folder_has(f->items[j]->sub, it)))
            return true;
    return false;
}

static bool in_a_folder(const item_t *it) {
    unsigned f;

    for(f = 0; f < FOLDER_COUNT; f++)
        if(folder_has(&folders[f], it))
            return true;
    return false;
}

/* a folder's items in menu order, subfolders opened up in place */
static void cat_add_folder(const folder_t *f) {
    unsigned j;

    for(j = 0; j < f->count && cat_count < (int)sizeof cat_order; j++) {
        int at;

        if(f->items[j]->kind == ITEM_FOLDER) {
            cat_add_folder(f->items[j]->sub);
            continue;
        }
        at = cat_index(f->items[j]);
        /* settings show no on/off here, they have tools and mode */
        if(at >= 0 && catalog[at]->kind != ITEM_SETTING)
            cat_order[cat_count++] = (unsigned char)at;
    }
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
        unsigned f;

        for(f = 0; f < FOLDER_COUNT; f++) {
            int head = cat_count;

            if(cat_count < (int)sizeof cat_order)
                cat_order[cat_count++] = (unsigned char)(CAT_FOLDER + f);
            cat_add_folder(&folders[f]);
            if(cat_count == head + 1)
                cat_count = head;   /* nothing left in it (tools), no lonely header */
        }

        cat_sorted(CAT_KEYS, false);
        cat_sorted(CAT_ALL, true);
        catalog_page.count = cat_count;
        cat_find_keys();
    }
    page_open(&catalog_page);
}

/* [math] pops up the plain characters with no key of their own, in a little grid that
   pops up at the cursor. 1-9 and 0 pick them straight away */

static const char popup_chars[] = {
    '=', CH_NE, '<', '>', CH_LE,
    CH_GE, '!', CH_INF, CH_DEG, CH_THETA
};

/* the nspire's [trig], all 12. an a in front draws as ^-1 */
static const char *const popup_trig[] = {
    "sin", "cos", "tan", "csc", "sec", "cot",
    "asin", "acos", "atan", "acsc", "asec", "acot"
};

/* the nspire's pi key */
static const char popup_constants[] = { CH_PI, CH_EULER, CH_IMAG, CH_INF, CH_THETA, CH_DEG };

/* the nspire's ?!> key */
static const char popup_marks[] = { '!', '%', CH_DEG };

/* one grid of characters, or one of functions */
typedef struct {
    const char *chars;
    const char *const *funcs;
    int count, cols, cell_w;
} popup_set_t;

static const popup_set_t math_set = { popup_chars, NULL, (int)sizeof popup_chars, 5, 22 };
static const popup_set_t trig_set = { NULL, popup_trig, (int)(sizeof popup_trig / sizeof popup_trig[0]), 6, 44 };
static const popup_set_t constants_set = { popup_constants, NULL, (int)sizeof popup_constants, 6, 22 };
static const popup_set_t marks_set = { popup_marks, NULL, (int)sizeof popup_marks, 3, 22 };

static const popup_set_t *pset = &math_set;
static int popup = -1; /* selected one, -1 = closed */
static int pop_x, pop_y, pop_cell_w = 1, pop_cell_h = 1; /* where draw_popup put it, for clicks */

/* where the cursor was last drawn, so the popup knows where to go */
static int cur_x, cur_top;

static void popup_open(const popup_set_t *set) {
    pset = set;
    popup = 0;
}

static void popup_pick(int i) {
    will_edit(false);
    if(pset->funcs != NULL)
        ed_insert(&ed, ED_FUNC, pset->funcs[i]);
    else
        ed_insert_char(&ed, pset->chars[i]);
    popup = -1;
}

/* false if it means nothing in a popup */
static bool popup_act(action_t a) {
    int n = pset->count;

    switch(a.op) {
    case A_LEFT:  popup = (popup + n - 1) % n; break;
    case A_RIGHT: popup = (popup + 1) % n; break;
    case A_UP: case A_DOWN:
        popup = (popup + pset->cols) % n;
        break;
    case A_ENTER: popup_pick(popup); break;
    case A_POPUP_PICK: popup_pick(a.n); break;
    case A_CLEAR: case A_ESC: case A_POPUP: popup = -1; break;
    case A_CHAR:
        if(a.n >= '1' && a.n <= '9' && a.n - '1' < n)
            popup_pick(a.n - '1');
        else if(a.n == '0' && n >= 10)
            popup_pick(9);
        else
            return false;
        break;
    default:
        return false;
    }
    return true;
}

/* typing (key -> editor) */

/* typed one after another these are one undo step */
static bool runs(int c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '.' || c == ','
        || c == ED_NEG || c == ED_PLUS || c == ED_SUB || c == ED_TIMES || c == ED_DIVIDE || c == CH_THETA;
}

/* an operator first thing on an empty line works on the last answer, like the
   TI. not (-), that's starting a new number */
static void ans_first(void) {
    if(ed_is_empty(&ed) && last_answer() != NULL)
        type_text(&ed, "ans");
}

static ed_row_t *clipboard;
static char *shared; /* last copy as the system clipboard has it, pasting the same keeps the editor's rows */

/* system clipboard text as a row, NULL when it isn't math. other programs write - as a minus after something, a negative before it */
static ed_row_t *row_from_text(const char *text) {
    char *engine = malloc(strlen(text) + 1), prev = '\0';
    ed_row_t *row = NULL;
    pcas_error_t err;
    pcas_ast_t *e;
    box_t *b;
    size_t n = 0;

    if(engine == NULL)
        return NULL;
    for(; *text != '\0'; text++) {
        if(*text == ' ' || *text == '\t' || *text == '\n' || *text == '\r')
            continue;
        if(isalpha((unsigned char)*text)) {
            /* a function name (( after), pi, e and i stay lowercase, any other letter is a variable, capital in the engine */
            const char *end = text;
            bool lower;

            while(isalpha((unsigned char)*end))
                end++;
            lower = *end == '(' || (end - text == 2 && strncmp(text, "pi", 2) == 0) || (end - text == 1 && (*text == 'e' || *text == 'i'));
            for(; text < end; text++)
                engine[n++] = lower ? *text : (char)toupper((unsigned char)*text);
            text--;
            prev = 'a';
            continue;
        }
        engine[n++] = *text == '-' && (isalnum((unsigned char)prev) || prev == ')' || prev == '.') ? '_' : *text;
        prev = *text;
    }
    engine[n] = '\0';
    e = n > 0 ? parse((const uint8_t *)engine, n, str_table, &err) : NULL;
    free(engine);
    if(e == NULL || err != E_SUCCESS) {
        ast_Cleanup(e);
        return NULL;
    }
    b = layout_build(e, &draw_font);
    ast_Cleanup(e);
    if(b == NULL)
        return NULL;
    layout_finish(b, &draw_font);
    row = ed_from_box(b);
    layout_free(b);
    return row;
}

/* "expand((x+1)^9)" from another program, args read as math. false and nothing typed if it isn't a command */
#define PASTE_ARGS_MAX 6
static bool paste_command(const char *text) {
    char name[16];
    ed_row_t *rows[PASTE_ARGS_MAX] = { NULL };
    const char *p = text, *start, *end;
    size_t len;
    int n = 0, depth = 0, i;
    bool ok = false;

    while(isspace((unsigned char)*p))
        p++;
    for(len = 0; isalpha((unsigned char)p[len]) && len < sizeof name - 1; len++)
        name[len] = (char)tolower((unsigned char)p[len]);
    name[len] = '\0';
    if(len == 0 || p[len] != '(' || !ed_is_command(name))
        return false;
    start = p + len + 1;
    end = start + strlen(start);
    while(end > start && isspace((unsigned char)end[-1]))
        end--;
    if(end == start || end[-1] != ')')
        return false;
    end--; /* the last ) */
    for(p = start; p <= end && n < PASTE_ARGS_MAX; p++) { /* split at the commas outside brackets */
        if(p < end && *p == '(')
            depth++;
        else if(p < end && *p == ')')
            depth--;
        if(depth < 0)
            goto done;
        if(p == end || (*p == ',' && depth == 0)) {
            char *arg = malloc((size_t)(p - start) + 1);

            if(arg == NULL)
                goto done;
            memcpy(arg, start, (size_t)(p - start));
            arg[p - start] = '\0';
            rows[n] = row_from_text(arg);
            free(arg);
            if(rows[n++] == NULL)
                goto done;
            start = p + 1;
        }
    }
    if(depth != 0 || p <= end)
        goto done;
    will_edit(false);
    type_text(&ed, name);
    ed_insert_char(&ed, '(');
    for(i = 0; i < n; i++) {
        if(i > 0)
            ed_insert_char(&ed, ',');
        ed_paste(&ed, rows[i]);
    }
    ed_insert_char(&ed, ')');
    ok = true;
done:
    for(i = 0; i < PASTE_ARGS_MAX; i++)
        ed_row_free(rows[i]);
    return ok;
}

/* copied row out to other programs, engine text with a plain minus */
static void clipboard_share(const ed_row_t *row) {
    char *text = ed_row_to_engine(row), *c;

    free(shared);
    shared = text;
    if(text == NULL)
        return;
    for(c = text; *c != '\0'; c++)
        if(*c == '_')
            *c = '-';
    clipboard_put(text);
}

static const popup_set_t *const popup_sets[] = { &math_set, &trig_set, &constants_set, &marks_set };

/* what a key does to the line. false to quit */
static bool edit_act(action_t a) {
    switch(a.op) {
    case A_CHAR: case A_SELECT_LEFT: case A_SELECT_RIGHT: case A_NEXT_FORM: case A_DECIMAL:
    case A_CUT: case A_COPY: case A_PASTE: case A_UNDO: case A_CLEAR_LINE: case A_ITEM: case A_SELECT_ALL: case A_POINT: case A_POINT_DRAG:
        break;  /* they do their own */
    case A_FUNC: case A_TEMPLATE: case A_POWER: case A_TYPE:
    case A_BACKSPACE: case A_DELETE: case A_CLEAR:
        will_edit(false);
        break;
    default:
        typing_run = false;     /* not an edit (enter saves its own step) */
        break;
    }

    switch(a.op) {
    case A_QUIT:   return false;
    case A_CHAR:
        will_edit(runs(a.n));
        if(a.n == ED_PLUS || a.n == ED_SUB || a.n == ED_TIMES || a.n == ED_DIVIDE)
            ans_first();
        if(a.n == ED_DIVIDE && ed_has_selection(&ed))   /* on top of a fraction (nspire) */
            ed_insert(&ed, ED_FRAC, NULL);
        else
            type_action(&ed, a);     /* sto-> is value -> name, see names.c */
        break;
    case A_TEMPLATE:
        if(a.n == ED_POW)
            ans_first();
        type_action(&ed, a);
        break;
    case A_FUNC: case A_POWER: case A_TYPE:
        type_action(&ed, a);
        break;
    case A_ITEM: {
        const item_t *it = names_builtin(a.s, (int)strlen(a.s));

        if(it != NULL && pick(it, true)) {
            will_edit(false);
            pick(it, false);
        }
        break;
    }

    /* with something selected, left/right just drop it on that side (like any
       text box) */
    case A_LEFT:
        if(ed_has_selection(&ed))
            ed_deselect(&ed, true);
        else
            ed_left(&ed);
        break;
    case A_RIGHT:
        if(ed_has_selection(&ed))
            ed_deselect(&ed, false);
        else
            ed_right(&ed);
        break;
    case A_UP:
        if(!ed_up(&ed))
            history_open();
        break;
    case A_DOWN:   ed_down(&ed); break;
    case A_SELECT_LEFT:  typing_run = false; ed_select_left(&ed); break;
    case A_SELECT_RIGHT: typing_run = false; ed_select_right(&ed); break;
    case A_LINE_START:   ed_home(&ed); break;
    case A_LINE_END:     ed_end(&ed); break;

    case A_BACKSPACE: case A_CLEAR: ed_backspace(&ed); break;
    case A_DELETE: ed_delete(&ed); break;
    case A_CLEAR_LINE:
        /* on an empty line it clears the screen instead. nothing gets deleted,
           the history just scrolls up out of view and up brings it back */
        if(a.n && ed_is_empty(&ed)) {
            cleared = history_len;
            break;
        }
        will_edit(false);
        ed_clear(&ed);
        break;
    case A_CUT:
    case A_COPY: {
        ed_row_t *copy = ed_copy(&ed);

        if(copy == NULL) {
            status = ed_has_selection(&ed) ? "out of memory" : "nothing selected ([alpha] + arrows)";
            break;
        }
        ed_row_free(clipboard);
        clipboard = copy;
        clipboard_share(copy);
        if(a.op == A_CUT) {
            will_edit(false);
            ed_erase_selection(&ed);
        }
        break;
    }
    case A_PASTE: {
        char *outside = clipboard_get();
        bool theirs = outside != NULL && (shared == NULL || strcmp(outside, shared) != 0); /* copied somewhere else since */
        ed_row_t *row = theirs ? row_from_text(outside) : NULL;

        if(theirs && row == NULL) {
            if(!paste_command(outside))
                status = "can't read that as math";
        } else if(row == NULL && clipboard == NULL) {
            status = "nothing copied yet";
        } else {
            will_edit(false);
            ed_paste(&ed, row != NULL ? row : clipboard);
        }
        ed_row_free(row);
        free(outside);
        break;
    }
    case A_UNDO:
        typing_run = false;
        if(!ed_undo(&ed))
            status = "nothing to undo";
        break;
    case A_SELECT_ALL:
        typing_run = false;
        ed_select_all(&ed);
        break;
    case A_POINT: case A_POINT_DRAG:
        typing_run = false;
        ed_click(&ed, &draw_font, (a.n & 0xFFF) - in_left, (a.n >> 12) - in_base, a.op == A_POINT_DRAG);
        break;

    case A_ENTER:  evaluate(); break;
    case A_NEXT_FORM: form_set((form_t)((form + 1) % FORM_COUNT)); break;
    case A_DECIMAL: decimal_once(); break;

    case A_SOFT:   menu_open(a.n); break;
    case A_MENU:   menu_open_top(); break;
    case A_POPUP:  popup_open(popup_sets[a.n]); break;
    case A_CATALOG: catalog_open(); break;
    case A_VARS:   vars_open(); break;  /* the stored names, like the TI's vars */
    case A_SETTINGS: mode_open(); break;

    default:
        break;
    }
    return true;
}

/* two taps of [2nd] or [alpha] this close together lock it */
#define DOUBLE_TAP (CLOCKS_PER_SEC * 2 / 5)

/* [2nd] / [alpha] / [on] (ctrl / shift on the nspire), on for the next key. seperate
   so any of them can be on together ([2nd][alpha] + a letter is a capital). a quick
   second tap locks it whatever the first one did (it may have just turned it off),
   a tap while locked unlocks */
static void tap(unsigned bit, bool again) {
    if(again) {
        mods |= bit;
        mods_lock |= bit;
    } else if(mods_lock & bit) {
        mods &= ~bit;
        mods_lock &= ~bit;
    } else {
        mods ^= bit;
    }
}

/* the last modifier tapped, 0 after any other key. a double tap is two
   taps with nothing in between (alpha a alpha b isn't one) */
static unsigned last;
static clock_t last_time;

static void modifier(unsigned bit) {
    /* modifier? i hardly know 'er bahahwahaa */
    bool again = bit == last && clock() - last_time < DOUBLE_TAP;

    last = bit;
    last_time = clock();
    tap(bit, again);

    /* locking ate the double tap, so a third quick tap starts a fresh one */
    if(again)
        last = 0;
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

/* what enter and copy take, just the selected part when browsing, else the whole thing */
static ed_row_t *hist_take(void) {
    if(browsing)
        return ed_has_selection(&view) ? ed_copy(&view) : ed_copy_line(&view);
    return ed_row_copy(hist_row());
}

/* keys while the history's highlighted. true if the key was used up, else it
   goes back to the input line and gets handled there like normal */
static bool history_act(action_t a) {
    if(a.op == A_COPY) {
        ed_row_t *copy = hist_take();

        if(copy != NULL) {
            ed_row_free(clipboard);
            clipboard = copy;
        }
        return true;
    }

    if(browsing) {
        switch(a.op) {
        case A_SELECT_LEFT:  ed_select_left(&view); return true;
        case A_SELECT_RIGHT: ed_select_right(&view); return true;
        case A_LEFT:
            if(ed_has_selection(&view))
                ed_deselect(&view, true);
            else
                ed_left(&view);
            return true;
        case A_RIGHT:
            if(ed_has_selection(&view))
                ed_deselect(&view, false);
            else
                ed_right(&view);
            return true;
        case A_UP:
            if(!ed_up(&view)) {
                browsing = false;
                hist_up();
            }
            return true;
        case A_DOWN:
            if(!ed_down(&view)) {
                browsing = false;
                hist_down();
            }
            return true;
        case A_CLEAR: case A_ESC:
            browsing = false;   /* back to the whole thing highlighted */
            return true;
        default:
            break;
        }
    } else {
        bool left = a.op == A_LEFT || a.op == A_SELECT_LEFT;

        switch(a.op) {
        case A_UP:    hist_up(); return true;
        case A_DOWN:  hist_down(); return true;
        case A_LEFT: case A_SELECT_LEFT:
        case A_RIGHT: case A_SELECT_RIGHT:
            /* inside it, the cursor coming in from that side */
            if(hist_row() != NULL && ed_load(&view, hist_row(), left))
                browsing = true;
            return true;
        case A_CLEAR: case A_ESC:
            hist = -1;
            return true;
        default:
            break;
        }
    }

    if(a.op == A_ENTER) {
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

    menu_from = a.op == A_SOFT || a.op == A_MENU ? hist : -1;
    hist = -1;
    browsing = false;
    return false;
}

/* the mouse as an action: soft keys, an open menu's or popup's items, the input line. nothing for the rest yet */
static action_t pointer_action(keypress_t p) {
    action_t a = { A_NONE, 0, NULL };
    bool on_keys = p.y >= areas.keys.y && p.y < areas.keys.y + areas.keys.h;
    int row;

    if(menu >= 0 && p.key == K_POINT_DOWN) {
        row = (p.y - menu_y - 2) / (menu_row_h > 0 ? menu_row_h : 1);
        if(p.x >= menu_x && p.x < menu_x + menu_w && p.y >= menu_y + 2 && row < menu_rows) {
            row += menu_top; /* its key: 1 to 9, then a to z */
            a.op = A_CHAR;
            a.n = row < 9 ? '1' + row : 'a' + (row - 9);
            return a;
        }
        if(!on_keys) {
            a.op = A_ESC; /* outside it */
            return a;
        }
    }
    if(popup >= 0 && p.key == K_POINT_DOWN) {
        int col = (p.x - pop_x) / pop_cell_w, line = (p.y - pop_y) / pop_cell_h;
        int i = line * pset->cols + col;

        a.op = A_ESC; /* outside it */
        if(p.x >= pop_x && p.y >= pop_y && col < pset->cols && i < pset->count) {
            a.op = A_POPUP_PICK;
            a.n = i;
        }
        return a;
    }

    if(p.key == K_POINT_DOWN && on_keys && !NSPIRE) {
        a.op = A_SOFT;
        a.n = p.x * FOLDER_COUNT / (areas.keys.w > 0 ? areas.keys.w : 1);
        if(a.n >= FOLDER_COUNT)
            a.n = FOLDER_COUNT - 1;
    } else if(p.key == K_POINT_DOWN && p.y < in_top) {
        for(row = 0; row < hit_n; row++)
            if(p.y >= hits[row].y0 && p.y < hits[row].y1) {
                a.op = A_HIST_POINT;
                a.n = hits[row].id;
                break;
            }
    } else if(p.y >= in_top && p.y < areas.keys.y) {
        a.op = p.key == K_POINT_DRAG || (p.held & M_ALPHA) != 0 ? A_POINT_DRAG : A_POINT;
        a.n = p.x | (p.y << 12);
    }
    return a;
}

static bool handle(keypress_t p) {
    action_t a, plain;

    status = NULL;
    if(menu < 0)
        menu_from = -1; /* [2nd][y=] and such open nothing */

    if(p.key == K_QUIT)
        return false;

    /* every modifier that's on (tapped or held down) goes with this key. plain is
       the key on its own, for the popups, menus and pages, where 2nd and such
       don't matter */
    if(p.key == K_POINT_DOWN || p.key == K_POINT_DRAG)
        a = plain = pointer_action(p);
    else {
        a = bind_key(p.key, mods | p.held);
        plain = bind_key(p.key, 0);
    }

    if(popup >= 0) {
        if(!popup_act(a))
            popup_act(plain);
        return true;
    }

    /* [2nd][alpha][on] is lights out. both are already on by the time the on tap comes */
    if(a.op == A_SCREEN_OFF) {
        mods = mods_lock = 0;
        keys_sleep();
        return true;
    }
    if(plain.op == A_MOD) {
        modifier((unsigned)plain.n);
        return true;
    }
    last = 0;
    mods = mods_lock;   /* the tapped ones turn off unless locked */

    if(menu >= 0) {
        /* locked alpha leaves 1-9 as numbers in here, a tapped one still makes 7-9 o p q */
        if((mods_lock & M_ALPHA) && plain.op == A_CHAR && plain.n >= '1' && plain.n <= '9')
            a = plain;
        if(!menu_act(a))
            menu_act(plain);
        return true;
    }
    if(page_is_open()) {
        if(a.op == A_QUIT)  /* [2nd][mode] is quit, out of the page here */
            page_close();
        else if(a.op == A_MENU && page_softkeys() != NULL)
            menu_open_soft(page_softkeys());
        else if(!page_act(a))
            page_act(plain);
        return true;
    }
    if(work_is_open()) {
        if(a.op == A_QUIT)
            work_close();
        else if(a.op == A_MENU && work_softkeys() != NULL)
            menu_open_soft(work_softkeys());
        else if(!work_act(a))
            work_act(plain);
        return true;
    }

    /* before the history so it works with an answer highlighted too */
    if(a.op == A_BRIGHTNESS) {
        setting_brightness += a.n;
        if(setting_brightness < 0) setting_brightness = 0;
        if(setting_brightness > 7) setting_brightness = 7;
        screen_brightness(setting_brightness);
        settings_save();
        return true;
    }

    if(a.op == A_HIST_POINT) {
        int id = a.n / 2;
        bool want_out;

        if(id < 0 || id >= history_len)
            return true;
        want_out = a.n % 2 == 1 && history[id].out_row != NULL;
        if(hist == id && hist_out == want_out && !browsing) {
            a.op = A_ENTER; /* the one that's lit goes in the line */
            return history_act(a);
        }
        hist = id;
        hist_out = want_out;
        browsing = false;
        return true;
    }

    if(hist >= 0 && history_act(a))
        return true;
    return edit_act(a);
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

/* the nspire's main screen gets the bar's rows too */
static int body_bottom(void) {
    return NSPIRE ? areas.keys.y + areas.keys.h : areas.body.y + areas.body.h;
}

static void draw_bar(void) {
    const char *names[FOLDER_COUNT];
    int i;

    for(i = 0; i < FOLDER_COUNT; i++)
        names[i] = folders[i].name;
    draw_softkeys(&areas.keys, names, menu, 0);
}

static void draw_menu(void) {
    const font_t *font = draw_label_font();
    const folder_t *f = menu_in;
    int row_h = font->ascent + font->descent + 2;
    int bottom = body_bottom() - 1;
    int rows = (int)f->count, max_rows;
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
    max_rows = (bottom - areas.body.y - 4) / row_h;

    if(rows > max_rows)
        rows = max_rows;
    if(menu_sel < menu_top)
        menu_top = menu_sel;
    if(menu_sel >= menu_top + rows)
        menu_top = menu_sel - rows + 1;

    if(NSPIRE) {   /* down from the top, a step right for each level like the nspire's own */
        x = areas.body.x + 2 + menu_depth * 14;
        top = areas.body.y + 1;
        bottom = top + rows * row_h + 2;
    } else {
        x = areas.keys.x + menu * (areas.keys.w / FOLDER_COUNT); /* over its key */
        top = bottom - rows * row_h - 2;
    }
    if(x + width > areas.body.x + areas.body.w - 1)
        x = areas.body.x + areas.body.w - 1 - width;

    menu_x = x;
    menu_y = top;
    menu_w = width;
    menu_row_h = row_h;
    menu_rows = rows;
    screen_color(COL_BG);
    screen_fill(x, top, width, bottom - top + 1);
    screen_color(COL_DIM);
    screen_hline(x, top, width);
    if(NSPIRE) /* nothing under it to close it off */
        screen_hline(x, bottom, width);
    screen_vline(x, top, bottom - top + 1);
    screen_vline(x + width - 1, top, bottom - top + 1);

    for(i = 0; i < rows; i++) {
        int n = menu_top + i, y = top + 2 + i * row_h;
        int text_y = y + (row_h - 2 - font->ascent - font->descent) / 2;
        const item_t *it = f->items[n];
        bool can = it->kind == ITEM_FOLDER || it->kind == ITEM_SOFTKEY || pick(it, true);
        char label[48];

        if(it->kind == ITEM_SETTING && !is_action(it))
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

        if(n < 9 + 26) {
            char num[3] = { (char)(n < 9 ? '1' + n : 'A' + (n - 9)), ':', '\0' };
            draw_text(x + 3, text_y, num, font);
        }
        if(it->kind == ITEM_FOLDER)
            draw_text(x + width - 3 - draw_text_width(">", font), text_y, ">", font);
        if(n < menu_box_count && menu_boxes[n] != NULL) {
            const box_t *b = menu_boxes[n];

            draw_box(b, x + 18, y + (row_h - 2 - b->ascent - b->descent) / 2 + b->ascent);
        } else {
            draw_text(x + 18, text_y, label, font);
        }
    }
}

/* the [math] or [trig] grid, just above the cursor, each cell numbered in the corner */
static void draw_popup(void) {
    const font_t *font = draw_font_for(draw_font.base), *tiny = draw_font_for(0);
    int cols = pset->cols, cell_w = pset->cell_w, cell_h = font->ascent + font->descent + 6;
    int rows = (pset->count + cols - 1) / cols;
    int w = cols * cell_w + 1, h = rows * cell_h + 1;
    int x = cur_x - w / 2, y = cur_top - h - 2, i;

    if(x < areas.body.x + 1)
        x = areas.body.x + 1;
    if(x + w > areas.body.x + areas.body.w - 1)
        x = areas.body.x + areas.body.w - 1 - w;
    if(y < areas.body.y)
        y = areas.body.y;
    pop_x = x;
    pop_y = y;
    pop_cell_w = cell_w;
    pop_cell_h = cell_h;

    screen_color(COL_BG);
    screen_fill(x, y, w, h);
    screen_color(COL_DIM);
    screen_hline(x, y, w);
    screen_hline(x, y + h - 1, w);
    screen_vline(x, y, h);
    screen_vline(x + w - 1, y, h);

    for(i = 0; i < pset->count; i++) {
        int cx = x + (i % cols) * cell_w, cy = y + (i / cols) * cell_h, lw, lx;
        char s[2] = { 0, '\0' }, num[2] = { (char)(i < 9 ? '1' + i : '0'), '\0' }, minus_one[3] = { CH_MINUS, '1', '\0' };
        const char *label = s;
        bool inv = false;

        if(pset->funcs != NULL) {
            label = pset->funcs[i];
            inv = label[0] == 'a';  /* asin as sin^-1 */
            label += inv;
        } else {
            s[0] = pset->chars[i];
        }
        lw = draw_text_width(label, font) + (inv ? draw_text_width(minus_one, tiny) : 0);
        lx = cx + (cell_w - lw) / 2 + 2;

        if(i == popup) {
            screen_color(COL_ACCENT);
            screen_fill(cx + 1, cy + 1, cell_w - 1, cell_h - 1);
        }
        screen_color(i == popup ? COL_BG : COL_DIM);
        if(i < 10)  /* 1-9 and 0 pick, past that it's the arrows */
            draw_text(cx + 2, cy + 1, num, tiny);
        screen_color(i == popup ? COL_BG : COL_FG);
        draw_text(lx, cy + 4, label, font);
        if(inv)
            draw_text(lx + draw_text_width(label, font), cy + 1, minus_one, tiny);
    }
}

/* the status bar */

/* along the top: a padlock each for 2nd / alpha / ctl, the working dots,
   the battery */

#define BATTERY_W   13
#define BATTERY_READ (CLOCKS_PER_SEC * 10)     /* the level is read this often (reading every frame was slow) */
#define LOCK_SLOT   13 /* per padlock, fixed so one turning on doesn't shove the others along */
#define BAR_LEFT    (areas.top.x + MARGIN)
#define BAR_RIGHT   (areas.top.x + areas.top.w - MARGIN)
#define BAR_BEFORE_BATTERY (BAR_RIGHT - BATTERY_W - 6) /* right edge of the form name and the busy dot */

static int battery = -1;
static clock_t battery_next;

/* 3x5 letters punched out of the padlocks, top bit on the left: 2, A, C, S */
static const unsigned char lock_letters[4][5] = {
    { 6, 1, 2, 4, 7 },
    { 2, 5, 7, 5, 5 },
    { 3, 4, 4, 4, 3 },
    { 3, 4, 2, 1, 6 }
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
    int h = areas.top.h, top = areas.top.y, y = top + (h - 1 - 11) / 2;
    int left = BAR_LEFT, right = BAR_RIGHT, i;

    if(battery < 0 || clock() >= battery_next) {
        battery = power_battery();
        battery_next = clock() + BATTERY_READ;
    }

    screen_color(COL_BG);
    screen_fill(areas.top.x, top, areas.top.w, h);
    screen_color(COL_DIM);
    screen_hline(left, top + h - 1, right - left);
    draw_battery(right - BATTERY_W, top + (h - 1 - 7) / 2, battery);

    for(i = 0; i < 3; i++) {
        unsigned m = keypad.shown[i];
        int letter = m == M_2ND ? 0 : m == M_ALPHA ? 1 : m == M_SHIFT ? 3 : 2;    /* [on] and ctrl are both C */

        if(m & mods)
            draw_padlock(left + i * LOCK_SLOT, y, letter, (m & mods_lock) != 0);
    }

    /* the form, when it isn't the normal one, over by the battery */
    if(form != FORM_HAND) {
        const font_t *small = draw_label_font();

        screen_color(COL_DIM);
        draw_text(BAR_BEFORE_BATTERY - draw_text_width(form_names[form], small), top + 1,
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

static void add_hit(int y0, int y1, int id) {
    if(hit_n < HITS_MAX) {
        hits[hit_n].y0 = y0;
        hits[hit_n].y1 = y1;
        hits[hit_n++].id = id;
    }
}

static void draw_screen(void) {
    const font_t *small = draw_label_font();
    box_t *in = ed_layout(&ed, &draw_font, true);
    int left = areas.body.x + MARGIN, right = areas.body.x + areas.body.w - MARGIN;
    int line_y, y, i, avail = right - left, first;
    int top = areas.body.y, bottom = body_bottom();

    screen_clear();

    if(page_is_open() || work_is_open()) {
        layout_free(in);
        if(page_is_open())
            page_draw();
        else
            work_draw();
        if(menu >= 0)
            draw_menu();
        draw_top_bar();
        screen_show();
        return;
    }

    if(!NSPIRE)
        draw_bar();

    /* input line, just above the soft keys. scrolls sideways to keep the cursor in view. */
    if(in != NULL) {
        int x = left, base = bottom - 3 - in->descent;

        if(in->width > avail) {
            int cx = cursor_x(in, 0), shift = cx - avail * 3 / 4;

            if(shift > in->width - avail)
                shift = in->width - avail;
            if(shift > 0)
                x -= shift;
        }
        screen_color(COL_FG);
        draw_box(in, x, base);
        in_left = x;
        in_base = base;
        cur_x = cursor_x(in, x);
        cur_top = base - in->ascent;
        line_y = base - in->ascent - 3;
        layout_free(in);
    } else {
        line_y = bottom - 20;
    }

    screen_color(COL_DIM);
    screen_hline(left, line_y, right - left);
    in_top = line_y;

    /* messages at the right of the line */
    if(status != NULL) {
        screen_color(COL_DIM);
        draw_text(right - draw_text_width(status, small),
                  line_y - small->ascent - small->descent - 1, status, small);
    }

    hit_n = 0;
    /* history, newest at the bottom, going up to the status bar (drawn over
       the top of the last one). scrolled so the highlighted one fits */
    i = history_len - 1;
    if(hist >= 0) {
        int below = 0, k;

        for(k = hist; k <= i; k++)
            below += entry_h(&history[k]);
        for(; i > hist && below > line_y - GAP - top; i--)
            below -= entry_h(&history[i]);
    }
    /* the status line takes the bottom row, else it lands on the last message */
    y = line_y - GAP - (status != NULL ? small->ascent + small->descent + 2 : 0);
    /* the cleared ones stay hidden unless you've gone up into them */
    first = hist >= 0 && hist < cleared ? 0 : cleared;
    for(; i >= first && y > top; i--) {
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
            int base = y - out->descent, x = right - out->width;

            if(i == hist && hist_out)
                highlight(x, base, out);
            screen_color(COL_FG);
            draw_box(out, x, base);
            add_hit(y - out->ascent - out->descent - 2, y, i * 2 + 1);
            if(h->ms >= 100) {
                char t[32];

                snprintf(t, sizeof t, "%lu.%lus", h->ms / 1000, h->ms % 1000 / 100);
                screen_color(COL_DIM);
                draw_text(left, base - small->ascent, t, small);
            }
            y -= out->ascent + out->descent + 2;
        } else {
            screen_color(COL_DIM);
            draw_text(right - draw_text_width(h->message, small),
                      y - small->ascent - small->descent, h->message, small);
            y -= small->ascent + small->descent + 2;
        }

        if(inb != NULL) {
            if(i == hist && !hist_out)
                highlight(left, y - inb->descent, inb);
            screen_color(COL_FG);
            draw_box(inb, left, y - inb->descent);
            add_hit(y - inb->ascent - inb->descent - GAP, y, i * 2);
            y -= inb->ascent + inb->descent + GAP;
        }
        layout_free(browse);

        screen_color(COL_DIM);
        screen_hline(left + 20, y + 1, right - left - 40);
        y -= GAP;
    }
    draw_top_bar();

    if(menu >= 0)
        draw_menu();
    if(popup >= 0)
        draw_popup();

    screen_show();
}

#define BUSY_HINT 5 /* s */

static clock_t busy_next;
static unsigned busy_frame;
static clock_t busy_started;
static unsigned long busy_shown; /* seconds the hint says, 0 = not up yet */

/* a dot going round in the status bar (left of the battery) while the engine
   runs, so a slow simplify doesn't look frozen. drawn straight onto the screen,
   swapping buffers made the cursor flash */
static void draw_working(void) {
    /* 2x2 dots round an 8x8 box, clockwise from the top */
    static const unsigned char dots[8][2] = {
        { 3, 0 }, { 5, 1 }, { 6, 3 }, { 5, 5 }, { 3, 6 }, { 1, 5 }, { 0, 3 }, { 1, 1 }
    };
    int x = BAR_BEFORE_BATTERY - 8, y = areas.top.y + (areas.top.h - 1 - 8) / 2;
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

/* "12s, [on]+[mode] stops it" after the padlocks. no guess at the total, 1/x+1/(x+1)=1 is one short line and ~17 s on the calc */
static void draw_busy_hint(unsigned long s) {
    const font_t *small = draw_label_font();
    int x = BAR_LEFT + 3 * LOCK_SLOT + 4;
    char t[32];

    snprintf(t, sizeof t, "%lus, %s stops it", s, keypad.cancel);
    screen_direct(true);
    screen_color(COL_BG);
    screen_fill(x, areas.top.y, draw_text_width(t, small), areas.top.h - 1);
    screen_color(COL_DIM);
    draw_text(x, areas.top.y + 1, t, small);
    screen_direct(false);
}

/* what the keys looked like at the last poll, for edges */
static clock_t busy_keys_next;
static keycode_t busy_key;
static unsigned busy_held;
static bool busy_chord;

/* pcas_poll, the engine calls this constantly, so check the clock and only read
   the keys 40 times a second and draw a frame 10 times a second. 40 Hz so a quick
   tap doesn't fall between two polls */
static bool busy_poll(void) {
    keypress_t p;
    action_t a;
    unsigned long s;

    if(clock() < busy_keys_next)
        return false;
    busy_keys_next = clock() + CLOCKS_PER_SEC / 40;
    if(clock() >= busy_next) {
        busy_next = clock() + CLOCKS_PER_SEC / 10;
        busy_frame++;
        draw_working();
        s = (unsigned long)((clock() - busy_started) / CLOCKS_PER_SEC);
        if(s >= BUSY_HINT && s != busy_shown) {
            busy_shown = s;
            draw_busy_hint(s);
        }
    }

    /* a modifier tapped while it works still counts for the next key (keys_peek marks
       whatever's down as held). the ones peek sees as keys (2nd, alpha) on the press,
       the held ones ([on], ctrl, shift) on the release and only if nothing came with
       them, since [on]+[mode] is the cancel */
    p = keys_peek();
    a = bind_key(p.key, 0);
    if(p.key != busy_key && a.op == A_MOD && !p.held)
        modifier((unsigned)a.n);
    if(p.held && !busy_held)
        busy_chord = false;
    if(p.held && p.key != K_NONE)
        busy_chord = true;
    if((busy_held & ~p.held) && !busy_chord)
        modifier(busy_held & ~p.held);
    busy_key = p.key;
    busy_held = p.held;

    a = bind_key(p.key, p.held);
    return a.op == A_CANCEL || a.op == A_ESC || a.op == A_QUIT;
}

static void busy_start(void) {
    pcas_cancelled = false;
    pcas_oom = false;
    pcas_work = 0;
    busy_frame = 0;
    busy_started = clock();
    busy_shown = 0;
    busy_next = busy_started + CLOCKS_PER_SEC / 10;
    busy_keys_next = busy_next;
    busy_key = K_NONE;
    busy_held = 0;
    busy_chord = false;
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
    ed_ans = last_answer;
    history_load();
    screen_begin();
    settings_load();    /* after, a first run starts from the OS brightness screen_begin read */
    keys_begin();
    areas_init();
#ifdef SCREEN_RESIZABLE
    screen_resized = areas_init; /* the window got a new size */
#endif
    draw_screen();
    pcas_poll = busy_poll;
    keys_on_sleep = on_sleep;

    for(;;) {
        keypress_t p = keys_wait();

        if(bind_key(p.key, mods | p.held).op == A_ENTER && menu < 0 && !ed_is_empty(&ed))
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
