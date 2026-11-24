/*Fluxion: show the work. not in upstream PineappleCAS.*/

#include <string.h>

#include "steps.h"

/*a side read as c[0] + c[1]x + c[2]x^2 in numbers*/
typedef struct {
    mp_rat c[3];
    int deg;
} side_t;

/*a fraction as top over bottom, one number node is a decimal to export*/
pcas_ast_t *steps_number(mp_rat r) {
    mp_rat top, bottom;

    if(mp_rat_is_integer(r))
        return ast_MakeNumber(num_Copy(r));
    top = num_FromInt(0);
    bottom = num_FromInt(0);
    mp_int_copy(mp_rat_numer_ref(r), mp_rat_numer_ref(top));
    mp_int_copy(mp_rat_denom_ref(r), mp_rat_numer_ref(bottom));
    return ast_MakeBinary(OP_DIV, ast_MakeNumber(top), ast_MakeNumber(bottom));
}

/*3 or 3/2, nothing with a root or an i in it*/
static bool is_number(pcas_ast_t *e) {
    mp_rat r = pow_value(e);

    num_Cleanup(r);
    return r != NULL;
}

static void side_free(side_t *s) {
    int i;

    for(i = 0; i < 3; i++) {
        num_Cleanup(s->c[i]);
        s->c[i] = NULL;
    }
}

/*false for anything with letters in a coefficient, or past x^2*/
static bool read_side(pcas_ast_t *e, Symbol var, side_t *s) {
    pcas_ast_t *c[3];
    bool ok = true;
    int i;

    s->deg = poly_coefs(e, var, c);
    for(i = 0; i < 3; i++) {
        s->c[i] = c[i] == NULL ? num_FromInt(0) : pow_value(c[i]);
        ok &= s->c[i] != NULL;
        ast_Cleanup(c[i]);
    }
    if(!ok || s->deg < 0 || s->deg > 2) {
        side_free(s);
        return false;
    }
    return true;
}

/*a x^p as you'd write it: x, -x, 3x, 3x^2, just a for p = 0*/
static pcas_ast_t *term(mp_rat a, Symbol var, int p) {
    pcas_ast_t *x;

    if(p == 0)
        return steps_number(a);
    x = ast_MakeSymbol(var);
    if(p > 1)
        x = ast_MakeBinary(OP_POW, x, ast_MakeInt(p));
    if(mp_rat_compare_value(a, 1, 1) != 0)
        x = ast_MakeBinary(OP_MULT, steps_number(a), x);
    return x;
}

/*c[2]x^2 + c[1]x + c[0], 0 if there's nothing*/
static pcas_ast_t *side_tree(mp_rat *c, Symbol var) {
    pcas_ast_t *sum = ast_MakeOperator(OP_ADD), *t;
    int p;

    if(sum == NULL)
        return NULL;
    for(p = 2; p >= 0; p--)
        if(mp_rat_compare_zero(c[p]) != 0)
            ast_ChildAppend(sum, term(c[p], var, p));
    if(ast_ChildLength(sum) > 1)
        return sum;
    t = ast_ChildLength(sum) == 1 ? ast_ChildRemoveIndex(sum, 0) : ast_MakeInt(0);
    ast_Cleanup(sum);
    return t;
}

/*an empty line on the end, NULL when it's full*/
static step_t *new_line(steps_t *s) {
    step_t *l;

    if(s->n == STEPS_MAX)
        return NULL;
    l = &s->line[s->n++];
    memset(l, 0, sizeof(*l));
    l->eqs = 1;
    return l;
}

step_t *steps_eq(steps_t *s, pcas_ast_t *left, pcas_ast_t *right) {
    step_t *l = left != NULL && right != NULL ? new_line(s) : NULL;

    if(l == NULL) {
        ast_Cleanup(left);
        ast_Cleanup(right);
        return NULL;
    }
    l->eq[0].left = left;
    l->eq[0].right = right;
    return l;
}

/*left = (right +- pm)/den, right and den can be NULL: x=+-3, x+3=+-sqrt(8)*/
static step_t *add_pm(steps_t *s, pcas_ast_t *left, pcas_ast_t *right, pcas_ast_t *pm, pcas_ast_t *den) {
    step_t *l = left != NULL && pm != NULL ? new_line(s) : NULL;

    if(l == NULL) {
        ast_Cleanup(left);
        ast_Cleanup(right);
        ast_Cleanup(pm);
        ast_Cleanup(den);
        return NULL;
    }
    l->eq[0].left = left;
    l->eq[0].right = right;
    l->eq[0].pm = pm;
    l->eq[0].den = den;
    return l;
}

/*on the last line, the move that gets to the next one*/
static void add_move(steps_t *s, char move, pcas_ast_t *by) {
    if(s->n == 0) {
        ast_Cleanup(by);
        return;
    }
    s->line[s->n - 1].move = move;
    s->line[s->n - 1].by = by;
}

/*_ for a positive one, + for a negative one, by its size*/
static char take_away(mp_rat r, mp_rat size) {
    mp_rat_abs(r, size);
    return mp_rat_compare_zero(r) > 0 ? '_' : '+';
}

static void add_sides(steps_t *s, side_t *side, Symbol var) {
    steps_eq(s, side_tree(side[0].c, var), side_tree(side[1].c, var));
}

