#include "applied.h"

#include "app.h"
#include "args.h"
#include "calc1.h"
#include "geom.h"
#include "engine/cas/steps.h"

#include <math.h>
#include <string.h>
#include <time.h>

#include "engine/ast.h"
#include "engine/cas/cas.h"
#include "render/draw.h"

/* a first course's uses of the integral, *dih*rivative and limit, cut at the commas like calc1's */

#define ARGS_MAX 6

enum { P_AVERAGE, P_AREA, P_MVT, P_CONTINUOUS, P_VOLUME, P_COUNT };

static const struct {
    const char *name, *usage;
    int least, most;
} cmds[P_COUNT] = {
    { "average", "average(f, a, b, x)", 3, 4 },
    { "area", "area(f, g, a, b, x)", 2, 5 },
    { "mvt", "mvt(f, a, b, x)", 3, 4 },
    { "continuous", "continuous(f, a, x)", 2, 3 },
    { "volume", "volume(f, g, a, b, y=c, x)", 5, 6 },
};

static bool same(double a, double b) {
    return fabs(a - b) <= 1e-9 * (fabs(b) > 1 ? fabs(b) : 1);
}

/* is there an op node anywere in e, an integral the engine couldn't do or a limit it couldn't finish */
static bool has_op(pcas_ast_t *e, OperatorType op) {
    pcas_ast_t *found[STEPS_FOUND_MAX];

    return e != NULL && steps_find(e, op, found, 0) > 0;
}

/* integral(f, var, a, b) not worked out. f is the new one's to keep, a and b get copied */
static pcas_ast_t *integral_of(pcas_ast_t *f, Symbol var, pcas_ast_t *a, pcas_ast_t *b) {
    pcas_ast_t *e = ast_MakeOperator(OP_INTEGRAL);

    ast_ChildAppend(e, f);
    ast_ChildAppend(e, ast_MakeSymbol((char)var));
    ast_ChildAppend(e, ast_Copy(a));
    ast_ChildAppend(e, ast_Copy(b));
    return e;
}

/* var -> a from one side (-1 or 1), worked out */
static pcas_ast_t *limit_of(pcas_ast_t *f, Symbol var, pcas_ast_t *a, int side) {
    pcas_ast_t *e = ast_MakeOperator(OP_LIMIT);

    ast_ChildAppend(e, ast_Copy(f));
    ast_ChildAppend(e, ast_MakeSymbol((char)var));
    ast_ChildAppend(e, ast_Copy(a));
    ast_ChildAppend(e, ast_MakeInt(side));
    simplify(e, SIMP_ALL);
    return e;
}

static bool between(double v, double lo, double hi) {
    return v - lo > 1e-9 && hi - v > 1e-9;
}

/* the roots of d from small to large. with bounded, only the ones stricly between lo and hi, and a trig family (one with an n in it) tried for n from -20 to 20.
   how many, or SOLVE_ALL / SOLVE_UNKNOWN */
static int roots_between(pcas_ast_t *d, Symbol var, bool bounded, double lo, double hi, pcas_ast_t **roots) {
    pcas_ast_t *found[SOLVE_MAX] = { NULL }, *g, *k;
    int n = solve_ex(d, var, found, false), m = 0, i, step;
    bool usable = true;
    double v;

    for(i = 0; i < n; i++) {
        if(approx_double(found[i], &v)) {
            if(m < SOLVE_MAX && (!bounded || between(v, lo, hi)))
                roots[m++] = found[i];
            else
                ast_Cleanup(found[i]);
        } else if(bounded && has_symbol(found[i], SYM_N)) {
            for(step = -20; step <= 20; step++) {
                k = ast_MakeInt(step);
                g = value_at(found[i], SYM_N, k);
                ast_Cleanup(k);
                if(m < SOLVE_MAX && approx_double(g, &v) && between(v, lo, hi))
                    roots[m++] = g;
                else
                    ast_Cleanup(g);
            }
            ast_Cleanup(found[i]);
        } else {
            usable = false;
            ast_Cleanup(found[i]);
        }
    }
    if(!usable) {
        for(i = 0; i < m; i++) {
            ast_Cleanup(roots[i]);
            roots[i] = NULL;
        }
        return SOLVE_UNKNOWN;
    }
    sort_roots(roots, m);
    return n < 0 ? n : m;
}

