#include "calc1.h"

#include "app.h"
#include "args.h"
#include "ineq.h"
#include "work.h"
#include "engine/cas/steps.h"

#include <string.h>
#include <time.h>

#include "engine/ast.h"
#include "engine/cas/cas.h"
#include "render/draw.h"

#define ARGS_MAX 4

enum { C_TANGENT, C_CRITICAL, C_EXTREMA, C_INFLECT, C_IMPLICIT, C_INCREASING, C_COUNT };

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
};

/* d/dvar of f, times over. NULL on oom or cancelled */
static pcas_ast_t *derive(pcas_ast_t *f, Symbol var, int times) {
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
static pcas_ast_t *at(pcas_ast_t *f, Symbol var, pcas_ast_t *v) {
    pcas_ast_t *e = ast_Copy(f), *x = ast_MakeSymbol((char)var);

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

/* the sign of f a thousanth to one side of r (side is -1 or 1), 0 if it's 0, 2 if it isn't a number (a root with an n in it) */
static int sign_beside(pcas_ast_t *f, Symbol var, pcas_ast_t *r, int side) {
    pcas_ast_t *p = ast_MakeBinary(OP_ADD, ast_Copy(r),
                                   ast_MakeBinary(OP_DIV, ast_MakeInt(side), ast_MakeInt(1000)));
    pcas_ast_t *e = at(f, var, p);
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

/* y = f'(a)x + b */
static box_t *tangent(pcas_ast_t *f, pcas_ast_t *a, Symbol var) {
    pcas_ast_t *d = derive(f, var, 1), *m, *fa, *line;
    box_t *out = NULL;

    if(d == NULL)
        return NULL;
    m = at(d, var, a);
    fa = at(f, var, a);
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
    ast_Cleanup(d);
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
            y = at(f, var, roots[i]);
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

/* where f' is over 0 and where it's under, one line each */
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
        layout_add(col, labelled("increasing", up));
        layout_add(col, labelled("decreasing", down));
        layout_finish(col, &draw_font);
    } else {
        layout_free(up);
        layout_free(down);
        *message = why;
    }
    ast_Cleanup(d);
    return col;
}

/* implicit(x^2+y^2=25, y, x): dy/dx = -(dF/dx)/(dF/dy) of F = left - right. y and x are the defaults */
static void implicit(char **piece, int n, box_t *in) {
    pcas_ast_t *l, *r = NULL, *f, *x, *y, *fx, *fy, *answer;
    Symbol dep = SYM_Y, ind = SYM_X;
    char label[12] = "dy/dx =", *eq = strchr(piece[0], '=');
    pcas_error_t err = E_SUCCESS;
    const char *message = NULL;
    box_t *out = NULL;

    if(eq == NULL || (n > 1 && !args_var(piece[1], &dep, &label[1])) || (n > 2 && !args_var(piece[2], &ind, &label[4]))
       || dep == ind) {
        push(in, NULL, cmds[C_IMPLICIT].usage, 0);
        return;
    }
    *eq = '\0';
    l = args_parse(piece[0], &err);
    if(l != NULL)
        r = args_parse(eq + 1, &err);
    if(l == NULL || r == NULL) {
        ast_Cleanup(l);
        push(in, NULL, error_text[err], 0);
        return;
    }
    f = ast_MakeBinary(OP_ADD, l, ast_MakeBinary(OP_MULT, ast_MakeInt(-1), r));
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

/* the pieces of a command (not implicit): the function in a[0], the point in a[1] for a tangent, the variable from a letter at the end, x if there isn't one.
   what's wrong with them, or NULL. a[] is the caller's to free either way */
static const char *read_command(int c, char **piece, int n, pcas_ast_t **a, Symbol *var, char *shown) {
    pcas_error_t err = E_SUCCESS;
    int i;

    *var = SYM_X;
    *shown = 'x';
    if(n == cmds[c].most && !args_var(piece[n - 1], var, shown))
        return cmds[c].usage;
    for(i = 0; i < (c == C_TANGENT ? 2 : 1); i++)
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
        return true;
    }

    start = clock();
    simplify(a[0], SIMP_ALL);
    if(c == C_TANGENT) {
        simplify(a[1], SIMP_ALL);
        out = tangent(a[0], a[1], var);
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
    if(c == C_COUNT || c == C_IMPLICIT || c == C_INCREASING)
        return "no steps for that one yet";
    n = args_split(src, piece, ARGS_MAX);
    if(n < cmds[c].least || n > cmds[c].most)
        return cmds[c].usage;
    message = read_command(c, piece, n, a, &var, &shown);
    if(message == NULL)
        message = work_open_command(c == C_TANGENT ? STEPS_TANGENT : c == C_CRITICAL ? STEPS_CRITICAL : c == C_EXTREMA ? STEPS_EXTREMA : STEPS_INFLECT,
                                    a[0], a[1], var);
    for(i = 0; i < ARGS_MAX; i++) /*the page copies what it keeps*/
        ast_Cleanup(a[i]);
    return message;
}
