#include "conic.h"

#include "app.h"

#include <string.h>
#include <time.h>

#include "engine/ast.h"
#include "engine/parser.h"
#include "engine/cas/cas.h"
#include "render/draw.h"

/*
    ellipses and hyperbolas, only the ones lined up with the axes. an equation
    in gives the center, vertices and foci (and the asymptotes), the center plus
    a vertex and a focus gives the equation. run by the app like geom.c, the
    engine only gets asked for + - * / and sqrt.

    the equation gets read as A x^2 + B xy + C y^2 + D x + E y + F by putting
    small numbers in for x and y, a quadratic in two letters only has those 6
    numbers in it. first a walk over the tree makes sure it is one (degree 2 at
    most, plain numbers in front). the reading is done in imath fractions and not
    with simplify, because simplify has (1+4/9)/2+1/9 as 119/18 right now and
    that's exactly the shape the averages come out in. completing the square
    from there is the usual h = -D/2A, k = -E/2C
*/

#define ARGS_MAX 6

#define SIMP_QUICK (SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_RATIONAL | SIMP_EVAL | SIMP_LIKE_TERMS)

/* these all take what they're given, so ast_Copy anything that's needed again */
static pcas_ast_t *num(int n) {
    return ast_MakeNumber(num_FromInt(n));
}

static pcas_ast_t *add(pcas_ast_t *a, pcas_ast_t *b) {
    return ast_MakeBinary(OP_ADD, a, b);
}

static pcas_ast_t *mul(pcas_ast_t *a, pcas_ast_t *b) {
    return ast_MakeBinary(OP_MULT, a, b);
}

static pcas_ast_t *over(pcas_ast_t *a, pcas_ast_t *b) {
    return ast_MakeBinary(OP_DIV, a, b);
}

/* no subtract node, a plus the negative */
static pcas_ast_t *sub(pcas_ast_t *a, pcas_ast_t *b) {
    return add(a, mul(num(-1), b));
}

static pcas_ast_t *root(pcas_ast_t *e) {
    return ast_MakeBinary(OP_ROOT, num(2), e);
}

static pcas_ast_t *simp(pcas_ast_t *e) {
    if(e != NULL && !pcas_cancelled)
        simplify(e, SIMP_ALL);
    return e;
}

static bool zero(pcas_ast_t *e) {
    return e != NULL && is_ast_int(e, 0);
}

/* a simplified number is still a/b as two nodes, so through * and / too */
static int sign_in(pcas_ast_t *e) {
    pcas_ast_t *k;
    int s = 1, ks;

    if(e->type == NODE_NUMBER) {
        s = mp_rat_compare_zero(e->op.num);
        return s < 0 ? -1 : s > 0;
    }
    if(!isoptype(e, OP_MULT) && !isoptype(e, OP_DIV))
        return 2;
    for(k = ast_ChildGet(e, 0); k != NULL; k = k->next) {
        ks = sign_in(k);
        if(ks == 2)
            return 2;
        s *= ks;
    }
    return s;
}

/* -1, 0 or 1, or 2 when there's a letter in it and it can't say. takes e. the
   approx is for the odd sqrt */
static int sign_of(pcas_ast_t *e) {
    int s = 2;

    if(e != NULL && !pcas_cancelled) {
        simplify(e, SIMP_ALL);
        approx(e);
        simplify(e, SIMP_QUICK);
        s = sign_in(e);
    }
    ast_Cleanup(e);
    return s;
}