/*the line as typed, then the fractions cleared (x/2+1=4 is x+2=8) or the sides tidyed (2(x+3) is 2x+6)*/
static void prep(pcas_ast_t *left, pcas_ast_t *right, Symbol var, side_t *side, steps_t *s) {
    mp_int d = mp_int_alloc();
    pcas_ast_t *l = ast_Copy(left), *r = ast_Copy(right), *want;
    bool tidy = false;
    int i, p;

    /*X_1 parses as X+-1*1 and shows a lone minus*/
    simplify(l, SIMP_NORMALIZE | SIMP_EVAL);
    simplify(r, SIMP_NORMALIZE | SIMP_EVAL);
    steps_eq(s, l, r);

    mp_int_set_value(d, 1);
    for(i = 0; i < 2; i++)
        for(p = 0; p < 3; p++)
            mp_int_lcm(d, mp_rat_denom_ref(side[i].c[p]), d);
    if(mp_int_compare_value(d, 1) != 0) {
        mp_rat m = num_FromInt(1);

        mp_int_copy(d, mp_rat_numer_ref(m));
        add_move(s, '*', steps_number(m));
        for(i = 0; i < 2; i++)
            for(p = 0; p < 3; p++)
                mp_rat_mul(side[i].c[p], m, side[i].c[p]);
        num_Cleanup(m);
        tidy = true;
    } else {
        for(i = 0; i < 2; i++) {
            l = ast_Copy(i == 0 ? left : right);
            want = side_tree(side[i].c, var);
            simplify(l, SIMP_LIGHT);
            tidy |= !ast_Compare(l, want);
            ast_Cleanup(l);
            ast_Cleanup(want);
        }
    }
    if(tidy)
        add_sides(s, side, var);
    mp_int_free(d);
}

/*the x^p terms over to the side with more of them, the number to the other side, divide. the side they
ended up on, written x^p = v, or -1 when they cancelled out (5=5, 5=3)*/
static int isolate(steps_t *s, side_t *side, Symbol var, int p) {
    mp_rat size = num_FromInt(0);
    int k = 0, o;
    char op;

    if(mp_rat_compare_zero(side[0].c[p]) == 0)
        k = 1;
    else if(mp_rat_compare_zero(side[1].c[p]) != 0 && mp_rat_compare(side[1].c[p], side[0].c[p]) > 0)
        k = 1;
    o = 1 - k;

    if(mp_rat_compare_zero(side[o].c[p]) != 0) {
        op = take_away(side[o].c[p], size); /*before term() reads size*/
        add_move(s, op, term(size, var, p));
        mp_rat_sub(side[k].c[p], side[o].c[p], side[k].c[p]);
        mp_rat_set_value(side[o].c[p], 0, 1);
        add_sides(s, side, var);
    }
    if(mp_rat_compare_zero(side[k].c[p]) == 0) {
        num_Cleanup(size);
        return -1;
    }
    if(mp_rat_compare_zero(side[k].c[0]) != 0) {
        op = take_away(side[k].c[0], size);
        add_move(s, op, steps_number(size));
        mp_rat_sub(side[o].c[0], side[k].c[0], side[o].c[0]);
        mp_rat_set_value(side[k].c[0], 0, 1);
        add_sides(s, side, var);
    }
    if(mp_rat_compare_value(side[k].c[p], 1, 1) != 0) {
        add_move(s, '/', steps_number(side[k].c[p]));
        mp_rat_div(side[o].c[0], side[k].c[p], side[o].c[0]);
        mp_rat_set_value(side[k].c[p], 1, 1);
        add_sides(s, side, var);
    }
    if(k == 1) /*4/3 = x, written the other way round*/
        steps_eq(s, term(side[1].c[p], var, p), steps_number(side[0].c[0]));
    num_Cleanup(size);
    return k;
}

/*ax+b = cx+d*/
static bool linear(pcas_ast_t *left, pcas_ast_t *right, Symbol var, side_t *side, steps_t *s) {
    prep(left, right, var, side, s);
    isolate(s, side, var, 1);
    return !pcas_cancelled;
}

/*x = (base + pm)/den or x = (base - pm)/den, each worked out. base and den can be NULL. takes all three*/
static void add_split(steps_t *s, Symbol var, pcas_ast_t *base, pcas_ast_t *pm, pcas_ast_t *den) {
    pcas_ast_t *x[2];
    step_t *l;
    int i;

    /*base/den +- pm/den, each half cancels on its own: (_6+4sqrt(2))/2 is _3+2sqrt(2) with no identity pass*/
    for(i = 0; i < 2; i++) {
        pcas_ast_t *b = base != NULL ? ast_Copy(base) : ast_MakeInt(0);
        pcas_ast_t *p = ast_MakeBinary(OP_MULT, ast_MakeInt(i == 0 ? 1 : -1), ast_Copy(pm));

        if(den != NULL) {
            b = ast_MakeBinary(OP_DIV, b, ast_Copy(den));
            p = ast_MakeBinary(OP_DIV, p, ast_Copy(den));
        }
        x[i] = ast_MakeBinary(OP_ADD, b, p);
        simplify(x[i], SIMP_LIGHT | SIMP_LIKE_TERMS);
    }
    ast_Cleanup(base);
    ast_Cleanup(pm);
    ast_Cleanup(den);

    l = steps_eq(s, ast_MakeSymbol(var), x[0]);
    if(l == NULL) {
        ast_Cleanup(x[1]);
        return;
    }
    l->eq[1].left = ast_MakeSymbol(var);
    l->eq[1].right = x[1];
    l->eqs = 2;
}

