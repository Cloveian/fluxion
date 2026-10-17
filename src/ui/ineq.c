#include "ineq.h"

#include "app.h"
#include "numval.h"

#include <math.h>
#include <stdlib.h>
#include <time.h>

#include "engine/cas/cas.h"
#include "editor/editor.h"
#include "render/draw.h"

/*
    inequalities by a sign chart, the way you'd do it on paper. the edges are
    where the top is 0 and where the bottom is 0, sorted by their decimals.
    between two edges the sign can't change, so one test point says it for the
    whole gap. an edge counts itself when it's <= or >= and it's a root of the
    top, never when it's a hole
*/

#define EDGES_MAX (2 * SOLVE_MAX)

typedef struct {
    pcas_ast_t *at; /* exact, for the answer */
    float x; /* for sorting and picking test points */
    bool hole;
} edge_t;

/* the real roots of e = 0 as edges, a root already there just gets hole added.
   false when solve gives up or a root isn't a plain number (a trig family with
   its n, another letter) */
static bool add_roots(pcas_ast_t *e, Symbol var, bool hole, edge_t *edges, int *n) {
    pcas_ast_t *roots[SOLVE_MAX] = { NULL };
    bool ok = true;
    int got, i, k;

    got = solve(e, var, roots);
    if(got == SOLVE_UNKNOWN || got == SOLVE_ALL)
        return false; /* SOLVE_ALL can't happen, the caller takes out the no-x case first */

    for(i = 0; i < got && ok; i++) {
        pcas_ast_t *d = ast_Copy(roots[i]);
        float x = 0;

        approx(d);
        ok = num_float(d, &x) && isfinite(x);
        ast_Cleanup(d);
        if(!ok)
            break;

        for(k = 0; k < *n && fabsf(edges[k].x - x) > 1e-6F * (1 + fabsf(x)); k++)
            ;
        if(k < *n) {
            edges[k].hole |= hole; /* x/x, top and bottom both 0, still a hole */
            continue;
        }
        if(*n == EDGES_MAX) {
            ok = false;
            break;
        }
        edges[*n].at = roots[i];
        edges[*n].x = x;
        edges[*n].hole = hole;
        roots[i] = NULL;
        (*n)++;
    }

    for(i = 0; i < got; i++)
        if(roots[i] != NULL)
            ast_Cleanup(roots[i]);
    return ok;
}

static int by_x(const void *a, const void *b) {
    float x = ((const edge_t *)a)->x, y = ((const edge_t *)b)->x;

    return x < y ? -1 : x > y;
}

/* the sign of f at x, -1 0 or 1, 2 when it can't tell (an i, another letter) */
static int sign_at(pcas_ast_t *f, Symbol var, float x) {
    float y = num_at(f, var, x);

    if(isnan(y))
        return 2;
    return y > 0 ? 1 : y < 0 ? -1 : 0;
}

/* where to test the gap before edge i (i = n is the one after the last edge).
   the two ends go out by at least 1 */
static float test_point(const edge_t *edges, int n, int i) {
    if(n == 0)
        return 0;
    if(i == 0)
        return edges[0].x - (fabsf(edges[0].x) > 1 ? fabsf(edges[0].x) : 1);
    if(i == n)
        return edges[n - 1].x + (fabsf(edges[n - 1].x) > 1 ? fabsf(edges[n - 1].x) : 1);
    return (edges[i - 1].x + edges[i].x) / 2;
}

static bool wanted(int sign, char rel) {
    return rel == '<' || rel == CH_LE ? sign < 0 : sign > 0;
}

static void put_text(box_t *row, const char *t) {
    layout_add(row, layout_space(LAYOUT_SPACE(0)));
    layout_add(row, layout_text(t, 0));
    layout_add(row, layout_space(LAYOUT_SPACE(0)));
}

/* value_box tidies the tree it gets, an edge can show up in two pieces */
static void put_value(box_t *row, pcas_ast_t *v) {
    pcas_ast_t *c = ast_Copy(v);

    layout_add(row, value_box(c));
    ast_Cleanup(c);
}