/* e with its integrals worked out, as label = e. e stays the caller's */
static box_t *integral_answer(const char *label, pcas_ast_t *e, const char **message) {
    box_t *out = NULL;

    simplify(e, SIMP_ALL);
    if(has_op(e, OP_INTEGRAL)) {
        *message = "can't integrate that";
    } else if(!pcas_cancelled) {
        out = labelled(label, value_box(e));
        layout_finish(out, &draw_font);
    }
    return out;
}

static box_t *average(pcas_ast_t *f, pcas_ast_t *a, pcas_ast_t *b, Symbol var, const char **message) {
    pcas_ast_t *e;
    box_t *out;

    if(ast_Compare(a, b)) {
        *message = "a and b are the same point";
        return NULL;
    }
    e = ast_MakeBinary(OP_DIV, integral_of(ast_Copy(f), var, a, b), minus(ast_Copy(b), ast_Copy(a)));
    out = integral_answer("average =", e, message);
    ast_Cleanup(e);
    return out;
}

static pcas_ast_t *midpoint(pcas_ast_t *p, pcas_ast_t *q) {
    return ast_MakeBinary(OP_DIV, ast_MakeBinary(OP_ADD, ast_Copy(p), ast_Copy(q)), ast_MakeInt(2));
}

/* integral of |d| a gap at a time, the sign from d in the gap's middle. NULL and a message when that isn't a number */
static pcas_ast_t *area_sum(pcas_ast_t *d, Symbol var, pcas_ast_t **edge, int edges, const char **message) {
    pcas_ast_t *mid, *piece, *total = NULL;
    double v;
    int i;

    for(i = 0; i + 1 < edges && !pcas_cancelled; i++) {
        mid = midpoint(edge[i], edge[i + 1]);
        piece = value_at(d, var, mid);
        ast_Cleanup(mid);
        if(!approx_double(piece, &v)) {
            *message = "can't tell which one is on top";
            ast_Cleanup(piece);
            ast_Cleanup(total);
            return NULL;
        }
        ast_Cleanup(piece);
        piece = integral_of(v < 0 ? minus(ast_MakeInt(0), ast_Copy(d)) : ast_Copy(d), var, edge[i], edge[i + 1]);
        total = total == NULL ? piece : ast_MakeBinary(OP_ADD, total, piece);
    }
    return total;
}

/* between f and g from a to b, or from where they cross when there's no a and b */
static box_t *area(pcas_ast_t *f, pcas_ast_t *g, pcas_ast_t *a, pcas_ast_t *b, Symbol var, const char **message) {
    pcas_ast_t *d, *roots[SOLVE_MAX] = { NULL }, *edge[SOLVE_MAX + 2], *total = NULL;
    double lo = 0, hi = 0;
    int n, edges = 0, i;
    box_t *out = NULL;

    if(a != NULL && (!approx_double(a, &lo) || !approx_double(b, &hi))) {
        *message = "the limits have to be numbers";
        return NULL;
    }
    if(a != NULL && lo >= hi) {
        *message = "a has to be less than b";
        return NULL;
    }
    d = minus(ast_Copy(f), ast_Copy(g));
    simplify(d, SIMP_ALL);
    n = roots_between(d, var, a != NULL, lo, hi, roots);
    if(n == SOLVE_ALL) {
        *message = "they're the same curve";
    } else if(n < 0) {
        *message = "can't solve where they cross";
    } else {
        if(a != NULL)
            edge[edges++] = a;
        for(i = 0; i < n; i++)
            edge[edges++] = roots[i];
        if(a != NULL)
            edge[edges++] = b;
        if(edges < 2)
            *message = "they cross fewer than twice, give the limits: area(f, g, a, b)";
        else
            total = area_sum(d, var, edge, edges, message);
    }
    if(total != NULL)
        out = integral_answer("area =", total, message);
    ast_Cleanup(total);
    for(i = 0; i < n; i++)
        ast_Cleanup(roots[i]);
    ast_Cleanup(d);
    return out;
}