/*sqrt(8) as written*/
pcas_ast_t *sqrt_of(pcas_ast_t *v) {
    return ast_MakeBinary(OP_POW, v, ast_MakeBinary(OP_DIV, ast_MakeInt(1), ast_MakeInt(2)));
}

/*sqrt(v) worked out: 2sqrt(2), 3/2, and 2i for -4. sqrt(-n) is i sqrt(n) by hand, the identity passes took ~10 s on the CE for it*/
static pcas_ast_t *root_of(mp_rat v) {
    mp_rat size = num_FromInt(0);
    pcas_ast_t *r;

    mp_rat_abs(v, size);
    /*sqrt(p)/sqrt(q), the light simplify doesn't root a fraction: sqrt(1/4) stayed*/
    r = sqrt_of(int_number(mp_rat_numer_ref(size)));
    if(!mp_rat_is_integer(size))
        r = ast_MakeBinary(OP_DIV, r, sqrt_of(int_number(mp_rat_denom_ref(size))));
    if(mp_rat_compare_zero(v) < 0)
        r = ast_MakeBinary(OP_MULT, ast_MakeSymbol(SYM_IMAG), r);
    simplify(r, SIMP_LIGHT);
    num_Cleanup(size);
    return r;
}

/*left = v, square root of both sides: left = +-sqrt(8), then left = +-2sqrt(2) when it isn't just a number
(x^2=9 goes straight to x = +-3). gives the root as it ended up, or NULL for v = 0 (left = 0). takes left*/
static pcas_ast_t *root_both(steps_t *s, pcas_ast_t *left, mp_rat v) {
    pcas_ast_t *root, *done;

    add_move(s, 'r', NULL);
    if(mp_rat_compare_zero(v) == 0) {
        steps_eq(s, left, ast_MakeInt(0));
        return NULL;
    }
    root = sqrt_of(steps_number(v));
    done = root_of(v);
    if(done != NULL && !ast_Compare(root, done) && !is_number(done))
        add_pm(s, ast_Copy(left), NULL, root, NULL);
    else
        ast_Cleanup(root);
    add_pm(s, left, NULL, ast_Copy(done), NULL);
    return done;
}

/*no x term: x^2 = v like a linear one, then the square root of both sides. x^2=-4 carries on with i like solve*/
static bool square_root(pcas_ast_t *left, pcas_ast_t *right, Symbol var, side_t *side, steps_t *s) {
    pcas_ast_t *done;
    int k;

    prep(left, right, var, side, s);
    k = isolate(s, side, var, 2);
    if(k < 0)
        return !pcas_cancelled;

    done = root_both(s, ast_MakeSymbol(var), side[1 - k].c[0]);
    if(done != NULL)
        add_split(s, var, NULL, done, NULL);
    return !pcas_cancelled;
}

/*--- the rest of the quadratics, ax^2+bx+c = 0 ---*/

/*left minus right, for x^p*/
static void net(side_t *side, int p, mp_rat r) {
    mp_rat_sub(side[0].c[p], side[1].c[p], r);
}

/*b^2-4ac of left - right = 0, new*/
static mp_rat discriminant(side_t *side) {
    mp_rat a = num_FromInt(0), b = num_FromInt(0), c = num_FromInt(0), d = num_FromInt(0), four = num_FromInt(4);

    net(side, 2, a);
    net(side, 1, b);
    net(side, 0, c);
    mp_rat_mul(b, b, d);
    mp_rat_mul(a, c, a);
    mp_rat_mul(a, four, a);
    mp_rat_sub(d, a, d);
    num_Cleanup(a);
    num_Cleanup(b);
    num_Cleanup(c);
    num_Cleanup(four);
    return d;
}

/*sqrt(r) into r when it's a fraction. r is junk when it isn't*/
static bool rational_root(mp_rat r) {
    return mp_rat_compare_zero(r) >= 0 && int_root(mp_rat_numer_ref(r), 2) && int_root(mp_rat_denom_ref(r), 2);
}

/*everything over to the left, a > 0 and nothing they all share: 2x^2+4x=6 is x^2+2x_3=0. after prep(), they're whole*/
static void to_zero(steps_t *s, side_t *side, Symbol var) {
    mp_rat size = num_FromInt(0), g = num_FromInt(0);
    bool moved = true;
    char op;
    int p;

    if(mp_rat_compare_zero(side[1].c[2]) != 0 || mp_rat_compare_zero(side[1].c[1]) != 0) {
        add_move(s, '_', side_tree(side[1].c, var));
    } else if(mp_rat_compare_zero(side[1].c[0]) != 0) {
        op = take_away(side[1].c[0], size);
        add_move(s, op, steps_number(size));
    } else {
        moved = false;
    }
    if(moved) {
        for(p = 0; p < 3; p++) {
            mp_rat_sub(side[0].c[p], side[1].c[p], side[0].c[p]);
            mp_rat_set_value(side[1].c[p], 0, 1);
        }
        add_sides(s, side, var);
    }

    if(mp_rat_compare_zero(side[0].c[2]) < 0) {
        add_move(s, '*', ast_MakeInt(-1));
        for(p = 0; p < 3; p++)
            mp_rat_neg(side[0].c[p], side[0].c[p]);
        add_sides(s, side, var);
    }

    for(p = 0; p < 3; p++)
        mp_int_gcd(mp_rat_numer_ref(g), mp_rat_numer_ref(side[0].c[p]), mp_rat_numer_ref(g));
    if(mp_rat_compare_value(g, 1, 1) > 0) {
        add_move(s, '/', steps_number(g));
        for(p = 0; p < 3; p++)
            mp_rat_div(side[0].c[p], g, side[0].c[p]);
        add_sides(s, side, var);
    }
    num_Cleanup(size);
    num_Cleanup(g);
}

