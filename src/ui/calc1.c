#include "calc1.h"

#include "app.h"
#include "args.h"
#include "ineq.h"
#include "work.h"
#include "engine/cas/steps.h"

#include <math.h>
#include <string.h>
#include <time.h>

#include "engine/ast.h"
#include "engine/cas/cas.h"
#include "render/draw.h"

#define ARGS_MAX 4

enum { C_TANGENT, C_CRITICAL, C_EXTREMA, C_INFLECT, C_IMPLICIT, C_INCREASING, C_TAYLOR, C_NORMAL, C_COUNT };

static const struct {
    const char *name, *usage;
    int least, most;
} cmds[C_COUNT] = {
    { "tangent", "tangent(f, a, x)", 2, 3 },
    { "critical", "critical(f, x)", 1, 2 },
    { "extrema", "extrema(f, x)", 1, 2 },
    { "inflect", "inflect(f, x)", 1, 2 },
    { "implicit", "implicit(equation, y, x)", 1, 3 },
    { "increasing", "increasing(f, x)", 1, 2 },
    { "taylor", "taylor(f, a, n, x)", 3, 4 },
    { "normal", "normal(f, a, x)", 2, 3 },
};

pcas_ast_t *derive(pcas_ast_t *f, Symbol var, int times) {
    pcas_ast_t *e = ast_Copy(f), *x = ast_MakeSymbol((char)var), *d;

    for(; times > 0 && e != NULL; times--) {
        d = slope_of(e, x);
        ast_Cleanup(e);
        e = d;
    }
    ast_Cleanup(x);
    return e;
}

/* f with v put in for var, worked out */
pcas_ast_t *value_at(pcas_ast_t *f, Symbol var, pcas_ast_t *v) {
    pcas_ast_t *e = ast_Copy(f), *x = ast_MakeSymbol((char)var);

    if(e == NULL) { /* oom */
        ast_Cleanup(x);
        return NULL;
    }
    if(e->type == NODE_SYMBOL && e->op.symbol == var) {
        ast_Cleanup(e);
        e = ast_Copy(v);
    } else {
        substitute(e, x, v);
    }
    ast_Cleanup(x);
    simplify(e, SIMP_ALL);
    return e;
}

/* var -> a from one side (-1 or 1), worked out */
pcas_ast_t *limit_at(pcas_ast_t *f, Symbol var, pcas_ast_t *a, int side) {
    pcas_ast_t *e = ast_MakeOperator(OP_LIMIT);

    ast_ChildAppend(e, ast_Copy(f));
    ast_ChildAppend(e, ast_MakeSymbol((char)var));
    ast_ChildAppend(e, ast_Copy(a));
    ast_ChildAppend(e, ast_MakeInt(side));
    simplify(e, SIMP_ALL);
    return e;
}

/* r a thousandth to one side (side is -1 or 1), not worked out */
pcas_ast_t *beside(pcas_ast_t *r, int side) {
    return ast_MakeBinary(OP_ADD, ast_Copy(r), ast_MakeBinary(OP_DIV, ast_MakeInt(side), ast_MakeInt(1000)));
}

bool numbers_same(double a, double b) {
    return fabs(a - b) <= 1e-9 * (fabs(b) > 1 ? fabs(b) : 1);
}

/* the limit from each side and f(a), each NULL when there isn't one, with the verdict under them */
box_t *limits_box(pcas_ast_t *left, pcas_ast_t *right, pcas_ast_t *at, const char *verdict) {
    box_t *col = layout_box(BOX_COL, 0);

    layout_add(col, labelled("from the left", left != NULL ? value_box(left) : layout_text("undefined", 0)));
    layout_add(col, labelled("from the right", right != NULL ? value_box(right) : layout_text("undefined", 0)));
    layout_add(col, at != NULL ? labelled("f(a)", value_box(at)) : layout_text("f(a) is undefined", 0));
    layout_add(col, layout_text(verdict, 0));
    layout_finish(col, &draw_font);
    return col;
}

