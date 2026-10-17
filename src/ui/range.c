#include "range.h"

#include "app.h"
#include "ineq.h"
#include "numval.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "engine/cas/cas.h"
#include "render/draw.h"

/*
    range(f, x). the engine has no limits, so it goes in pieces. domain() and
    asym() cut the x line wherever f stops being continuous (a bound, a hole, a
    vertical asymptote), and on each piece the y values run between the biggest
    and smallest of what f hits (turning points from f' = 0, closed ends) and
    what it only heads towards (open ends, a hole's missing value, a horizontal
    asymptote, +-infinity). heading towards is found by poking f with numbers
    closer and closer, the values themselves stay exact where they can
*/

#define CUTS_MAX (3 * SOLVE_MAX + 2)
#define YS_MAX (SOLVE_MAX + 2)

enum { CUT_CLOSED, CUT_OPEN, CUT_VERT, CUT_HOLE };

typedef struct {
    pcas_ast_t *at;
    float x;
    int kind;
} cut_t;

/* a y value on a piece. inf is -1 or 1 for -+infinity (then v is NULL), hit
   says f gets there and doesn't just head towards it */
typedef struct {
    pcas_ast_t *v;
    float y;
    int inf;
    bool hit;
} yval_t;

/* the float of an exact value, on a copy so the exact one stays exact */
static bool decimal(pcas_ast_t *v, float *y) {
    pcas_ast_t *d = ast_Copy(v);
    bool ok;

    approx(d);
    simplify(d, SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_RATIONAL | SIMP_EVAL);
    ok = num_float(d, y) && isfinite(*y);
    ast_Cleanup(d);
    return ok;
}

/* f at an exact x, simplified. NULL when it isn't a finite plain number (a hole
   in the original f, use the cancelled one for those) */
static pcas_ast_t *f_exact(pcas_ast_t *f, Symbol var, pcas_ast_t *x, float *y) {
    pcas_ast_t *e = ast_Copy(f), *sym = ast_MakeSymbol(var);

    substitute(e, sym, x);
    simplify(e, SIMP_ALL);
    ast_Cleanup(sym);
    if(!decimal(e, y)) {
        ast_Cleanup(e);
        return NULL;
    }
    return e;
}

/* which way f goes next to x on side s (-1 left, 1 right): 1 or -1 when it
   blows up that way, 0 when it settles, 2 when it can't tell. three pokes, each
   ten times closer. something smooth that settles changes about 10x less each
   poke, so steps that don't fade mean it blows up (ln(x) at 0 only grows by 2.3
   a poke, 1/x by 10x) */
static int blows_up(pcas_ast_t *f, Symbol var, float x, int s) {
    float a = num_at(f, var, x + s * 1e-2F), b = num_at(f, var, x + s * 1e-3F), c = num_at(f, var, x + s * 1e-4F);
    float d1 = fabsf(b) - fabsf(a), d2 = fabsf(c) - fabsf(b);

    if(isnan(a) || isnan(b) || isnan(c))
        return 2;
    if(d1 > 0 && d2 >= d1 / 2 && (c > 0) == (b > 0))
        return c > 0 ? 1 : -1;
    return 0;
}

/* f' = 0, real roots into xs. SOLVE_UNKNOWN when solve gives up */
static int turning(pcas_ast_t *f, Symbol var, pcas_ast_t **xs) {
    pcas_ast_t *d = ast_MakeOperator(OP_DERIV);
    int n;

    ast_ChildAppend(d, ast_Copy(f));
    ast_ChildAppend(d, ast_MakeSymbol(var));
    ast_ChildAppend(d, ast_MakeSymbol(var));
    simplify(d, SIMP_ALL);
    n = solve(d, var, xs);
    ast_Cleanup(d);
    return n;
}

/* an interval of y, lo/hi NULL for -+infinity. the trees are owned */
typedef struct {
    pcas_ast_t *lo, *hi;
    float ylo, yhi;
    bool lo_in, hi_in;
} span_t;

/* what f does at one end of a piece, added to ys. side is which way the piece
   is from the cut (1 = the piece is to the right). x NULL for the far end out at
   -+infinity, far is a number past every cut and turning point on that side.
   false when it can't tell */