/*a line of two, l1 = r1 or l2 = r2. one if l2 is NULL*/
static void add_or(steps_t *s, pcas_ast_t *l1, pcas_ast_t *r1, pcas_ast_t *l2, pcas_ast_t *r2) {
    step_t *l = steps_eq(s, l1, r1);

    if(l == NULL || l2 == NULL || r2 == NULL) {
        ast_Cleanup(l2);
        ast_Cleanup(r2);
        return;
    }
    l->eq[1].left = l2;
    l->eq[1].right = r2;
    l->eqs = 2;
}

/*qx - p, the factor for the root p/q*/
static pcas_ast_t *root_factor(mp_rat root, Symbol var) {
    mp_rat c[3];
    pcas_ast_t *t;
    int p;

    for(p = 0; p < 3; p++)
        c[p] = num_FromInt(0);
    mp_int_neg(mp_rat_numer_ref(root), mp_rat_numer_ref(c[0]));
    mp_int_copy(mp_rat_denom_ref(root), mp_rat_numer_ref(c[1]));
    t = side_tree(c, var);
    for(p = 0; p < 3; p++)
        num_Cleanup(c[p]);
    return t;
}

/*rational roots: (x+2)(x+3)=0. a isn't 1: the ac method, 2x^2+3x+4x+6=0 first, the middle split so it groups*/
static bool factoring(pcas_ast_t *left, pcas_ast_t *right, Symbol var, side_t *side, steps_t *s) {
    mp_rat d, r[2], m = num_FromInt(0), n = num_FromInt(0), two_a = num_FromInt(2);
    pcas_ast_t *f[2], *split;
    mp_rat *c = side[0].c;
    bool twice;
    int i;

    prep(left, right, var, side, s);
    to_zero(s, side, var);

    d = discriminant(side);
    rational_root(d); /*fits() checked it is one*/
    mp_rat_mul(two_a, c[2], two_a);
    for(i = 0; i < 2; i++) {
        r[i] = num_FromInt(0);
        mp_rat_neg(c[1], r[i]);
        if(i == 0)
            mp_rat_add(r[i], d, r[i]);
        else
            mp_rat_sub(r[i], d, r[i]);
        mp_rat_div(r[i], two_a, r[i]);
    }
    twice = mp_rat_compare(r[0], r[1]) == 0;

    /*-q1p2 and -q2p1, they add up to b and multiply to ac*/
    if(mp_rat_compare_value(c[2], 1, 1) != 0) {
        mp_int_mul(mp_rat_denom_ref(r[0]), mp_rat_numer_ref(r[1]), mp_rat_numer_ref(m));
        mp_int_mul(mp_rat_denom_ref(r[1]), mp_rat_numer_ref(r[0]), mp_rat_numer_ref(n));
        mp_rat_neg(m, m);
        mp_rat_neg(n, n);
        if(mp_rat_compare_zero(m) != 0 && mp_rat_compare_zero(n) != 0) {
            split = ast_MakeOperator(OP_ADD);
            ast_ChildAppend(split, term(c[2], var, 2));
            ast_ChildAppend(split, term(m, var, 1));
            ast_ChildAppend(split, term(n, var, 1));
            ast_ChildAppend(split, steps_number(c[0]));
            steps_eq(s, split, ast_MakeInt(0));
        }
    }

    f[0] = root_factor(r[0], var);
    f[1] = root_factor(r[1], var);
    if(twice)
        steps_eq(s, ast_MakeBinary(OP_POW, ast_Copy(f[0]), ast_MakeInt(2)), ast_MakeInt(0));
    else
        steps_eq(s, ast_MakeBinary(OP_MULT, ast_Copy(f[0]), ast_Copy(f[1])), ast_MakeInt(0));
    add_or(s, f[0], ast_MakeInt(0), twice ? NULL : f[1], ast_MakeInt(0));
    if(twice)
        ast_Cleanup(f[1]);
    add_or(s, ast_MakeSymbol(var), steps_number(r[0]), twice ? NULL : ast_MakeSymbol(var), steps_number(r[1]));

    for(i = 0; i < 2; i++)
        num_Cleanup(r[i]);
    num_Cleanup(d);
    num_Cleanup(m);
    num_Cleanup(n);
    num_Cleanup(two_a);
    return !pcas_cancelled;
}