/* the roots of d strictly between lo and hi go on edge[*n..], taken over. false and a message when it can't. an identically 0 d adds none */
static bool edges_from(pcas_ast_t *d, Symbol var, double lo, double hi, pcas_ast_t **edge, int *n, const char **message) {
    pcas_ast_t *roots[SOLVE_MAX] = { NULL };
    int found = roots_between(d, var, true, lo, hi, roots), i;

    if(found < 0 && found != SOLVE_ALL) {
        *message = "can't solve where they cross";
        return false;
    }
    for(i = 0; i < found; i++)
        edge[(*n)++] = roots[i];
    return true;
}

/* a shell's |x - c| |f - g|. right is x past c, flip is f under g there */
static pcas_ast_t *shell_piece(pcas_ast_t *f, pcas_ast_t *g, pcas_ast_t *c, Symbol var, bool right, bool flip) {
    pcas_ast_t *x = ast_MakeSymbol((char)var);

    return ast_MakeBinary(OP_MULT, right ? minus(x, ast_Copy(c)) : minus(ast_Copy(c), x),
                          flip ? minus(ast_Copy(g), ast_Copy(f)) : minus(ast_Copy(f), ast_Copy(g)));
}

/* R^2 - r^2 for u = f - c and w = g - c. a curve each side of the axis is a disk of the bigger one */
static pcas_ast_t *washer_piece(pcas_ast_t *u, pcas_ast_t *w, double uv, double wv) {
    pcas_ast_t *uu = ast_MakeBinary(OP_POW, ast_Copy(u), ast_MakeInt(2)), *ww = ast_MakeBinary(OP_POW, ast_Copy(w), ast_MakeInt(2));

    if(uv * wv < 0) {
        ast_Cleanup(uv * uv > wv * wv ? ww : uu);
        return uv * uv > wv * wv ? uu : ww;
    }
    return fabs(uv) > fabs(wv) ? minus(uu, ww) : minus(ww, uu);
}

/* f and g from a to b turned about axis = c (X or Y). the same letter as var is shells, the other washers. NULL and a message when it can't */
static box_t *volume(pcas_ast_t *f, pcas_ast_t *g, pcas_ast_t *a, pcas_ast_t *b, pcas_ast_t *c, Symbol var, char axis, const char **message) {
    pcas_ast_t *d[3], *edge[3 * SOLVE_MAX + 2], *mid, *u, *w, *piece, *total = NULL, *setup;
    double lo, hi, cv, from, to, uv, wv, mv;
    bool shells = axis == (char)var, ok = true;
    int kinds = shells ? 1 : 3, n = 1, roots, i;
    box_t *col = NULL, *out;

    if(!approx_double(a, &lo) || !approx_double(b, &hi) || !approx_double(c, &cv)) {
        *message = "the limits and the axis have to be numbers";
        return NULL;
    }
    if(lo >= hi) {
        *message = "a has to be less than b";
        return NULL;
    }
    if(shells && cv > lo + 1e-9 && cv < hi - 1e-9) {
        *message = "the axis goes through the region";
        return NULL;
    }
    d[0] = minus(ast_Copy(f), ast_Copy(g));
    if(!shells) {
        d[1] = minus(ast_Copy(f), ast_Copy(c));
        d[2] = minus(ast_Copy(g), ast_Copy(c));
    }
    edge[0] = a;
    for(i = 0; i < kinds && ok; i++) {
        simplify(d[i], SIMP_ALL);
        ok = edges_from(d[i], var, lo, hi, edge, &n, message);
    }
    roots = n;
    if(ok) {
        sort_roots(edge + 1, roots - 1);
        edge[n++] = b;
    }
    /* a gap at a time, the sings read off in the middle of it */
    for(i = 0; ok && i + 1 < n && !pcas_cancelled; i++) {
        if(!approx_double(edge[i], &from) || !approx_double(edge[i + 1], &to) || to - from < 1e-12)
            continue;
        mid = midpoint(edge[i], edge[i + 1]);
        u = value_at(d[shells ? 0 : 1], var, mid);
        w = shells ? NULL : value_at(d[2], var, mid);
        if(!approx_double(u, &uv) || (w != NULL && !approx_double(w, &wv)) || !approx_double(mid, &mv)) {
            *message = "can't tell which one is on top";
            ok = false;
        } else {
            piece = shells ? shell_piece(f, g, c, var, mv > cv, uv < 0) : washer_piece(d[1], d[2], uv, wv);
            piece = integral_of(piece, var, edge[i], edge[i + 1]);
            total = total == NULL ? piece : ast_MakeBinary(OP_ADD, total, piece);
        }
        ast_Cleanup(mid);
        ast_Cleanup(u);
        ast_Cleanup(w);
    }
    for(i = 1; i < roots; i++)
        ast_Cleanup(edge[i]);
    for(i = 0; i < kinds; i++)
        ast_Cleanup(d[i]);
    if(!ok || total == NULL) {
        ast_Cleanup(total);
        return NULL;
    }
    total = ast_MakeBinary(OP_MULT, shells ? ast_MakeBinary(OP_MULT, ast_MakeInt(2), ast_MakeSymbol(SYM_PI)) : ast_MakeSymbol(SYM_PI), total);
    setup = ast_Copy(total);
    out = integral_answer("V =", total, message);
    if(out != NULL) {
        col = layout_box(BOX_COL, 0);
        layout_add(col, labelled("set up", value_box(setup)));
        layout_add(col, out);
        layout_finish(col, &draw_font);
    }
    ast_Cleanup(setup);
    ast_Cleanup(total);
    return col;
}