/* the sign of f a thousanth to one side of r (side is -1 or 1), 0 if it's 0, 2 if it isn't a number (a root with an n in it) */
static int sign_beside(pcas_ast_t *f, Symbol var, pcas_ast_t *r, int side) {
    pcas_ast_t *p = beside(r, side);
    pcas_ast_t *e = value_at(f, var, p);
    double v;
    int s = 2;

    if(approx_double(e, &v))
        s = v > 0 ? 1 : v < 0 ? -1 : 0;
    ast_Cleanup(e);
    ast_Cleanup(p);
    return s;
}

/* the roots of f = 0 and how many, a message when there's no list of them */
static int zeros_of(pcas_ast_t *f, Symbol var, pcas_ast_t **roots, const char **message) {
    int n = solve_ex(f, var, roots, false);

    if(n == SOLVE_ALL)
        *message = "it's 0 everywhere";
    else if(n < 0)
        *message = "can't solve where that is 0";
    return n;
}

/* f'(a) worked out. NULL on oom, or with a message when f or f' is undefined at a */
pcas_ast_t *slope_at(pcas_ast_t *f, Symbol var, pcas_ast_t *a, const char **message) {
    pcas_ast_t *d = derive(f, var, 1), *m;

    if(d == NULL)
        return NULL;
    if(bottom_zero(f, var, a) || bottom_zero(d, var, a)) { /* simplify says 0/0 is 1, |x| at 0 would get a slope */
        *message = "f isn't defined there, or has no slope there";
        ast_Cleanup(d);
        return NULL;
    }
    m = value_at(d, var, a);
    ast_Cleanup(d);
    return m;
}

/* y = f'(a)x + b, or the line across it (slope -1 / f'(a), x = a where f' is 0) when normal */
static box_t *tangent(pcas_ast_t *f, pcas_ast_t *a, Symbol var, bool normal, const char **message) {
    pcas_ast_t *m = slope_at(f, var, a, message), *fa, *line;
    box_t *out = NULL;
    double v;

    if(m == NULL)
        return NULL;
    fa = value_at(f, var, a);
    if(normal && approx_double(m, &v) && v == 0) { /* flat there, the normal is straight up */
        out = labelled("x =", value_box(a));
        layout_finish(out, &draw_font);
        ast_Cleanup(m);
        ast_Cleanup(fa);
        return out;
    }
    if(normal)
        m = ast_MakeBinary(OP_DIV, ast_MakeInt(-1), m);
    /* fa - m a + m x */
    line = ast_MakeBinary(OP_ADD, ast_MakeBinary(OP_ADD, fa, ast_MakeBinary(OP_MULT, ast_MakeInt(-1),
                          ast_MakeBinary(OP_MULT, ast_Copy(m), ast_Copy(a)))),
                          ast_MakeBinary(OP_MULT, m, ast_MakeSymbol((char)var)));
    simplify(line, SIMP_ALL);
    if(!pcas_cancelled) {
        out = labelled("y =", value_box(line));
        layout_finish(out, &draw_font); /* no column around it to finish it */
    }
    ast_Cleanup(line);
    return out;
}

/* the points where f' = 0 and changes sign (f'' for inflection), a line each */
static box_t *turning_points(pcas_ast_t *f, Symbol var, bool inflection, const char **message) {
    pcas_ast_t *roots[SOLVE_MAX] = { NULL }, *d = derive(f, var, inflection ? 2 : 1), *y;
    box_t *col = NULL;
    int n, i, before, after, lines = 0, unsure = 0;

    if(d == NULL)
        return NULL;
    n = zeros_of(d, var, roots, message);
    if(n > 0) {
        col = layout_box(BOX_COL, 0);
        for(i = 0; i < n && !pcas_cancelled; i++) {
            before = sign_beside(d, var, roots[i], -1);
            after = sign_beside(d, var, roots[i], 1);
            if(before == 2 || after == 2) {
                unsure++;
                continue;
            }
            if(before == 0 || after == 0 || before == after)
                continue;
            y = value_at(f, var, roots[i]);
            layout_add(col, labelled(inflection ? "inflection" : before > 0 ? "max" : "min", point_box(roots[i], y)));
            ast_Cleanup(y);
            lines++;
        }
        if(lines == 0) {
            layout_free(col);
            col = NULL;
            *message = unsure > 0 ? "can't tell where it turns" : inflection ? "no inflection points" : "no maximums or minimums";
        } else {
            layout_finish(col, &draw_font);
        }
    } else if(n == 0) {
        *message = inflection ? "no inflection points" : "no maximums or minimums";
    }
    for(i = 0; i < n; i++)
        ast_Cleanup(roots[i]);
    ast_Cleanup(d);
    return col;
}

