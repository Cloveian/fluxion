#include "tri.h"

#include "app.h"
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
    triangle(, amplitude(, pand( and por(. the line gets cut at the top level
    commas like in geom.c and each piece is parsed on its own. the triangle one
    is plain float maths, it's a numbers tool and nobody wants acos(11/14) as an
    answer. the other three stay exact
*/

#define ARGS_MAX 6

/* float like approx.c, the CE's double is a float anyway so the PC gets the same digits */
#define PI 3.14159265358979323846F
#define EPS 1e-5F

enum { T_TRIANGLE, T_AMPLITUDE, T_PAND, T_POR, T_COUNT };

static const struct {
    const char *name, *usage;
    int least, most;
} cmds[T_COUNT] = {
    { "triangle",  "triangle(a, b, c, ab, bc, ca), empty = unknown", 6, 6 },
    { "amplitude", "amplitude(f, x)", 1, 2 },
    { "pand",      "pand(pa, pb)", 2, 2 },
    { "por",       "por(pa, pb)", 2, 2 },
};

/* cut at the top level commas, empty pieces kept (triangle(3,4,5,,,) needs
   them). 0 when there are more than ARGS_MAX */
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

/* NULL and err set on a parse error. an empty piece counts as one too */
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
    return e;
}

/* the variable for amplitude(f, x). one letter or theta, like read_var in app.c */
static bool read_var(const char *s, Symbol *var) {
    if(strcmp(s, "theta") == 0) {
        *var = SYM_THETA;
        return true;
    }
    if(s[0] >= 'A' && s[0] <= 'Z' && s[1] == '\0') {
        *var = (Symbol)s[0];
        return true;
    }
    return false;
}

static bool uses(const pcas_ast_t *e, Symbol var) {
    const pcas_ast_t *k;

    if(e->type == NODE_SYMBOL)
        return e->op.symbol == var;
    if(e->type != NODE_OPERATOR)
        return false;
    for(k = e->op.operator.base; k != NULL; k = k->next)
        if(uses(k, var))
            return true;
    return false;
}

/* triangle */

static float int_value(mp_int z) {
    float v = 0;
    mp_size i;

    for(i = MP_USED(z); i > 0; i--)
        v = ldexpf(v, (int)MP_DIGIT_BIT) + (float)MP_DIGITS(z)[i - 1];
    return MP_SIGN(z) == MP_NEG ? -v : v;
}

/* what's left after approx: numbers, and simplify likes 5.01 as 501/100 in a
   DIV node, so the + * / of them too */
static bool float_of(const pcas_ast_t *e, float *x) {
    const pcas_ast_t *k;
    float y;

    if(e->type == NODE_NUMBER) {
        *x = int_value(mp_rat_numer_ref(e->op.num)) / int_value(mp_rat_denom_ref(e->op.num));
        return true;
    }
    if(!isoptype(e, OP_ADD) && !isoptype(e, OP_MULT) && !isoptype(e, OP_DIV))
        return false;
    for(k = e->op.operator.base; k != NULL; k = k->next) {
        if(!float_of(k, &y))
            return false;
        if(k == e->op.operator.base)
            *x = y;
        else
            *x = isoptype(e, OP_ADD) ? *x + y : isoptype(e, OP_MULT) ? *x * y : *x / y;
    }
    return true;
}

/* a piece as a plain number. pi/3 and sqrt(2) are fine, x isn't */
static bool number_of(const char *s, float *x, pcas_error_t *err) {
    pcas_ast_t *e = read_piece(s, err);
    bool ok;

    if(e == NULL)
        return false;
    if(setting_angle == ANGLE_DEGREES)
        e = degrees_in(e);
    simplify(e, SIMP_ALL);
    approx(e);
    ok = float_of(e, x) && isfinite(*x);
    ast_Cleanup(e);
    return ok;
}

/* x the way the decimal form writes it, 6 digits. through a fraction so it's
   approx_format and not a printf the CE doesn't have room for */
static void format(float x, char *buf) {
    mp_rat r = num_FromInt(0);
    int k;

    if(x != 0) {
        mp_int_set_value(mp_rat_numer_ref(r), (long)ldexpf(frexpf(x, &k), 24));
        k -= 24;
        if(k >= 0)
            mp_int_mul_pow2(mp_rat_numer_ref(r), k, mp_rat_numer_ref(r));
        else
            mp_int_mul_pow2(mp_rat_denom_ref(r), -k, mp_rat_denom_ref(r));
        mp_rat_reduce(r);
    }
    approx_format(r, 6, buf);
    num_Cleanup(r);
}

/* the angles that are still 0, from the three sides (cosine rule) */
static void angles_from_sides(const float *s, float *t) {
    int i;

    for(i = 0; i < 3; i++) {
        float a = s[(i + 1) % 3], b = s[(i + 2) % 3], c;

        if(t[i] != 0)
            continue;
        c = (a * a + b * b - s[i] * s[i]) / (2 * a * b);
        /* 3,4,5 lands a hair past 0 or 1 sometimes */
        c = c > 1 ? 1 : c < -1 ? -1 : c;
        t[i] = acosf(c);
    }
}

/* every side from one, once all three angles are there (sine rule) */
static void sides_from_angles(float *s, const float *t) {
    int i, k;

    for(k = 0; s[k] == 0; k++)
        ;
    for(i = 0; i < 3; i++)
        if(s[i] == 0)
            s[i] = s[k] * sinf(t[i]) / sinf(t[k]);
}

/*
    s are the sides, t[i] the angle across from s[i], radians, 0 = unknown.
    exactly 3 known with a side among them, the caller checked. fills in the
    rest and says how many triangles fit (the ambiguous ssa one can be 2, the
    other one goes in s2 t2), 0 with msg when none do
*/
static int solve_tri(float *s, float *t, float *s2, float *t2, const char **msg) {
    int ns = 0, na = 0, i, j, k, n = 0;
    float h, tk[2];

    for(i = 0; i < 3; i++) {
        ns += s[i] != 0;
        na += t[i] != 0;
    }

    /* aas / asa: the third angle is what's left of 180, then the sine rule */
    if(na == 2) {
        for(i = 0; t[i] != 0; i++)
            ;
        t[i] = PI - t[(i + 1) % 3] - t[(i + 2) % 3];
        sides_from_angles(s, t);
        return 1;
    }

    if(ns == 3) {
        for(i = 0; i < 3; i++)
            if(s[i] >= s[(i + 1) % 3] + s[(i + 2) % 3]) {
                *msg = "those sides don't close up, one's too long";
                return 0;
            }
        angles_from_sides(s, t);
        return 1;
    }

    /* two sides and an angle. i is the missing side */
    for(i = 0; s[i] != 0; i++)
        ;
    j = (i + 1) % 3;
    k = (i + 2) % 3;

    /* sas, the angle's between them, so the cosine rule straight away */
    if(t[i] != 0) {
        s[i] = sqrtf(s[j] * s[j] + s[k] * s[k] - 2 * s[j] * s[k] * cosf(t[i]));
        angles_from_sides(s, t);
        return 1;
    }

    /* ssa. j is the side with its angle known, so swap if it's the other way */
    if(t[j] == 0) {
        int swap = j;

        j = k;
        k = swap;
    }
    h = s[k] * sinf(t[j]) / s[j];
    if(h > 1 + EPS) {
        *msg = "no triangle fits, that side's too short";
        return 0;
    }
    if(h > 1)
        h = 1;
    tk[0] = asinf(h);
    tk[1] = PI - tk[0];

    /* acute one first. the obtuse one only if it still fits and isn't the
       same triangle again (h = 1, a right angle) */
    for(n = 0, i = 0; i < 2; i++) {
        float *ss = n == 0 ? s : s2, *tt = n == 0 ? t : t2;
        int m = 3 - j - k;

        if(i == 1 && h > 1 - EPS)
            break;
        if(t[j] + tk[i] >= PI - EPS)
            continue;
        if(n == 1) {
            memcpy(s2, s, 3 * sizeof(float));
            memcpy(t2, t, 3 * sizeof(float));
            s2[m] = 0;
            t2[m] = 0;
        }
        tt[k] = tk[i];
        tt[m] = PI - tt[j] - tt[k];
        sides_from_angles(ss, tt);
        n++;
    }
    if(n == 0)
        *msg = "no triangle fits, that angle's too big";
    return n;
}

/* "ab = 36.8699" with the degree sign in degrees, one line of the answer */
static box_t *value_line(const char *name, float x, bool angle) {
    char num[28];
    box_t *row = layout_box(BOX_ROW, 0);

    if(angle && setting_angle == ANGLE_DEGREES)
        x = x * 180 / PI;
    format(x, num);
    if(angle && setting_angle == ANGLE_DEGREES) {
        size_t len = strlen(num);

        num[len] = CH_DEG;
        num[len + 1] = '\0';
    }

    layout_add(row, layout_text(name, 0));
    layout_add(row, layout_space(LAYOUT_SPACE(0)));
    layout_add(row, layout_text("=", 0));
    layout_add(row, layout_space(LAYOUT_SPACE(0)));
    layout_add(row, layout_text(num, 0));
    layout_finish(row, &draw_font);
    return row;
}

/* all six in the order they're typed, then the area */
static box_t *tri_box(const float *s, const float *t) {
    box_t *col = layout_box(BOX_COL, 0);

    layout_add(col, value_line("a", s[0], false));
    layout_add(col, value_line("b", s[1], false));
    layout_add(col, value_line("c", s[2], false));
    layout_add(col, value_line("ab", t[2], true));
    layout_add(col, value_line("bc", t[0], true));
    layout_add(col, value_line("ca", t[1], true));
    layout_add(col, value_line("area", s[0] * s[1] * sinf(t[2]) / 2, false));
    layout_finish(col, &draw_font);
    return col;
}

static const char *run_triangle(char **piece, box_t **out) {
    float s[3], t[3], s2[3], t2[3], v[ARGS_MAX], sum = 0;
    float limit = setting_angle == ANGLE_DEGREES ? 180 : PI;
    const char *msg = NULL;
    pcas_error_t err;
    int i, ns = 0, na = 0, n;

    for(i = 0; i < ARGS_MAX; i++) {
        v[i] = 0;
        if(piece[i][0] == '\0')
            continue;
        if(!number_of(piece[i], &v[i], &err))
            return err != E_SUCCESS ? error_text[err] : "triangle takes numbers, not letters";
        if(v[i] < 0)
            return "no negative sides or angles";
        if(i < 3)
            ns += v[i] != 0;
        else {
            na += v[i] != 0;
            sum += v[i];
        }
    }

    if(ns + na < 3)
        return "needs 3 of the 6, and one of them a side";
    if(ns + na > 3)
        return "just 3 of the 6, leave the rest empty";
    if(ns == 0)
        return "needs a side too, angles only give the shape";
    if(sum >= limit * (1 - EPS))
        return setting_angle == ANGLE_DEGREES ? "the angles add up to 180 or more"
                                              : "the angles add up to pi or more";

    /* ab is across from c, bc from a, ca from b */
    for(i = 0; i < 3; i++)
        s[i] = v[i];
    t[2] = v[3] * PI / limit;
    t[0] = v[4] * PI / limit;
    t[1] = v[5] * PI / limit;

    n = solve_tri(s, t, s2, t2, &msg);
    if(n == 0)
        return msg;
    if(n == 1) {
        *out = tri_box(s, t);
        return NULL;
    }

    /* both side by side with an or in the middle, so it's 7 lines and not 15.
       the outside is a column so push doesn't try to make an editor row of it */
    {
        box_t *row = layout_box(BOX_ROW, 0), *mid = layout_box(BOX_COL, 0);

        for(i = 0; i < 7; i++)
            layout_add(mid, i == 3 ? layout_text("or", 0) : layout_box(BOX_ROW, 0));
        layout_add(row, tri_box(s, t));
        layout_add(row, layout_space(6 * SPACE_BINARY));
        layout_add(row, mid);
        layout_add(row, layout_space(6 * SPACE_BINARY));
        layout_add(row, tri_box(s2, t2));
        *out = layout_box(BOX_COL, 0);
        layout_add(*out, row);
        layout_finish(*out, &draw_font);
    }
    return NULL;
}

/* amplitude */

/* t as coef * sin(u) or coef * cos(u), coef free of var, u not. a copy of coef
   in *coef, the sin or cos itself (not copied) in *wave */
static bool wave_term(pcas_ast_t *t, Symbol var, pcas_ast_t **coef, pcas_ast_t **wave) {
    pcas_ast_t *k, *found = NULL;

    if(isoptype(t, OP_SIN) || isoptype(t, OP_COS)) {
        if(!uses(t, var))
            return false;
        *coef = ast_MakeNumber(num_FromInt(1));
        *wave = t;
        return true;
    }
    if(isoptype(t, OP_DIV)) {
        pcas_ast_t *bottom = ast_ChildGet(t, 1);

        if(uses(bottom, var) || !wave_term(ast_ChildGet(t, 0), var, coef, wave))
            return false;
        *coef = ast_MakeBinary(OP_DIV, *coef, ast_Copy(bottom));
        return true;
    }
    if(!isoptype(t, OP_MULT))
        return false;

    /* one factor with the variable in it, and that one's the sin or cos */
    for(k = ast_ChildGet(t, 0); k != NULL; k = k->next) {
        if(!uses(k, var))
            continue;
        if(found != NULL || !(isoptype(k, OP_SIN) || isoptype(k, OP_COS)))
            return false;
        found = k;
    }
    if(found == NULL)
        return false;
    *coef = ast_MakeOperator(OP_MULT);
    for(k = ast_ChildGet(t, 0); k != NULL; k = k->next)
        if(k != found)
            ast_ChildAppend(*coef, ast_Copy(k));
    *wave = found;
    return true;
}

/* NULL when it's not one of the shapes. A sin(Bx + C) + D (or cos) is |A|, and
   a sin(u) + b cos(u) is the sqrt(a^2 + b^2) one, the same wave shifted. the
   sins and the coss get added up first, simplify splits (1 - sqrt(2))cos(x) in two */
static pcas_ast_t *amplitude(pcas_ast_t *f, Symbol var) {
    pcas_ast_t *coef[2] = { NULL, NULL }, *inside = NULL, *k, *c, *w, *e = NULL;
    int i;
    bool sum = isoptype(f, OP_ADD);

    for(k = sum ? ast_ChildGet(f, 0) : f; k != NULL; k = sum ? k->next : NULL) {
        if(!uses(k, var))
            continue;
        if(!wave_term(k, var, &c, &w))
            goto done;
        if(inside != NULL && !ast_Compare(inside, ast_ChildGet(w, 0))) {
            ast_Cleanup(c);
            goto done;
        }
        inside = ast_ChildGet(w, 0);
        i = isoptype(w, OP_COS);
        if(coef[i] == NULL)
            coef[i] = ast_MakeOperator(OP_ADD);
        ast_ChildAppend(coef[i], c);
    }
    if(inside == NULL)
        goto done;

    for(i = 0; i < 2; i++)
        if(coef[i] != NULL)
            simplify(coef[i], SIMP_ALL);

    if(coef[0] == NULL || coef[1] == NULL || is_ast_int(coef[0], 0) || is_ast_int(coef[1], 0)) {
        pcas_ast_t *v;
        float x;

        /* just the one. the engine leaves |1/2| as it is and makes |-k| into k,
           so a plain number gets its sign looked at here and only letters keep
           the bars, with the minus taken off first */
        i = coef[0] == NULL || (coef[1] != NULL && is_ast_int(coef[0], 0));
        v = ast_Copy(coef[i]);
        approx(v);
        if(float_of(v, &x))
            e = x < 0 ? ast_MakeBinary(OP_MULT, ast_MakeNumber(num_FromInt(-1)), coef[i]) : coef[i];
        else {
            absolute_val(coef[i]);
            e = ast_MakeUnary(OP_ABS, coef[i]);
        }
        ast_Cleanup(v);
        coef[i] = NULL;
    } else {
        e = ast_MakeBinary(OP_ROOT, ast_MakeNumber(num_FromInt(2)),
                           ast_MakeBinary(OP_ADD, ast_MakeBinary(OP_POW, coef[0], ast_MakeNumber(num_FromInt(2))),
                                          ast_MakeBinary(OP_POW, coef[1], ast_MakeNumber(num_FromInt(2)))));
        coef[0] = coef[1] = NULL;
    }
    simplify(e, SIMP_ALL);

done:
    for(i = 0; i < 2; i++)
        ast_Cleanup(coef[i]);
    return e;
}

bool tri_run(const char *cmd, char *src, box_t *in) {
    pcas_ast_t *a[2] = { NULL, NULL }, *e = NULL;
    char *piece[ARGS_MAX];
    pcas_error_t err = E_SUCCESS;
    const char *message = NULL;
    box_t *out = NULL;
    clock_t start;
    Symbol var = SYM_X;
    int c, n, i;

    if(cmd == NULL)
        return false;
    for(c = 0; c < T_COUNT; c++)
        if(strcmp(cmd, cmds[c].name) == 0)
            break;
    if(c == T_COUNT)
        return false;

    n = split(src, piece);
    if(n < cmds[c].least || n > cmds[c].most) {
        push(in, NULL, cmds[c].usage, 0);
        return true;
    }

    start = clock();
    if(c == T_TRIANGLE)
        message = run_triangle(piece, &out);
    else if(c == T_AMPLITUDE) {
        if(n == 2 && !read_var(piece[1], &var))
            message = cmds[c].usage;
        else if((a[0] = read_piece(piece[0], &err)) == NULL)
            message = error_text[err];
        else {
            /* no trig identities, they'd turn sin(2x) into 2sin(x)cos(x) and
               that's not a shape this knows */
            simplify(a[0], SIMP_ALL & ~SIMP_ID_TRIG);
            e = amplitude(a[0], var);
            if(e == NULL && !pcas_cancelled)
                message = "can't tell the amplitude of that one";
        }
    } else {
        for(i = 0; i < 2 && message == NULL; i++) {
            if((a[i] = read_piece(piece[i], &err)) == NULL)
                message = error_text[err];
            else {
                pcas_ast_t *v;
                float x;

                simplify(a[i], SIMP_ALL);
                /* letters are fine, 3/2 isn't a probability of anything */
                v = ast_Copy(a[i]);
                approx(v);
                if(float_of(v, &x) && (x < -EPS || x > 1 + EPS))
                    message = "probabilities go from 0 to 1";
                ast_Cleanup(v);
            }
        }
        if(message == NULL) {
            /* independent, so P(A and B) = pa pb, and P(A or B) takes the overlap off once */
            e = ast_MakeBinary(OP_MULT, ast_Copy(a[0]), ast_Copy(a[1]));
            if(c == T_POR)
                e = ast_MakeBinary(OP_ADD, ast_MakeBinary(OP_ADD, ast_Copy(a[0]), ast_Copy(a[1])),
                                   ast_MakeBinary(OP_MULT, ast_MakeNumber(num_FromInt(-1)), e));
            simplify(e, SIMP_ALL);
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
    for(i = 0; i < 2; i++)
        ast_Cleanup(a[i]);
    return true;
}