/*a = 1, the number over, (b/2)^2 on both sides, (x+b/2)^2 = k, square root, b/2 back over*/
static bool complete_square(pcas_ast_t *left, pcas_ast_t *right, Symbol var, side_t *side, steps_t *s) {
    mp_rat h = num_FromInt(0), size = num_FromInt(0), two = num_FromInt(2), shift[3];
    mp_rat *c = side[0].c;
    pcas_ast_t *x, *done;
    char op;
    int p;

    prep(left, right, var, side, s);
    to_zero(s, side, var);

    if(mp_rat_compare_value(c[2], 1, 1) != 0) {
        add_move(s, '/', steps_number(c[2]));
        for(p = 0; p < 2; p++)
            mp_rat_div(c[p], c[2], c[p]);
        mp_rat_set_value(c[2], 1, 1);
        add_sides(s, side, var);
    }
    if(mp_rat_compare_zero(c[0]) != 0) {
        op = take_away(c[0], size);
        add_move(s, op, steps_number(size));
        mp_rat_neg(c[0], side[1].c[0]);
        mp_rat_set_value(c[0], 0, 1);
        add_sides(s, side, var);
    }

    /*x + b/2, squared on the left*/
    mp_rat_div(c[1], two, h);
    for(p = 0; p < 3; p++)
        shift[p] = num_FromInt(0);
    mp_rat_copy(h, shift[0]);
    mp_rat_set_value(shift[1], 1, 1);
    x = side_tree(shift, var);
    if(mp_rat_compare_zero(h) != 0) {
        mp_rat_mul(h, h, size);
        add_move(s, '+', steps_number(size));
        mp_rat_copy(size, c[0]);
        mp_rat_add(side[1].c[0], size, side[1].c[0]);
        add_sides(s, side, var);
        steps_eq(s, ast_MakeBinary(OP_POW, ast_Copy(x), ast_MakeInt(2)), steps_number(side[1].c[0]));
    }

    done = root_both(s, x, side[1].c[0]);
    if(mp_rat_compare_zero(h) != 0) {
        op = take_away(h, size);
        add_move(s, op, steps_number(size));
        mp_rat_neg(h, h);
        if(done == NULL)
            steps_eq(s, ast_MakeSymbol(var), steps_number(h));
        else
            add_pm(s, ast_MakeSymbol(var), steps_number(h), ast_Copy(done), NULL);
    }
    if(done != NULL)
        add_split(s, var, mp_rat_compare_zero(h) != 0 ? steps_number(h) : NULL, done, NULL);

    for(p = 0; p < 3; p++)
        num_Cleanup(shift[p]);
    num_Cleanup(h);
    num_Cleanup(size);
    num_Cleanup(two);
    return !pcas_cancelled;
}

/*k*a*b as written, 1s left out. layout.c draws a 1 in a product as nothing, 4*1*1 came out 4* */
static pcas_ast_t *times(int k, mp_rat a, mp_rat b) {
    pcas_ast_t *t = ast_MakeOperator(OP_MULT), *only;

    if(t == NULL)
        return NULL;
    ast_ChildAppend(t, ast_MakeInt(k));
    if(mp_rat_compare_value(a, 1, 1) != 0)
        ast_ChildAppend(t, steps_number(a));
    if(b != NULL && mp_rat_compare_value(b, 1, 1) != 0)
        ast_ChildAppend(t, steps_number(b));
    if(ast_ChildLength(t) > 1)
        return t;
    only = ast_ChildRemoveIndex(t, 0);
    ast_Cleanup(t);
    return only;
}

/*a=, b=, c=, then x = (-b +- sqrt(b^2-4ac))/(2a) with the numbers just in, then worked out a bit at a time*/
static bool formula(pcas_ast_t *left, pcas_ast_t *right, Symbol var, side_t *side, steps_t *s) {
    mp_rat *c = side[0].c, d, nb = num_FromInt(0), two_a = num_FromInt(2);
    pcas_ast_t *in, *raw, *done;
    step_t *l;

    prep(left, right, var, side, s);
    to_zero(s, side, var);

    l = steps_eq(s, ast_MakeSymbol('A'), steps_number(c[2]));
    if(l != NULL) {
        l->eq[1].left = ast_MakeSymbol('B');
        l->eq[1].right = steps_number(c[1]);
        l->eq[2].left = ast_MakeSymbol('C');
        l->eq[2].right = steps_number(c[0]);
        l->eqs = 3;
        l->list = true;
    }

    /*root nodes by hand, these two lines skip the canonical form that'd make them*/
    mp_rat_neg(c[1], nb);
    mp_rat_mul(two_a, c[2], two_a);
    in = times(-4, c[2], c[0]);
    in = ast_MakeBinary(OP_ADD, ast_MakeBinary(OP_POW, steps_number(c[1]), ast_MakeInt(2)), in);
    l = add_pm(s, ast_MakeSymbol(var), steps_number(nb), ast_MakeBinary(OP_ROOT, ast_MakeInt(2), in),
               times(2, c[2], NULL));
    if(l != NULL)
        l->as_is = true;

    d = discriminant(side);
    l = add_pm(s, ast_MakeSymbol(var), steps_number(nb), ast_MakeBinary(OP_ROOT, ast_MakeInt(2), steps_number(d)), steps_number(two_a));
    if(l != NULL)
        l->as_is = true;

    /*sqrt(32) is 4sqrt(2), sqrt(-16) is 4i*/
    raw = sqrt_of(steps_number(d));
    done = root_of(d);
    if(!ast_Compare(raw, done))
        add_pm(s, ast_MakeSymbol(var), steps_number(nb), ast_Copy(done), steps_number(two_a));
    ast_Cleanup(raw);
    if(mp_rat_compare_zero(d) == 0) { /*one root, just x=-3*/
        mp_rat_div(nb, two_a, nb);
        steps_eq(s, ast_MakeSymbol(var), steps_number(nb));
        ast_Cleanup(done);
    } else {
        add_split(s, var, steps_number(nb), done, steps_number(two_a));
    }

    num_Cleanup(d);
    num_Cleanup(nb);
    num_Cleanup(two_a);
    return !pcas_cancelled;
}

/*--- derivatives, one rule a line ---*/