/* what ineq_box says when no x does it */
static bool never(const box_t *b) {
    return b->kind == BOX_TEXT && b->text != NULL && strcmp(b->text, "false") == 0;
}

/* where f' is over 0 and where it's under, one line each, none for a side that never happens */
static box_t *slopes(pcas_ast_t *f, Symbol var, char shown, const char **message) {
    pcas_ast_t *d = derive(f, var, 1);
    box_t *col = NULL, *up, *down;
    const char *why = NULL;

    if(d == NULL)
        return NULL;
    if(!has_symbol(d, var)) { /* a line, or a constant */
        *message = "the slope is the same everywhere";
        ast_Cleanup(d);
        return NULL;
    }
    up = ineq_box(d, var, shown, '>', true, &why);
    down = up != NULL ? ineq_box(d, var, shown, '<', true, &why) : NULL;
    if(up != NULL && down != NULL) {
        col = layout_box(BOX_COL, 0);
        if(never(up))
            layout_free(up);
        else
            layout_add(col, labelled("increasing", up));
        if(never(down))
            layout_free(down);
        else
            layout_add(col, labelled("decreasing", down));
        if(col->child == NULL) { /* both said no x at all */
            layout_free(col);
            col = NULL;
            *message = "can't tell where it goes up or down";
        } else {
            layout_finish(col, &draw_font);
        }
    } else {
        layout_free(up);
        layout_free(down);
        *message = why;
    }
    ast_Cleanup(d);
    return col;
}

#define ORDER_MAX 8 /* each term is a *dih*rivative and a simplify */

/* f(a) + f'(a)(x-a) + f''(a)/2 (x-a)^2 + ... up to the order, zero terms left out. order and a are worked out already. NULL and a message when it doesn't work */
static box_t *taylor(pcas_ast_t *f, pcas_ast_t *a, pcas_ast_t *order, Symbol var, const char **message) {
    pcas_ast_t *d = ast_Copy(f), *sum = ast_MakeOperator(OP_ADD), *x = ast_MakeSymbol((char)var), *c, *base, *next;
    box_t *out = NULL;
    mp_small n, k;
    int fact = 1;

    if(!whole_number(order, &n) || n < 0 || n > ORDER_MAX) {
        *message = "the order is a whole number from 0 to 8";
    } else {
        for(k = 0; k <= n && d != NULL && !pcas_cancelled; k++) {
            c = ast_MakeBinary(OP_DIV, value_at(d, var, a), ast_MakeInt(fact));
            simplify(c, SIMP_ALL);
            if(!is_ast_int(c, 0)) {
                if(k == 0) {
                    ast_ChildAppend(sum, c);
                } else {
                    base = is_ast_int(a, 0) ? ast_Copy(x) : ast_MakeBinary(OP_ADD, ast_Copy(x), ast_MakeBinary(OP_MULT, ast_MakeInt(-1), ast_Copy(a)));
                    c = ast_MakeBinary(OP_MULT, c, k == 1 ? base : ast_MakeBinary(OP_POW, base, ast_MakeInt(k)));
                    simplify(c, SIMP_ALL);
                    ast_ChildAppend(sum, c);
                }
            } else {
                ast_Cleanup(c);
            }
            next = derive(d, var, 1);
            ast_Cleanup(d);
            d = next;
            fact *= (int)k + 1;
        }
        if(pcas_cancelled) {
            *message = pcas_oom ? "out of memory" : "cancelled";
        } else if(ast_ChildLength(sum) == 0) {
            out = layout_text("0", 0);
        } else {
            simplify(sum, SIMP_NORMALIZE | SIMP_COMMUTATIVE);
            simplify_canonical_form(sum, CANONICAL_ALL);
            out = layout_build(sum, &draw_font);
        }
    }
    ast_Cleanup(d);
    ast_Cleanup(sum);
    ast_Cleanup(x);
    return out;
}