/* one piece: lo < x < hi, x <= hi, x > lo or x = c. NULL for no end on a side */
box_t *ineq_row(char shown, pcas_ast_t *lo, bool lo_in, pcas_ast_t *hi, bool hi_in) {
    char name[2] = { shown, '\0' }, lt[2] = { '<', '\0' }, le[2] = { CH_LE, '\0' };
    box_t *row = layout_box(BOX_ROW, 0);

    if(lo != NULL && lo == hi) {
        layout_add(row, layout_text(name, 0));
        put_text(row, "=");
        put_value(row, lo);
    } else if(hi == NULL) {
        char gt[2] = { '>', '\0' }, ge[2] = { CH_GE, '\0' };

        layout_add(row, layout_text(name, 0));
        put_text(row, lo_in ? ge : gt);
        put_value(row, lo);
    } else {
        if(lo != NULL) {
            put_value(row, lo);
            put_text(row, lo_in ? le : lt);
        }
        layout_add(row, layout_text(name, 0));
        put_text(row, hi_in ? le : lt);
        put_value(row, hi);
    }
    layout_finish(row, &draw_font);
    return row;
}

/* the chart as one list: gap 0, edge 0, gap 1, ..., gap n. even p is a gap */
static bool truth(const bool *gap, const bool *edge, int p) {
    return p % 2 == 0 ? gap[p / 2] : edge[p / 2];
}

void ineq_run(pcas_ast_t *f, Symbol var, char shown, char rel, box_t *in) {
    edge_t edges[EDGES_MAX];
    bool gap[EDGES_MAX + 1], edge[EDGES_MAX];
    pcas_ast_t *g = ast_Copy(f), *top, *bottom = NULL;
    const char *fail = NULL;
    box_t *col = NULL;
    clock_t start = clock();
    int n = 0, i, p, runs = 0;

    simplify(g, SIMP_ALL);
    comdenom(g, true);
    top = g;
    if(isoptype(g, OP_DIV)) {
        top = ast_ChildGet(g, 0);
        bottom = ast_ChildGet(g, 1);
    }

    if(!has_symbol(g, var)) {
        /* 3 < 5 and friends, or x - x < 1. just the one sign */
        int s = sign_at(g, var, 0);

        if(s == 2)
            fail = "can't tell the sign of that";
        else
            col = layout_text(wanted(s, rel) || (s == 0 && (rel == CH_LE || rel == CH_GE)) ? "true" : "false", 0);
    } else if(!add_roots(top, var, false, edges, &n)
              || (bottom != NULL && has_symbol(bottom, var) && !add_roots(bottom, var, true, edges, &n))) {
        fail = "can't solve this one yet";
    } else {
        qsort(edges, (size_t)n, sizeof *edges, by_x);
        for(i = 0; i <= n && fail == NULL; i++) {
            int s = sign_at(g, var, test_point(edges, n, i));

            if(s == 2)
                fail = "can't tell the sign in between";
            gap[i] = wanted(s, rel);
        }
        for(i = 0; i < n; i++)
            edge[i] = (rel == CH_LE || rel == CH_GE) && !edges[i].hole;
    }

    /* each run of true bits in a row is one piece of the answer. a run starting
       or ending on a gap has that side open, on an edge closed */
    if(fail == NULL && col == NULL) {
        col = layout_box(BOX_COL, 0);
        for(p = 0; p <= 2 * n; p++) {
            pcas_ast_t *lo = NULL, *hi = NULL;
            int q = p;

            if(!truth(gap, edge, p))
                continue;
            while(q + 1 <= 2 * n && truth(gap, edge, q + 1))
                q++;
            if(p > 0)
                lo = edges[p % 2 == 0 ? p / 2 - 1 : p / 2].at;
            if(q < 2 * n)
                hi = edges[q / 2].at; /* the edge after gap q/2, or edge q/2 itself */

            if(lo == NULL && hi == NULL) {
                layout_free(col);
                col = layout_text("true", 0);
                runs = 1;
                break;
            }
            if(runs++ > 0)
                layout_add(col, layout_text("or", 0));
            layout_add(col, ineq_row(shown, lo, p % 2 == 1, hi, q % 2 == 1));
            p = q;
        }
        if(runs == 0) {
            layout_free(col);
            col = layout_text("false", 0);
        }
    }

    if(col != NULL)
        layout_finish(col, &draw_font);
    push(in, col, fail, (unsigned long)((clock() - start) * 1000 / CLOCKS_PER_SEC));

    for(i = 0; i < n; i++)
        ast_Cleanup(edges[i].at);
    ast_Cleanup(g);
}