step_t *steps_expr(steps_t *s, pcas_ast_t *e) {
    step_t *l = e != NULL ? new_line(s) : NULL;

    if(l == NULL) {
        ast_Cleanup(e);
        return NULL;
    }
    l->eq[0].left = e;
    return l;
}

int steps_find(pcas_ast_t *e, OperatorType op, pcas_ast_t **d, int n) {
    pcas_ast_t *c;

    if(e->type != NODE_OPERATOR || n >= STEPS_FOUND_MAX)
        return n;
    if(isoptype(e, op)) {
        d[n] = e;
        return n + 1;
    }
    for(c = ast_ChildGet(e, 0); c != NULL; c = c->next)
        n = steps_find(c, op, d, n);
    return n;
}

/*d/dx(f), same x as e*/
static pcas_ast_t *deriv_of(pcas_ast_t *f, pcas_ast_t *e) {
    pcas_ast_t *d = ast_MakeOperator(OP_DERIV);

    if(d == NULL) {
        ast_Cleanup(f);
        return NULL;
    }
    ast_ChildAppend(d, f);
    ast_ChildAppend(d, ast_Copy(ast_ChildGet(e, 1)));
    ast_ChildAppend(d, ast_Copy(ast_ChildGet(e, 2)));
    return d;
}

/*which id_derivative rule f gets, and its argument for the chain rule*/
static const char *table_rule(pcas_ast_t *f, pcas_ast_t **arg) {
    *arg = ast_ChildGet(f, 0);
    switch(optype(f)) {
    case OP_DIV:
        *arg = NULL;
        return "quotient rule";
    case OP_POW:
        *arg = ast_ChildGet(f, 1);
        return "exp rule";
    case OP_LOG:
        *arg = ast_ChildGet(f, 1);
        return "log rule";
    case OP_SIN: case OP_COS: case OP_TAN: case OP_COT: case OP_SEC: case OP_CSC:
        return "trig rule";
    case OP_SIN_INV: case OP_COS_INV: case OP_TAN_INV:
    case OP_COT_INV: case OP_SEC_INV: case OP_CSC_INV:
        return "inverse trig rule";
    default:
        return "hyperbolic rule";
    }
}

/*one rule on the deriv node e, the one eval_derivative_nodes would use first. its name, NULL if none fits.
*chain when it left a d/dx of something that isn't just x*/
static const char *one_rule(pcas_ast_t *e, Symbol var, bool *chain) {
    pcas_ast_t *f = ast_ChildGet(e, 0), *k, *rest, *c, *arg;
    const char *name;

    *chain = false;
    if(!has_symbol(f, var)) {
        replace_node(e, ast_MakeInt(0));
        return "constant rule";
    }
    if(isoptype(f, OP_ADD)) {
        rest = ast_MakeOperator(OP_ADD);
        for(c = ast_ChildGet(f, 0); c != NULL; c = c->next)
            ast_ChildAppend(rest, deriv_of(ast_Copy(c), e));
        replace_node(e, rest);
        return "sum rule";
    }
    if(isoptype(f, OP_MULT)) {
        /*3x^2 is 3 d/dx(x^2) on paper, the product rule would make a d/dx(3) to throw away*/
        k = ast_MakeOperator(OP_MULT);
        rest = ast_MakeOperator(OP_MULT);
        for(c = ast_ChildGet(f, 0); c != NULL; c = c->next)
            ast_ChildAppend(has_symbol(c, var) ? rest : k, ast_Copy(c));
        if(ast_ChildLength(k) > 0) {
            if(ast_ChildLength(rest) == 1) {
                pcas_ast_t *only = ast_ChildRemoveIndex(rest, 0);

                ast_Cleanup(rest);
                rest = only;
            }
            ast_ChildAppend(k, deriv_of(rest, e));
            replace_node(e, k);
            return "constant multiple";
        }
        ast_Cleanup(k);
        ast_Cleanup(rest);
        return id_Execute(e, &id_deriv_product_rule, false) ? "product rule" : NULL;
    }
    if(isoptype(f, OP_DIV) && !has_symbol(ast_ChildGet(f, 1), var)) {
        /*x^2/2, and log(x) is ln(x)/ln(10) off the table. the quotient rule would d/dx the bottom for nothing*/
        replace_node(e, ast_MakeBinary(OP_DIV, deriv_of(ast_Copy(ast_ChildGet(f, 0)), e), ast_Copy(ast_ChildGet(f, 1))));
        return "constant multiple";
    }
    if(isoptype(f, OP_POW) && !has_symbol(ast_ChildGet(f, 1), var)) {
        *chain = ast_ChildGet(f, 0)->type != NODE_SYMBOL;
        return id_Execute(e, &id_deriv_power_rule, false) ? "power rule" : NULL;
    }
    if(f->type == NODE_SYMBOL) { /*just x, it's x^1*/
        replace_node(e, ast_MakeInt(1));
        return "power rule";
    }
    if(f->type != NODE_OPERATOR)
        return NULL;
    name = table_rule(f, &arg);
    *chain = arg != NULL && arg->type != NODE_SYMBOL;
    return id_ExecuteTable(e, id_derivative, ID_NUM_DERIV, false) ? name : NULL;
}