/* implicit(x^2+y^2=25, y, x): left - right in *f, y and x in dep and ind (defaults when missing), their letters go in label. what's wrong with it, or NULL */
static const char *read_implicit(char **piece, int n, pcas_ast_t **f, Symbol *dep, Symbol *ind, char *label) {
    pcas_ast_t *l, *r = NULL;
    pcas_error_t err = E_SUCCESS;
    char *eq = strchr(piece[0], '=');

    *dep = SYM_Y;
    *ind = SYM_X;
    if(eq == NULL || (n > 1 && !args_var(piece[1], dep, &label[1])) || (n > 2 && !args_var(piece[2], ind, &label[4])) || *dep == *ind)
        return cmds[C_IMPLICIT].usage;
    *eq = '\0';
    l = args_parse(piece[0], &err);
    if(l != NULL)
        r = args_parse(eq + 1, &err);
    if(l == NULL || r == NULL) {
        ast_Cleanup(l);
        return error_text[err];
    }
    *f = ast_MakeBinary(OP_ADD, l, ast_MakeBinary(OP_MULT, ast_MakeInt(-1), r));
    return NULL;
}

/* implicit(x^2+y^2=25, y, x): dy/dx = -(dF/dx)/(dF/dy) of F = left - right */
static void implicit(char **piece, int n, box_t *in) {
    pcas_ast_t *f = NULL, *x, *y, *fx, *fy, *answer;
    Symbol dep, ind;
    char label[12] = "dy/dx =";
    const char *message = read_implicit(piece, n, &f, &dep, &ind, label);
    box_t *out = NULL;

    if(message != NULL) {
        push(in, NULL, message, 0);
        return;
    }
    x = ast_MakeSymbol((char)ind);
    y = ast_MakeSymbol((char)dep);
    fx = slope_of(f, x);
    fy = slope_of(f, y);
    if(fx == NULL || fy == NULL)
        message = pcas_oom ? "out of memory" : "cancelled";
    else if(is_ast_int(fy, 0))
        message = "no y in it, so no dy/dx";
    else {
        answer = ast_MakeBinary(OP_DIV, ast_MakeBinary(OP_MULT, ast_MakeInt(-1), ast_Copy(fx)), ast_Copy(fy));
        simplify(answer, SIMP_ALL);
        if(pcas_cancelled) {
            message = pcas_oom ? "out of memory" : "cancelled";
        } else {
            out = labelled(label, value_box(answer));
            layout_finish(out, &draw_font);
        }
        ast_Cleanup(answer);
    }
    push(in, out, message, 0);
    ast_Cleanup(f);
    ast_Cleanup(x);
    ast_Cleanup(y);
    ast_Cleanup(fx);
    ast_Cleanup(fy);
}

/* the pieces of a command (not implicit): the function in a[0], the point in a[1] for a tangent or taylor, the order in a[2] for taylor, the variable from a letter at the end, x if there isn't one.
   what's wrong with them, or NULL. a[] is the caller's to free either way */
static const char *read_command(int c, char **piece, int n, pcas_ast_t **a, Symbol *var, char *shown) {
    pcas_error_t err = E_SUCCESS;
    int i, parsed = c == C_TAYLOR ? 3 : c == C_TANGENT || c == C_NORMAL ? 2 : 1;

    *var = SYM_X;
    *shown = 'x';
    if(n == cmds[c].most && !args_var(piece[n - 1], var, shown))
        return cmds[c].usage;
    for(i = 0; i < parsed; i++)
        if((a[i] = args_parse(piece[i], &err)) == NULL)
            return error_text[err];
    return NULL;
}