static bool end_of(pcas_ast_t *f, pcas_ast_t *flat, Symbol var, const asym_t *a, const cut_t *cut,
                   int side, float far, yval_t *ys, int *n) {
    yval_t *y = &ys[*n];

    y->v = NULL;
    y->inf = 0;
    y->hit = false;
    if(cut == NULL) {
        /* off to infinity when |f| keeps growing and the steps don't fade. log and
           sqrt grow slowly but steadily, 1 - 1/x fades by 10x a step (and has its
           asymptote anyway). doubling, not 10x, so e^x doesn't overflow a float */
        float p = num_at(f, var, far), q = num_at(f, var, 2 * far), r = num_at(f, var, 4 * far);
        float d1 = fabsf(q) - fabsf(p), d2 = fabsf(r) - fabsf(q);

        if(a->horiz != NULL) { /* only heads there out at the end, so hit stays false. a turning point can still land on it */
            if(!decimal(a->horiz, &y->y))
                return false;
            y->v = ast_Copy(a->horiz);
        } else if(!isnan(p) && !isnan(q) && !isnan(r) && d1 > 0 && d2 >= d1 / 2 && (r > 0) == (q > 0)) {
            y->inf = r > 0 ? 1 : -1;
        } else if(!isnan(r) && fabsf(r) < 1e-4F && fabsf(r) < fabsf(q) && fabsf(q) < fabsf(p)) {
            /* dying off to 0, e^x on the left. asym only finds the rational kind of asymptote */
            y->v = ast_MakeNumber(num_FromInt(0));
            y->y = 0;
        } else {
            return false;
        }
    } else if(cut->kind == CUT_CLOSED) {
        if((y->v = f_exact(f, var, cut->at, &y->y)) == NULL)
            return false;
        y->hit = true;
    } else if(cut->kind == CUT_HOLE) {
        /* the cancelled f fills the hole in, that's the value it heads for */
        if((y->v = f_exact(flat, var, cut->at, &y->y)) == NULL)
            return false;
    } else {
        int b = blows_up(f, var, cut->x, side);

        if(b != 1 && b != -1)
            return false; /* an open bound that settles, log(x)/x style. not worth it yet */
        y->inf = b;
    }
    (*n)++;
    return true;
}

/* a below b, with -+infinity at the ends */
static bool below(const yval_t *a, const yval_t *b) {
    if(a->inf != b->inf)
        return a->inf < b->inf;
    return a->inf == 0 && a->y < b->y;
}

static bool same_y(float a, float b) {
    return fabsf(a - b) <= 1e-5F * (fabsf(a) > 1 ? fabsf(a) : 1);
}

/* the y values on the piece between cuts l and r (NULL for out to -+infinity):
   both ends, and the turning points that land inside. it's all one continuous
   stretch, so it hits everything between its lowest and highest. an extreme
   counts as in when something actually hits it, only heading there leaves it
   open. false when an end can't be told */
static bool piece_span(pcas_ast_t *f, pcas_ast_t *flat, Symbol var, const asym_t *a, const cut_t *l,
                       const cut_t *r, pcas_ast_t **xs, const float *xf, int nx, float far, span_t *out) {
    yval_t ys[YS_MAX];
    int n = 0, i, lo = 0, hi = 0;
    bool ok;

    ok = end_of(f, flat, var, a, l, 1, -far, ys, &n) && end_of(f, flat, var, a, r, -1, far, ys, &n);
    for(i = 0; ok && i < nx && n < YS_MAX; i++) {
        if((l != NULL && xf[i] <= l->x) || (r != NULL && xf[i] >= r->x))
            continue;
        ys[n].inf = 0;
        ys[n].hit = true;
        if((ys[n].v = f_exact(f, var, xs[i], &ys[n].y)) == NULL)
            ok = false;
        else
            n++;
    }

    if(ok) {
        for(i = 1; i < n; i++) {
            if(below(&ys[i], &ys[lo]))
                lo = i;
            if(below(&ys[hi], &ys[i]))
                hi = i;
        }
        ok = ys[lo].inf <= 0 && ys[hi].inf >= 0; /* both ends off to the same infinity isn't a piece */
    }
    if(ok) {
        out->lo = ys[lo].inf ? NULL : ast_Copy(ys[lo].v);
        out->hi = ys[hi].inf ? NULL : ast_Copy(ys[hi].v);
        out->ylo = ys[lo].inf ? -INFINITY : ys[lo].y;
        out->yhi = ys[hi].inf ? INFINITY : ys[hi].y;
        out->lo_in = out->hi_in = false;
        for(i = 0; i < n; i++) {
            if(ys[i].inf || !ys[i].hit)
                continue;
            out->lo_in |= out->lo != NULL && same_y(ys[i].y, out->ylo);
            out->hi_in |= out->hi != NULL && same_y(ys[i].y, out->yhi);
        }
    }

    for(i = 0; i < n; i++)
        if(ys[i].v != NULL)
            ast_Cleanup(ys[i].v);
    return ok;
}