/* the c in (a, b) where f'(c) is the slope from a to b. Rolle's theorem when the two ends of f are level */
static box_t *mean_value(pcas_ast_t *f, pcas_ast_t *a, pcas_ast_t *b, Symbol var, const char **message) {
    pcas_ast_t *d, *eq, *roots[SOLVE_MAX] = { NULL };
    double lo = 0, hi = 0;
    int n, i;
    box_t *out = NULL;

    if(!approx_double(a, &lo) || !approx_double(b, &hi)) {
        *message = "the ends have to be numbers";
        return NULL;
    }
    if(lo >= hi) {
        *message = "a has to be less than b";
        return NULL;
    }
    d = derive(f, var, 1);
    if(d == NULL)
        return NULL;
    /* f'(c) - (f(b) - f(a)) / (b - a), both the ends worked out first */
    eq = minus(d, ast_MakeBinary(OP_DIV, minus(value_at(f, var, b), value_at(f, var, a)), minus(ast_Copy(b), ast_Copy(a))));
    simplify(eq, SIMP_ALL);
    n = roots_between(eq, var, true, lo, hi, roots);
    ast_Cleanup(eq);
    if(n == SOLVE_ALL)
        *message = "f' is the slope everywhere, any c works";
    else if(n < 0)
        *message = "can't solve where f' is that slope";
    else if(n == 0)
        *message = "no c between a and b, f may not be continuous and differentiable there";
    else
        out = solutions_box('c', roots, n);
    for(i = 0; i < n; i++)
        ast_Cleanup(roots[i]);
    return out;
}

/* both sides of the limit against the value, and which of the four things it is */
static box_t *continuity(pcas_ast_t *f, pcas_ast_t *a, Symbol var, const char **message) {
    /* no f(a) when a bottom is 0 there, simplify says 0/0 is 1 */
    pcas_ast_t *left = limit_of(f, var, a, -1), *right = limit_of(f, var, a, 1), *at = bottom_zero(f, var, a) ? NULL : value_at(f, var, a);
    double l = 0, r = 0, v = 0;
    bool left_ok, right_ok, at_ok;
    const char *verdict;
    box_t *col = NULL;

    if(pcas_cancelled) {
        /* nothing to say, the caller reads pcas_cancelled */
    } else if(has_op(left, OP_LIMIT) || has_op(right, OP_LIMIT)) {
        *message = "can't work that limit out";
    } else {
        left_ok = approx_double(left, &l);
        right_ok = approx_double(right, &r);
        at_ok = at != NULL && approx_double(at, &v);
        if(left_ok && right_ok && same(l, r))
            verdict = at_ok && same(v, l) ? "continuous" : "removable discontinuity";
        else if(left_ok && right_ok)
            verdict = "jump discontinuity";
        else
            verdict = "infinite discontinuity";
        col = layout_box(BOX_COL, 0);
        layout_add(col, labelled("from the left", value_box(left)));
        layout_add(col, labelled("from the right", value_box(right)));
        if(at_ok)
            layout_add(col, labelled("f(a)", value_box(at)));
        else
            layout_add(col, layout_text("f(a) is undefined", 0));
        layout_add(col, layout_text(verdict, 0));
        layout_finish(col, &draw_font);
    }
    ast_Cleanup(left);
    ast_Cleanup(right);
    ast_Cleanup(at);
    return col;
}