/* cut at the top level commas like geom's read_args. how many pieces, 0 for too many */
static int split(char *src, char **piece) {
    int depth = 0, pieces = 1;
    char *s;

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

static pcas_ast_t *read_piece(const char *s, pcas_error_t *err) {
    pcas_ast_t *e = parse((const uint8_t *)s, strlen(s), str_table, err);

    if(*err != E_SUCCESS || e == NULL) {
        if(*err == E_SUCCESS)
            *err = E_GENERIC;
        ast_Cleanup(e);
        return NULL;
    }
    return e;
}

/* ( a, b ) like geom's, except it takes them */
static box_t *point_box(pcas_ast_t *x, pcas_ast_t *y) {
    box_t *row = layout_box(BOX_ROW, 0);

    layout_add(row, layout_text("(", 0));
    layout_add(row, value_box(x));
    layout_add(row, layout_text(",", 0));
    layout_add(row, layout_space(2 * SPACE_BINARY));
    layout_add(row, value_box(y));
    layout_add(row, layout_text(")", 0));
    layout_finish(row, &draw_font);
    ast_Cleanup(x);
    ast_Cleanup(y);
    return row;
}

/* the two points d either side of the center, left-right when across, else up-down */
static box_t *pair_box(pcas_ast_t *h, pcas_ast_t *k, pcas_ast_t *d, bool across) {
    box_t *row = layout_box(BOX_ROW, 0);
    int i;

    for(i = 0; i < 2; i++) {
        pcas_ast_t *off = i == 0 ? mul(num(-1), ast_Copy(d)) : ast_Copy(d);

        if(i > 0) {
            layout_add(row, layout_text(",", 0));
            layout_add(row, layout_space(2 * SPACE_BINARY));
        }
        if(across)
            layout_add(row, point_box(simp(add(ast_Copy(h), off)), ast_Copy(k)));
        else
            layout_add(row, point_box(ast_Copy(h), simp(add(ast_Copy(k), off))));
    }
    layout_finish(row, &draw_font);
    return row;
}

static bool is_xy(const pcas_ast_t *e) {
    return e->type == NODE_SYMBOL && (e->op.symbol == SYM_X || e->op.symbol == SYM_Y);
}

/* how many x's and y's multiplied together at most, -1 when it isn't a
   polynomial with plain fractions in front. any other letter (pi and e too)
   sets other */
static int degree(pcas_ast_t *e, bool *other) {
    pcas_ast_t *k, *exp;
    mp_small n, d;
    int deg = 0, kd;

    if(e->type == NODE_NUMBER)
        return 0;
    if(e->type == NODE_SYMBOL) {
        if(is_xy(e))
            return 1;
        *other = true;
        return -1;
    }
    if(isoptype(e, OP_ADD) || isoptype(e, OP_MULT)) {
        for(k = ast_ChildGet(e, 0); k != NULL; k = k->next) {
            kd = degree(k, other);
            if(kd < 0)
                return -1;
            deg = isoptype(e, OP_ADD) ? (kd > deg ? kd : deg) : deg + kd;
        }
        return deg;
    }
    if(isoptype(e, OP_DIV)) {
        deg = degree(ast_ChildGet(e, 0), other);
        return deg >= 0 && degree(ast_ChildGet(e, 1), other) == 0 ? deg : -1;
    }
    /* whole number powers only. a negative one is fine on a plain number */
    if(isoptype(e, OP_POW)) {
        exp = ast_ChildGet(e, 1);
        if(exp->type != NODE_NUMBER || !mp_rat_is_integer(exp->op.num)
           || mp_rat_to_ints(exp->op.num, &n, &d) != MP_OK || n > 8 || n < -8)
            return -1;
        deg = degree(ast_ChildGet(e, 0), other);
        if(deg < 0 || (n < 0 && deg > 0))
            return -1;
        return deg * (int)n;
    }
    return -1;
}

/* e with x and y put in, exactly, into r. only for trees degree() was happy
   with. false for a divide by 0 (or out of memory) */
static bool value_at(pcas_ast_t *e, mp_rat r, int x, int y) {
    pcas_ast_t *k;
    mp_small n, d;
    mp_rat t;
    bool ok;

    if(e->type == NODE_NUMBER)
        return mp_rat_copy(e->op.num, r) == MP_OK;
    if(e->type == NODE_SYMBOL)
        return mp_rat_set_value(r, e->op.symbol == SYM_X ? x : y, 1) == MP_OK;
    if(isoptype(e, OP_POW)) {
        mp_rat_to_ints(ast_ChildGet(e, 1)->op.num, &n, &d);
        if(!value_at(ast_ChildGet(e, 0), r, x, y))
            return false;
        if(n < 0 && mp_rat_compare_zero(r) == 0)
            return false;
        if(mp_rat_expt(r, n < 0 ? -n : n, r) != MP_OK)
            return false;
        return n >= 0 || mp_rat_recip(r, r) == MP_OK;
    }

    k = ast_ChildGet(e, 0);
    if(!value_at(k, r, x, y))
        return false;
    t = num_FromInt(0);
    if(t == NULL)
        return false;
    for(ok = true, k = k->next; k != NULL && ok; k = k->next) {
        ok = value_at(k, t, x, y);
        if(!ok)
            break;
        if(isoptype(e, OP_ADD))
            ok = mp_rat_add(r, t, r) == MP_OK;
        else if(isoptype(e, OP_MULT))
            ok = mp_rat_mul(r, t, r) == MP_OK;
        else
            ok = mp_rat_compare_zero(t) != 0 && mp_rat_div(r, t, r) == MP_OK;
    }
    num_Cleanup(t);
    return ok;
}

/* simplified, so it has the shape the rest of the engine expects (a/b, not one odd number) */
static pcas_ast_t *rat_ast(mp_rat q) {
    return simp(ast_MakeNumber(num_Copy(q)));
}

enum { V_00, V_10, V_M10, V_01, V_0M1, V_11, Q_A, Q_B, Q_C, Q_D, Q_E, Q_H, Q_K, Q_R, Q_T, Q_COUNT };

/*
    p = 0 as (x-h)^2/xx + (y-k)^2/yy = 1, xx and yy can come out negative (that's
    the hyperbola). NULL when it worked, the message otherwise and then nothing
    is handed back. signs gets xx's sign, yy's, and xx compared to yy, worked
    out here while they're still plain fractions
*/
static const char *standard(pcas_ast_t *p, pcas_ast_t **h, pcas_ast_t **k, pcas_ast_t **xx, pcas_ast_t **yy,
                            int *signs) {
    /* where p gets looked at: the middle, x = 1 and -1, y = 1 and -1, and both 1 for the xy */
    static const signed char spots[6][2] = { { 0, 0 }, { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 }, { 1, 1 } };
    mp_rat q[Q_COUNT];
    const char *message = NULL;
    bool other = false, ok = true;
    int i, deg, sa, sc;

    *h = *k = *xx = *yy = NULL;
    deg = degree(p, &other);
    if(other)
        return "numbers only, no other letters";
    if(deg < 0 || deg > 2)
        return "only x^2, y^2, x, y and numbers in it";

    for(i = 0; i < Q_COUNT; i++) {
        q[i] = num_FromInt(0);
        ok = ok && q[i] != NULL;
    }
    for(i = 0; i < 6 && ok; i++)
        ok = value_at(p, q[i], spots[i][0], spots[i][1]);

    /* f(1) + f(-1) = 2A + 2F and f(1) - f(-1) = 2D, the same for y. the xy one is
       what's left of f(1, 1) once the x and y parts are taken off */
    if(ok) {
        mp_rat_add(q[V_10], q[V_M10], q[Q_A]);
        mp_rat_set_value(q[Q_T], 1, 2);
        mp_rat_mul(q[Q_A], q[Q_T], q[Q_A]);
        mp_rat_sub(q[Q_A], q[V_00], q[Q_A]);
        mp_rat_sub(q[V_10], q[V_M10], q[Q_D]);
        mp_rat_mul(q[Q_D], q[Q_T], q[Q_D]);
        mp_rat_add(q[V_01], q[V_0M1], q[Q_C]);
        mp_rat_mul(q[Q_C], q[Q_T], q[Q_C]);
        mp_rat_sub(q[Q_C], q[V_00], q[Q_C]);
        mp_rat_sub(q[V_01], q[V_0M1], q[Q_E]);
        mp_rat_mul(q[Q_E], q[Q_T], q[Q_E]);
        mp_rat_sub(q[V_11], q[V_10], q[Q_B]);
        mp_rat_sub(q[Q_B], q[V_01], q[Q_B]);
        mp_rat_add(q[Q_B], q[V_00], q[Q_B]);
    }

    sa = ok ? mp_rat_compare_zero(q[Q_A]) : 0;
    sc = ok ? mp_rat_compare_zero(q[Q_C]) : 0;
    if(!ok)
        message = pcas_cancelled ? "out of memory" : "there's a divide by 0 in it";
    else if(mp_rat_compare_zero(q[Q_B]) != 0)
        message = "has an xy, only ones lined up with the axes";
    else if(sa == 0 || sc == 0)
        message = "it needs an x^2 and a y^2";
    else {
        /* h = -D/2A, k = -E/2C, then A(x-h)^2 + C(y-k)^2 = r with r = A h^2 + C k^2 - F.
           T is the -1/2 and then the scratch for the squares */
        mp_rat_set_value(q[Q_T], -1, 2);
        mp_rat_mul(q[Q_D], q[Q_T], q[Q_H]);
        mp_rat_div(q[Q_H], q[Q_A], q[Q_H]);
        mp_rat_mul(q[Q_E], q[Q_T], q[Q_K]);
        mp_rat_div(q[Q_K], q[Q_C], q[Q_K]);
        mp_rat_mul(q[Q_H], q[Q_H], q[Q_T]);
        mp_rat_mul(q[Q_T], q[Q_A], q[Q_R]);
        mp_rat_mul(q[Q_K], q[Q_K], q[Q_T]);
        mp_rat_mul(q[Q_T], q[Q_C], q[Q_T]);
        mp_rat_add(q[Q_R], q[Q_T], q[Q_R]);
        mp_rat_sub(q[Q_R], q[V_00], q[Q_R]);
        if(mp_rat_compare_zero(q[Q_R]) == 0)
            message = (sa > 0) == (sc > 0) ? "that's just one point" : "that's two lines crossing";
        else {
            /* D and E are done with, they hold xx and yy now */
            mp_rat_div(q[Q_R], q[Q_A], q[Q_D]);
            mp_rat_div(q[Q_R], q[Q_C], q[Q_E]);
            signs[0] = mp_rat_compare_zero(q[Q_D]);
            signs[1] = mp_rat_compare_zero(q[Q_E]);
            signs[2] = mp_rat_compare(q[Q_D], q[Q_E]);
            *h = rat_ast(q[Q_H]);
            *k = rat_ast(q[Q_K]);
            *xx = rat_ast(q[Q_D]);
            *yy = rat_ast(q[Q_E]);
        }
    }

    for(i = 0; i < Q_COUNT; i++)
        num_Cleanup(q[i]);
    if(message == NULL && pcas_cancelled)
        message = "out of memory";
    if(message != NULL) {
        ast_Cleanup(*h);
        ast_Cleanup(*k);
        ast_Cleanup(*xx);
        ast_Cleanup(*yy);
        *h = *k = *xx = *yy = NULL;
    }
    return message;
}

/* equation in, the column of center / vertices / ... out */
static const char *from_equation(bool hyper, pcas_ast_t *lhs, pcas_ast_t *rhs, box_t **out) {
    pcas_ast_t *p, *h, *k, *xx, *yy, *a2, *b2, *a, *b, *c, *m, *lines[2];
    const char *message;
    bool across;
    int signs[3], sx, sy;
    box_t *col;

    p = sub(ast_Copy(lhs), ast_Copy(rhs));
    message = pcas_cancelled ? "out of memory" : standard(p, &h, &k, &xx, &yy, signs);
    ast_Cleanup(p);
    if(message != NULL)
        return message;

    sx = signs[0] < 0 ? -1 : 1;
    sy = signs[1] < 0 ? -1 : 1;
    if(!hyper && sx != sy)
        message = "x^2 and y^2 have opposite signs, a hyperbola";
    else if(!hyper && sx < 0)
        message = "no points, squares can't add up to less than 0";
    else if(hyper && sx == sy)
        message = "x^2 and y^2 have the same sign, not a hyperbola";
    if(message != NULL) {
        ast_Cleanup(h);
        ast_Cleanup(k);
        ast_Cleanup(xx);
        ast_Cleanup(yy);
        return message;
    }

    /* across = the long way (or the way it opens) is left-right */
    across = hyper ? sx > 0 : signs[2] >= 0;
    a2 = across ? xx : yy;
    b2 = across ? yy : xx;
    if(hyper)
        b2 = simp(mul(num(-1), b2));
    a = simp(root(ast_Copy(a2)));
    b = simp(root(ast_Copy(b2)));
    /* c^2 = a^2 - b^2 for the ellipse, a^2 + b^2 for the hyperbola */
    c = simp(root(hyper ? add(ast_Copy(a2), ast_Copy(b2)) : sub(ast_Copy(a2), ast_Copy(b2))));

    col = layout_box(BOX_COL, 0);
    layout_add(col, labelled("center", point_box(ast_Copy(h), ast_Copy(k))));
    layout_add(col, labelled("vertices", pair_box(h, k, a, across)));
    if(!hyper)
        layout_add(col, labelled("co-vertices", pair_box(h, k, b, !across)));
    /* a circle, both foci sit on the center */
    if(zero(c))
        layout_add(col, labelled("focus", point_box(ast_Copy(h), ast_Copy(k))));
    else
        layout_add(col, labelled("foci", pair_box(h, k, c, across)));
    if(hyper) {
        /* y = k +- m(x - h), rise over run is b/a opening sideways and a/b up-down */
        m = simp(across ? over(ast_Copy(b), ast_Copy(a)) : over(ast_Copy(a), ast_Copy(b)));
        lines[0] = simp(add(ast_Copy(k), mul(ast_Copy(m), sub(ast_MakeSymbol(SYM_X), ast_Copy(h)))));
        lines[1] = simp(sub(ast_Copy(k), mul(m, sub(ast_MakeSymbol(SYM_X), ast_Copy(h)))));
        layout_add(col, labelled("asymptotes", solutions_box('y', lines, 2)));
        ast_Cleanup(lines[0]);
        ast_Cleanup(lines[1]);
    }
    layout_finish(col, &draw_font);
    *out = col;

    ast_Cleanup(h);
    ast_Cleanup(k);
    ast_Cleanup(a2);
    ast_Cleanup(b2);
    ast_Cleanup(a);
    ast_Cleanup(b);
    ast_Cleanup(c);
    return NULL;
}

/* (x - h)^2/under, built by hand so simplify can't multiply it out. no h when
   it's 0 and no bottom when it's 1 */
static box_t *term_box(Symbol s, pcas_ast_t *h, pcas_ast_t *under) {
    pcas_ast_t *e = ast_MakeSymbol(s);
    box_t *b;

    if(!zero(h)) {
        e = sub(e, ast_Copy(h));
        if(e != NULL && !pcas_cancelled)
            simplify(e, SIMP_QUICK);
    }
    e = ast_MakeBinary(OP_POW, e, num(2));
    /* under 25/4 reads better as 4(x-h)^2/25 than a fraction under a fraction */
    if(isoptype(under, OP_DIV) && ast_ChildGet(under, 0)->type == NODE_NUMBER
       && ast_ChildGet(under, 1)->type == NODE_NUMBER)
        e = over(mul(ast_Copy(ast_ChildGet(under, 1)), e), ast_Copy(ast_ChildGet(under, 0)));
    else if(!is_ast_int(under, 1))
        e = over(e, ast_Copy(under));
    b = pcas_cancelled ? NULL : layout_build(e, &draw_font);
    ast_Cleanup(e);
    return b;
}

/* center, a vertex and a focus in, the equation out */
static const char *from_points(bool hyper, pcas_ast_t **p, box_t **out) {
    pcas_ast_t *dvx, *dvy, *dfx, *dfy, *a2, *c2, *b2;
    const char *message = NULL;
    bool across = false;
    char minus[2] = { CH_MINUS, '\0' };
    box_t *row, *first, *second;
    int sb;

    /* p is h, k, vx, vy, fx, fy */
    dvx = simp(sub(ast_Copy(p[2]), ast_Copy(p[0])));
    dvy = simp(sub(ast_Copy(p[3]), ast_Copy(p[1])));
    dfx = simp(sub(ast_Copy(p[4]), ast_Copy(p[0])));
    dfy = simp(sub(ast_Copy(p[5]), ast_Copy(p[1])));

    if(pcas_cancelled)
        message = "out of memory";
    else if(zero(dvx) && zero(dvy))
        message = "the vertex can't be the center";
    else if(!zero(dvx) && !zero(dvy))
        message = "the vertex has to line up with the center";
    else {
        across = zero(dvy);
        if(!zero(across ? dfy : dfx))
            message = "the focus has to line up with the vertex";
    }
    if(message != NULL) {
        ast_Cleanup(dvx);
        ast_Cleanup(dvy);
        ast_Cleanup(dfx);
        ast_Cleanup(dfy);
        return message;
    }

    a2 = simp(ast_MakeBinary(OP_POW, ast_Copy(across ? dvx : dvy), num(2)));
    c2 = simp(ast_MakeBinary(OP_POW, ast_Copy(across ? dfx : dfy), num(2)));
    b2 = simp(hyper ? sub(ast_Copy(c2), ast_Copy(a2)) : sub(ast_Copy(a2), ast_Copy(c2)));
    ast_Cleanup(dvx);
    ast_Cleanup(dvy);
    ast_Cleanup(dfx);
    ast_Cleanup(dfy);

    /* letters in it, it can't tell, so it just goes ahead */
    sb = sign_of(ast_Copy(b2));
    if(pcas_cancelled)
        message = "out of memory";
    else if(sb == 0 || sb == -1)
        message = hyper ? "the focus has to be further out than the vertex"
                        : "the focus has to be closer in than the vertex";
    if(message != NULL) {
        ast_Cleanup(a2);
        ast_Cleanup(b2);
        ast_Cleanup(c2);
        return message;
    }

    /* the ellipse is always x then y, the hyperbola starts with the way it opens */
    if(!hyper || across) {
        first = term_box(SYM_X, p[0], across ? a2 : b2);
        second = term_box(SYM_Y, p[1], across ? b2 : a2);
    } else {
        first = term_box(SYM_Y, p[1], a2);
        second = term_box(SYM_X, p[0], b2);
    }

    row = layout_box(BOX_ROW, 0);
    layout_add(row, first);
    layout_add(row, layout_space(LAYOUT_SPACE(0)));
    layout_add(row, layout_text(hyper ? minus : "+", 0));
    layout_add(row, layout_space(LAYOUT_SPACE(0)));
    layout_add(row, second);
    layout_add(row, layout_space(LAYOUT_SPACE(0)));
    layout_add(row, layout_text("=", 0));
    layout_add(row, layout_space(LAYOUT_SPACE(0)));
    layout_add(row, layout_text("1", 0));
    layout_finish(row, &draw_font);
    *out = row;

    ast_Cleanup(a2);
    ast_Cleanup(b2);
    ast_Cleanup(c2);
    return NULL;
}

bool conic_run(const char *cmd, char *src, box_t *in) {
    pcas_ast_t *a[ARGS_MAX] = { NULL };
    pcas_error_t err = E_SUCCESS;
    const char *message = NULL;
    char *piece[ARGS_MAX], *eq = NULL, *s;
    box_t *out = NULL;
    clock_t start;
    bool hyper;
    int n, i, depth = 0;

    if(cmd == NULL)
        return false;
    if(strcmp(cmd, "ellipse") == 0)
        hyper = false;
    else if(strcmp(cmd, "hyperbola") == 0)
        hyper = true;
    else
        return false;

    /* an = means the equation one, else it has to be the 6 numbers */
    n = split(src, piece);
    if(n == 1) {
        for(s = src; *s != '\0' && eq == NULL; s++) {
            if(*s == '(')
                depth++;
            else if(*s == ')')
                depth--;
            else if(depth == 0 && *s == '=')
                eq = s;
        }
    }
    if(eq != NULL) {
        *eq = '\0';
        a[0] = read_piece(src, &err);
        if(a[0] != NULL)
            a[1] = read_piece(eq + 1, &err);
    } else if(n == ARGS_MAX) {
        for(i = 0; i < n && err == E_SUCCESS; i++)
            a[i] = read_piece(piece[i], &err);
    }
    if(err != E_SUCCESS || (eq == NULL && n != ARGS_MAX)) {
        for(i = 0; i < ARGS_MAX; i++)
            ast_Cleanup(a[i]);
        push(in, NULL, err != E_SUCCESS ? error_text[err]
                       : hyper ? "hyperbola(equation) or (h, k, vx, vy, fx, fy)"
                       : "ellipse(equation) or (h, k, vx, vy, fx, fy)", 0);
        return true;
    }

    start = clock();
    if(eq != NULL)
        message = from_equation(hyper, a[0], a[1], &out);
    else
        message = from_points(hyper, a, &out);
    if(pcas_cancelled) {
        layout_free(out);
        out = NULL;
        message = pcas_oom ? "out of memory" : "cancelled";
    }
    push(in, out, message, (unsigned long)((clock() - start) * 1000 / CLOCKS_PER_SEC));

    for(i = 0; i < ARGS_MAX; i++)
        ast_Cleanup(a[i]);
    return true;
}