static int by_lo(const void *a, const void *b) {
    float x = ((const span_t *)a)->ylo, y = ((const span_t *)b)->ylo;

    return x < y ? -1 : x > y;
}

static void span_free(span_t *s) {
    if(s->lo != NULL)
        ast_Cleanup(s->lo);
    if(s->hi != NULL)
        ast_Cleanup(s->hi);
}

/* the pieces' spans as one answer. two spans that overlap, or touch with the
   point in one of them, become one. 1/x touches at 0 with 0 in neither, so that
   stays y < 0 or y > 0 */
static box_t *union_box(span_t *s, int n, bool partial) {
    box_t *col = layout_box(BOX_COL, 0);
    int i, k = 0;

    qsort(s, (size_t)n, sizeof *s, by_lo);
    for(i = 1; i < n; i++) {
        span_t *cur = &s[k];
        bool meet = cur->hi != NULL && same_y(s[i].ylo, cur->yhi);
        bool overlap = s[i].ylo < cur->yhi && !meet;

        if(overlap || (meet && (cur->hi_in || s[i].lo_in))) {
            if(s[i].hi == NULL || (cur->hi != NULL && s[i].yhi > cur->yhi && !same_y(s[i].yhi, cur->yhi))) {
                if(cur->hi != NULL)
                    ast_Cleanup(cur->hi);
                cur->hi = s[i].hi;
                cur->yhi = s[i].yhi;
                cur->hi_in = s[i].hi_in;
                s[i].hi = NULL;
            } else if(cur->hi != NULL && same_y(s[i].yhi, cur->yhi)) {
                cur->hi_in |= s[i].hi_in;
            }
            if(s[i].lo != NULL && cur->lo != NULL && same_y(s[i].ylo, cur->ylo))
                cur->lo_in |= s[i].lo_in;
            span_free(&s[i]);
        } else {
            s[++k] = s[i];
        }
    }
    n = k + 1;

    for(i = 0; i < n; i++) {
        if(i > 0)
            layout_add(col, layout_text("or", 0));
        if(s[i].lo == NULL && s[i].hi == NULL)
            layout_add(col, layout_text("all reals", 0));
        else if(s[i].lo != NULL && s[i].hi != NULL && same_y(s[i].ylo, s[i].yhi))
            layout_add(col, ineq_row('y', s[i].lo, true, s[i].lo, true)); /* a constant, y = c */
        else
            layout_add(col, ineq_row('y', s[i].lo, s[i].lo_in, s[i].hi, s[i].hi_in));
    }
    if(partial)
        layout_add(col, layout_text("(and maybe more)", 0));
    for(i = 0; i < n; i++)
        span_free(&s[i]);
    layout_finish(col, &draw_font);
    return col;
}

static int by_cut(const void *a, const void *b) {
    float x = ((const cut_t *)a)->x, y = ((const cut_t *)b)->x;

    return x < y ? -1 : x > y;
}