/*d/dx(5) is 0 and d/dx(2x+1) is 2 without a line of their own, like on paper, then a light tidy: 1*2x^(2_1) is 2x*/
static void quiet_rules(pcas_ast_t *e, Symbol var) {
    pcas_ast_t *d[STEPS_FOUND_MAX], *c[3];
    int n = steps_find(e, OP_DERIV, d, 0), i, k, deg;

    for(i = 0; i < n; i++) {
        deg = poly_coefs(ast_ChildGet(d[i], 0), var, c);
        if(deg == 0) {
            replace_node(d[i], ast_MakeInt(0));
        } else if(deg == 1) {
            /*any slope, pi/180 from degrees too*/
            replace_node(d[i], c[1]);
            c[1] = NULL;
        }
        for(k = 0; k < 3; k++)
            ast_Cleanup(c[k]);
    }
    simplify(e, SIMP_LIGHT | SIMP_LIKE_TERMS); /*like terms takes the 1 off cos(x)*1. not SIMP_ALL, ~7 s on the CE for x^2sin(x) and it spreads (1-ln(x))/x^2 out*/
}

void steps_rule(step_t *l, const char *name) {
    int i;

    if(l == NULL)
        return;
    l->move = 'n';
    for(i = 0; i < STEP_RULES_MAX && l->rules[i] != NULL; i++)
        if(strcmp(l->rules[i], name) == 0)
            return;
    if(i < STEP_RULES_MAX)
        l->rules[i] = name;
}

bool steps_derivative(pcas_ast_t *d, steps_t *out) {
    pcas_ast_t *cur, *found[STEPS_FOUND_MAX], *done, *first;
    Symbol var;
    bool chain, any_chain;
    const char *name;
    step_t *l;
    int n, i;

    out->n = 0;
    if(!isoptype(d, OP_DERIV) || ast_ChildGet(d, 1)->type != NODE_SYMBOL)
        return false;
    var = ast_ChildGet(d, 1)->op.symbol;

    first = ast_Copy(d);
    simplify(ast_ChildGet(first, 0), SIMP_LIGHT); /*the shape degrees_out knows, like the answer's. the rational pass makes (x pi)/180*/
    l = steps_expr(out, first);
    cur = ast_Copy(d);
    simplify(ast_ChildGet(cur, 0), SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_EVAL); /*sqrt(x) is x^(1/2) for the rules, a+b+c one sum*/
    if(cur != NULL) /*d/dx at 3: the rules on d/dx at x, the 3 goes in at the end*/
        replace_node(ast_ChildGet(cur, 2), ast_Copy(ast_ChildGet(d, 1)));

    while(l != NULL && !pcas_cancelled && (n = steps_find(cur, OP_DERIV, found, 0)) > 0) { /*cur can be NULL after an oom*/
        any_chain = false;
        for(i = 0; i < n; i++) {
            if((name = one_rule(found[i], var, &chain)) == NULL) {
                ast_Cleanup(cur);
                steps_free(out);
                return false;
            }
            steps_rule(l, name);
            any_chain |= chain;
        }
        if(any_chain)
            steps_rule(l, "chain rule");
        quiet_rules(cur, var);
        l = steps_expr(out, ast_Copy(cur));
    }
    if(l == NULL || pcas_cancelled) { /*ran out of lines*/
        ast_Cleanup(cur);
        steps_free(out);
        return false;
    }

    /*at a value: the value in as written, then what that comes to. no x left in it is no plug in*/
    if(!ast_Compare(ast_ChildGet(d, 1), ast_ChildGet(d, 2)) && substitute(cur, ast_ChildGet(d, 1), ast_ChildGet(d, 2))) {
        steps_rule(l, "plug in");
        steps_expr(out, ast_Copy(cur));
        done = ast_Copy(cur);
        simplify(done, SIMP_ALL);
        if(ast_Compare(done, cur)) /*2y at y*/
            ast_Cleanup(done);
        else
            steps_expr(out, done);
    }
    ast_Cleanup(cur);
    if(pcas_cancelled) { /*cancel in the plug in, half done*/
        steps_free(out);
        return false;
    }
    steps_shown(out);
    return true;
}

bool steps_calc(pcas_ast_t *e, steps_t *out) {
    if(isoptype(e, OP_LIMIT))
        return steps_limit(e, out);
    if(isoptype(e, OP_INTEGRAL))
        return steps_integral(e, out);
    return steps_derivative(e, out);
}

void steps_shown(steps_t *s) {
    int i, j;

    for(i = 0; i < s->n; i++) {
        for(j = 0; j < s->line[i].eqs && !s->line[i].as_is; j++) {
            step_eq_t *q = &s->line[i].eq[j];

            if(q->left != NULL)
                simplify_canonical_form(q->left, CANONICAL_ALL);
            if(q->right != NULL)
                simplify_canonical_form(q->right, CANONICAL_ALL);
            if(q->pm != NULL)
                simplify_canonical_form(q->pm, CANONICAL_ALL);
            if(q->den != NULL)
                simplify_canonical_form(q->den, CANONICAL_ALL);
            if(q->low != NULL)
                simplify_canonical_form(q->low, CANONICAL_ALL);
        }
        if(s->line[i].by != NULL)
            simplify_canonical_form(s->line[i].by, CANONICAL_ALL);
    }
}

static bool read_sides(pcas_ast_t *left, pcas_ast_t *right, Symbol var, side_t *side) {
    if(!read_side(left, var, &side[0]))
        return false;
    if(!read_side(right, var, &side[1])) {
        side_free(&side[0]);
        return false;
    }
    return true;
}

/*an x somewhere and no x^2 anywhere*/
static bool is_linear(side_t *side) {
    return side[0].deg <= 1 && side[1].deg <= 1
           && (mp_rat_compare_zero(side[0].c[1]) != 0 || mp_rat_compare_zero(side[1].c[1]) != 0);
}