bool applied_run(const char *cmd, char *src, box_t *in) {
    pcas_ast_t *a[ARGS_MAX] = { NULL };
    pcas_error_t err = E_SUCCESS;
    char *piece[ARGS_MAX + 1];
    const char *message = NULL;
    box_t *out = NULL;
    clock_t start;
    Symbol var = SYM_X;
    char shown = 'x', axis = 'X';
    int c, n, parsed, i;
    bool has_var;

    if(cmd == NULL)
        return false;
    for(c = 0; c < P_COUNT; c++)
        if(strcmp(cmd, cmds[c].name) == 0)
            break;
    if(c == P_COUNT)
        return false;

    n = args_split(src, piece, ARGS_MAX);
    if(n < cmds[c].least || n > cmds[c].most) {
        push(in, NULL, cmds[c].usage, 0);
        return true;
    }
    /* a letter on the end is the variable, when there's room for one. area has two forms, f g x and f g a b x */
    has_var = c == P_AREA ? n == 3 || n == 5 : n == cmds[c].most;
    parsed = has_var ? n - 1 : n;
    if(has_var && !args_var(piece[n - 1], &var, &shown))
        message = cmds[c].usage;
    if(c == P_VOLUME && message == NULL) {
        /* axis typed y=3 or x=0, cut at the = */
        axis = piece[4][0];
        if((axis != 'X' && axis != 'Y') || piece[4][1] != '=' || (var != SYM_X && var != SYM_Y))
            message = cmds[c].usage;
        else
            piece[4] += 2;
    }
    for(i = 0; i < parsed && message == NULL; i++)
        if((a[i] = args_parse(piece[i], &err)) == NULL)
            message = error_text[err];
    if(message != NULL) {
        push(in, NULL, message, 0);
        for(i = 0; i < ARGS_MAX; i++)
            ast_Cleanup(a[i]);
        return true;
    }

    start = clock();
    /* the ends and the point, not f and g: the full simplify splits (x^2-1)/(x-1) into a sum whose limits don't come out */
    for(i = c == P_AREA || c == P_VOLUME ? 2 : 1; i < parsed; i++)
        simplify(a[i], SIMP_ALL);
    if(has_var && !has_symbol(a[0], var) && !((c == P_AREA || c == P_VOLUME) && has_symbol(a[1], var)))        message = "that letter isn't in the function";
    else if(c == P_AVERAGE)
        out = average(a[0], a[1], a[2], var, &message);
    else if(c == P_AREA)
        out = area(a[0], a[1], parsed > 3 ? a[2] : NULL, parsed > 3 ? a[3] : NULL, var, &message);
    else if(c == P_VOLUME)
        out = volume(a[0], a[1], a[2], a[3], a[4], var, axis, &message);
    else if(c == P_MVT)
        out = mean_value(a[0], a[1], a[2], var, &message);
    else
        out = continuity(a[0], a[1], var, &message);

    if(pcas_cancelled && out == NULL)
        message = pcas_oom ? "out of memory" : "cancelled";
    push(in, out, message, (unsigned long)((clock() - start) * 1000 / CLOCKS_PER_SEC));
    for(i = 0; i < ARGS_MAX; i++)
        ast_Cleanup(a[i]);
    return true;
}