/* everything range_run needs worked out, NULL when it went fine, a message when not */
static const char *work_out(pcas_ast_t *f, Symbol var, box_t **out) {
    pcas_ast_t *xs[SOLVE_MAX] = { NULL }, *flat = NULL;
    cut_t cuts[CUTS_MAX], *ends[CUTS_MAX + 2];
    span_t spans[CUTS_MAX + 1];
    float xf[SOLVE_MAX], far = 10;
    const char *fail = NULL;
    domain_t d;
    asym_t a;
    int nc = 0, nx, ns = 0, ne = 0, i, j;
    bool have_a;

    if(!has_symbol(f, var)) { /* range(3), just y = 3 */
        spans[0].lo = ast_Copy(f);
        spans[0].hi = ast_Copy(f);
        if(!decimal(f, &spans[0].ylo)) {
            span_free(&spans[0]);
            return "that isn't a real number";
        }
        spans[0].yhi = spans[0].ylo;
        spans[0].lo_in = spans[0].hi_in = true;
        *out = union_box(spans, 1, false);
        return NULL;
    }
    if(!domain(f, var, &d))
        return "nothing's defined there";
    if(d.empty) {
        domain_free(&d);
        return "it's not defined anywhere";
    }
    have_a = asym(f, var, &a);
    if(!have_a)
        memset(&a, 0, sizeof a);

    for(i = 0; i < d.nholes && fail == NULL; i++) {
        cuts[nc].at = d.holes[i];
        cuts[nc].kind = CUT_HOLE;
        if(!decimal(d.holes[i], &cuts[nc].x))
            fail = "can't do the periodic ones yet";
        for(j = 0; j < a.nvert && fail == NULL; j++) {
            float v;

            if(decimal(a.vert[j], &v) && same_y(v, cuts[nc].x))
                cuts[nc].kind = CUT_VERT;
        }
        nc++;
    }
    qsort(cuts, (size_t)nc, sizeof *cuts, by_cut);

    nx = fail == NULL ? turning(f, var, xs) : 0;
    if(nx == SOLVE_UNKNOWN)
        fail = "can't find where it turns around";
    if(nx < 0)
        nx = 0;
    for(i = 0; i < nx && fail == NULL; i++)
        if(!decimal(xs[i], &xf[i]))
            fail = "can't do the periodic ones yet";

    /* the ends of each piece, in order: the lower bound (or NULL), the holes, the upper bound */
    if(fail == NULL) {
        static cut_t lower, upper;

        if(d.lower != NULL) {
            lower.at = d.lower;
            lower.kind = d.lower_open ? CUT_OPEN : CUT_CLOSED;
            if(!decimal(d.lower, &lower.x))
                fail = "can't tell where the domain starts";
        }
        if(d.upper != NULL) {
            upper.at = d.upper;
            upper.kind = d.upper_open ? CUT_OPEN : CUT_CLOSED;
            if(!decimal(d.upper, &upper.x))
                fail = "can't tell where the domain ends";
        }
        ends[ne++] = d.lower != NULL ? &lower : NULL;
        for(i = 0; i < nc; i++)
            ends[ne++] = &cuts[i];
        ends[ne++] = d.upper != NULL ? &upper : NULL;
    }

    /* far enough out that nothing interesting is left past it */
    for(i = 0; i < nc; i++)
        far = fmaxf(far, 2 * fabsf(cuts[i].x) + 10);
    for(i = 0; i < nx; i++)
        far = fmaxf(far, 2 * fabsf(xf[i]) + 10);

    if(fail == NULL) {
        flat = ast_Copy(f);
        simplify(flat, SIMP_ALL);
        comdenom(flat, true);
    }
    for(i = 0; i + 1 < ne && fail == NULL; i++)
        if(piece_span(f, flat, var, &a, ends[i], ends[i + 1], xs, xf, nx, far, &spans[ns]))
            ns++;
        else
            fail = "can't work out what it heads towards";

    if(fail == NULL)
        *out = union_box(spans, ns, d.partial);
    else
        for(i = 0; i < ns; i++)
            span_free(&spans[i]);

    if(flat != NULL)
        ast_Cleanup(flat);
    for(i = 0; i < nx; i++)
        ast_Cleanup(xs[i]);
    if(have_a)
        asym_free(&a);
    domain_free(&d);
    return fail;
}

bool range_run(const char *cmd, char *src, box_t *in) {
    pcas_ast_t *f;
    Symbol var;
    char shown;
    const char *fail = NULL;
    box_t *out = NULL;
    clock_t start;

    if(cmd == NULL || strcmp(cmd, "range") != 0)
        return false;
    f = read_f(src, in, "range(expression, variable)", &var, &shown);
    if(f == NULL)
        return true;

    start = clock();
    simplify(f, SIMP_ALL);
    if(pcas_cancelled)
        fail = pcas_oom ? "out of memory" : "cancelled";
    else
        fail = work_out(f, var, &out);
    if(fail == NULL && pcas_cancelled) {
        layout_free(out);
        out = NULL;
        fail = pcas_oom ? "out of memory" : "cancelled";
    }
    push(in, out, fail, (unsigned long)((clock() - start) * 1000 / CLOCKS_PER_SEC));
    ast_Cleanup(f);
    return true;
}