/*an x^2 that doesn't cancel out*/
static bool is_quadratic(side_t *side) {
    return mp_rat_compare(side[0].c[2], side[1].c[2]) != 0;
}

/*the order optimal goes down*/
static const Method optimal[] = { METHOD_ROOT, METHOD_FACTOR, METHOD_SQUARE, METHOD_FORMULA };

/*works at all, for the method menu*/
static bool fits(side_t *side, Method m) {
    mp_rat d;
    bool ok;

    if(m == METHOD_OPTIMAL)
        return is_linear(side) || is_quadratic(side);
    if(!is_quadratic(side))
        return false;
    switch(m) {
    case METHOD_ROOT:
        return mp_rat_compare_zero(side[0].c[1]) == 0 && mp_rat_compare_zero(side[1].c[1]) == 0;
    case METHOD_FACTOR:
        d = discriminant(side);
        ok = rational_root(d);
        num_Cleanup(d);
        return ok;
    default:
        return true;
    }
}

/*what optimal takes: completing the square only with a 1 in front and an even b, x^2+6x+1*/
static bool suits(side_t *side, Method m) {
    mp_rat a, b;
    bool ok;

    if(m != METHOD_SQUARE)
        return fits(side, m);
    a = num_FromInt(0);
    b = num_FromInt(0);
    net(side, 2, a);
    net(side, 1, b);
    mp_rat_abs(a, a);
    ok = mp_rat_compare_value(a, 1, 1) == 0 && mp_rat_is_integer(b) && mp_int_is_even(mp_rat_numer_ref(b));
    num_Cleanup(a);
    num_Cleanup(b);
    return ok;
}

/*optimal as one of the others, OPTIMAL itself for a linear one*/
static Method pick(side_t *side, Method m) {
    unsigned i;

    if(m != METHOD_OPTIMAL || !is_quadratic(side))
        return m;
    for(i = 0; i < sizeof(optimal) / sizeof(optimal[0]); i++)
        if(suits(side, optimal[i]))
            return optimal[i];
    return METHOD_FORMULA;
}

bool steps_fit(pcas_ast_t *left, pcas_ast_t *right, Symbol var, Method m) {
    side_t side[2];
    bool fit;

    if(!read_sides(left, right, var, side))
        return false;
    fit = fits(side, m);
    side_free(&side[0]);
    side_free(&side[1]);
    return fit;
}

bool steps_append(steps_t *s, steps_t *t) {
    step_t *last = &s->line[s->n - 1];
    int k, j;

    if(s->n + t->n - 1 > STEPS_MAX || t->n < 2) {
        steps_free(t);
        return false;
    }
    last->move = t->line[0].move;
    last->by = t->line[0].by;
    memcpy(last->rules, t->line[0].rules, sizeof(last->rules));
    for(j = 0; j < t->line[0].eqs; j++) {
        ast_Cleanup(t->line[0].eq[j].left);
        ast_Cleanup(t->line[0].eq[j].right);
        ast_Cleanup(t->line[0].eq[j].pm);
        ast_Cleanup(t->line[0].eq[j].den);
    }
    for(k = 1; k < t->n; k++)
        s->line[s->n++] = t->line[k];
    t->n = 0;
    return true;
}

bool steps_linear(pcas_ast_t *left, pcas_ast_t *right, Symbol var, steps_t *out) {
    side_t side[2];
    bool ok = false;

    out->n = 0;
    if(!read_sides(left, right, var, side))
        return false;
    if(side[0].deg <= 1 && side[1].deg <= 1 && (side[0].deg == 1 || side[1].deg == 1))
        ok = linear(left, right, var, side, out);
    side_free(&side[0]);
    side_free(&side[1]);
    if(!ok)
        steps_free(out);
    return ok;
}

bool steps_solve(pcas_ast_t *left, pcas_ast_t *right, Symbol var, Method m, steps_t *out) {
    side_t side[2];
    bool ok = false;

    out->n = 0;
    if(!read_sides(left, right, var, side))
        return m == METHOD_OPTIMAL && (steps_layers(left, right, var, out) || steps_poly(left, right, var, out));
    if(fits(side, m)) {
        switch(pick(side, m)) {
        case METHOD_ROOT:
            ok = square_root(left, right, var, side, out);
            break;
        case METHOD_FACTOR:
            ok = factoring(left, right, var, side, out);
            break;
        case METHOD_SQUARE:
            ok = complete_square(left, right, var, side, out);
            break;
        case METHOD_FORMULA:
            ok = formula(left, right, var, side, out);
            break;
        default:
            ok = linear(left, right, var, side, out);
            break;
        }
    }

    side_free(&side[0]);
    side_free(&side[1]);
    if(!ok)
        steps_free(out);
    else
        steps_shown(out);
    return ok;
}

void steps_free(steps_t *s) {
    steps_cut(s, 0);
}

void steps_cut(steps_t *s, int n) {
    int i, j;

    for(i = n; i < s->n; i++) {
        for(j = 0; j < s->line[i].eqs; j++) {
            ast_Cleanup(s->line[i].eq[j].left);
            ast_Cleanup(s->line[i].eq[j].right);
            ast_Cleanup(s->line[i].eq[j].pm);
            ast_Cleanup(s->line[i].eq[j].den);
            ast_Cleanup(s->line[i].eq[j].low);
        }
        ast_Cleanup(s->line[i].by);
    }
    s->n = n;
}