bool calc1_run(const char *cmd, char *src, box_t *in) {
    pcas_ast_t *a[ARGS_MAX] = { NULL }, *d, *roots[SOLVE_MAX] = { NULL };
    char *piece[ARGS_MAX + 1];
    const char *message = NULL;
    box_t *out = NULL;
    clock_t start;
    Symbol var = SYM_X;
    char shown = 'x';
    int c, n, i, k = 0;

    if(cmd == NULL)
        return false;
    for(c = 0; c < C_COUNT; c++)
        if(strcmp(cmd, cmds[c].name) == 0)
            break;
    if(c == C_COUNT)
        return false;

    n = args_split(src, piece, ARGS_MAX);
    if(n < cmds[c].least || n > cmds[c].most) {
        push(in, NULL, cmds[c].usage, 0);
        return true;
    }
    if(c == C_IMPLICIT) {
        implicit(piece, n, in);
        return true;
    }
    message = read_command(c, piece, n, a, &var, &shown);
    if(message != NULL) {
        push(in, NULL, message, 0);
        for(i = 0; i < ARGS_MAX; i++)
            ast_Cleanup(a[i]);
        return true;
    }

    start = clock();
    simplify(a[0], SIMP_ALL);
    if(c == C_TANGENT || c == C_NORMAL) {
        simplify(a[1], SIMP_ALL);
        out = tangent(a[0], a[1], var, c == C_NORMAL, &message);
    } else if(c == C_CRITICAL) {
        d = derive(a[0], var, 1);
        k = d != NULL ? zeros_of(d, var, roots, &message) : 0;
        if(k > 0)
            out = solutions_box(shown, roots, k);
        else if(k == 0 && message == NULL && d != NULL)
            message = "no critical points";
        ast_Cleanup(d);
        for(i = 0; i < k; i++)
            ast_Cleanup(roots[i]);
    } else if(c == C_INCREASING) {
        out = slopes(a[0], var, shown, &message);
    } else if(c == C_TAYLOR) {
        simplify(a[1], SIMP_ALL);
        simplify(a[2], SIMP_ALL);
        out = taylor(a[0], a[1], a[2], var, &message);
    } else {
        out = turning_points(a[0], var, c == C_INFLECT, &message);
    }

    if(pcas_cancelled && out == NULL)
        message = pcas_oom ? "out of memory" : "cancelled";
    push(in, out, message, (unsigned long)((clock() - start) * 1000 / CLOCKS_PER_SEC));
    for(i = 0; i < ARGS_MAX; i++)
        ast_Cleanup(a[i]);
    return true;
}

const char *calc1_work(const char *cmd, char *src) {
    pcas_ast_t *a[ARGS_MAX] = { NULL };
    char *piece[ARGS_MAX + 1], shown;
    const char *message;
    Symbol var;
    int c, n, i;

    for(c = 0; c < C_COUNT && strcmp(cmd, cmds[c].name) != 0; c++)
        ;
    if(c == C_COUNT || c == C_INCREASING || c == C_TAYLOR || c == C_NORMAL)
        return "no steps for that one yet";
    n = args_split(src, piece, ARGS_MAX);
    if(n < cmds[c].least || n > cmds[c].most)
        return cmds[c].usage;
    if(c == C_IMPLICIT) {
        char label[12];
        Symbol dep, ind;

        message = read_implicit(piece, n, &a[0], &dep, &ind, label);
        if(message == NULL) {
            a[1] = ast_MakeSymbol((char)dep); /*the y, for the steps*/
            message = work_open_command(STEPS_IMPLICIT, a[0], a[1], ind);
        }
        for(i = 0; i < ARGS_MAX; i++)
            ast_Cleanup(a[i]);
        return message;
    }
    message = read_command(c, piece, n, a, &var, &shown);
    if(message == NULL)
        message = work_open_command(c == C_TANGENT ? STEPS_TANGENT : c == C_CRITICAL ? STEPS_CRITICAL : c == C_EXTREMA ? STEPS_EXTREMA : STEPS_INFLECT,
                                    a[0], a[1], var);
    for(i = 0; i < ARGS_MAX; i++) /*the page copies what it keeps*/
        ast_Cleanup(a[i]);
    return message;
}
