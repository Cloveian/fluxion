/*Fluxion: show the work. not in upstream PineappleCAS.*/

#include <string.h>

#include "steps.h"

#define SIMP_LIGHT (SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_RATIONAL | SIMP_EVAL)

/*a side read as c[0] + c[1]x + c[2]x^2 in numbers*/
typedef struct {
    mp_rat c[3];
    int deg;
} side_t;

/*a fraction as top over bottom, one number node is a decimal to export*/
static pcas_ast_t *number(mp_rat r) {
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
        return number(a);
    x = ast_MakeSymbol(var);
    if(p > 1)
        x = ast_MakeBinary(OP_POW, x, ast_MakeNumber(num_FromInt(p)));
    if(mp_rat_compare_value(a, 1, 1) != 0)
        x = ast_MakeBinary(OP_MULT, number(a), x);
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
    t = ast_ChildLength(sum) == 1 ? ast_ChildRemoveIndex(sum, 0) : ast_MakeNumber(num_FromInt(0));
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

static step_t *add_line(steps_t *s, pcas_ast_t *left, pcas_ast_t *right) {
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

/*x = (right +- pm)/den, right and den can be NULL: x=+-3*/
static step_t *add_pm(steps_t *s, Symbol var, pcas_ast_t *right, pcas_ast_t *pm, pcas_ast_t *den) {
    step_t *l = pm != NULL ? new_line(s) : NULL;

    if(l == NULL) {
        ast_Cleanup(right);
        ast_Cleanup(pm);
        ast_Cleanup(den);
        return NULL;
    }
    l->eq[0].left = ast_MakeSymbol(var);
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
    add_line(s, side_tree(side[0].c, var), side_tree(side[1].c, var));
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
    add_line(s, l, r);

    mp_int_set_value(d, 1);
    for(i = 0; i < 2; i++)
        for(p = 0; p < 3; p++)
            mp_int_lcm(d, mp_rat_denom_ref(side[i].c[p]), d);
    if(mp_int_compare_value(d, 1) != 0) {
        mp_rat m = num_FromInt(1);

        mp_int_copy(d, mp_rat_numer_ref(m));
        add_move(s, '*', number(m));
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
        add_move(s, op, number(size));
        mp_rat_sub(side[o].c[0], side[k].c[0], side[o].c[0]);
        mp_rat_set_value(side[k].c[0], 0, 1);
        add_sides(s, side, var);
    }
    if(mp_rat_compare_value(side[k].c[p], 1, 1) != 0) {
        add_move(s, '/', number(side[k].c[p]));
        mp_rat_div(side[o].c[0], side[k].c[p], side[o].c[0]);
        mp_rat_set_value(side[k].c[p], 1, 1);
        add_sides(s, side, var);
    }
    if(k == 1) /*4/3 = x, written the other way round*/
        add_line(s, term(side[1].c[p], var, p), number(side[0].c[0]));
    num_Cleanup(size);
    return k;
}

/*ax+b = cx+d*/
static bool linear(pcas_ast_t *left, pcas_ast_t *right, Symbol var, side_t *side, steps_t *s) {
    prep(left, right, var, side, s);
    isolate(s, side, var, 1);
    return !pcas_cancelled;
}

/*x = r or x = -r, the last line of the root methods*/
static void add_both(steps_t *s, Symbol var, pcas_ast_t *r) {
    pcas_ast_t *neg = ast_MakeBinary(OP_MULT, ast_MakeNumber(num_FromInt(-1)), ast_Copy(r));
    step_t *l;

    simplify(neg, SIMP_LIGHT);
    l = add_line(s, ast_MakeSymbol(var), r);
    if(l == NULL) {
        ast_Cleanup(neg);
        return;
    }
    l->eq[1].left = ast_MakeSymbol(var);
    l->eq[1].right = neg;
    l->eqs = 2;
}

/*no x term: x^2 = v like a linear one, then the square root of both sides. x^2=-4 carries on with i like solve*/
static bool square_root(pcas_ast_t *left, pcas_ast_t *right, Symbol var, side_t *side, steps_t *s) {
    pcas_ast_t *root, *done;
    int k;

    prep(left, right, var, side, s);
    k = isolate(s, side, var, 2);
    if(k < 0)
        return !pcas_cancelled;

    add_move(s, 'r', NULL);
    if(mp_rat_compare_zero(side[1 - k].c[0]) == 0) {
        add_line(s, ast_MakeSymbol(var), ast_MakeNumber(num_FromInt(0)));
        return !pcas_cancelled;
    }

    /*x = +-sqrt(8) then x = +-2sqrt(2), but x^2=9 goes straight to x = +-3*/
    root = ast_MakeBinary(OP_POW, number(side[1 - k].c[0]),
                          ast_MakeBinary(OP_DIV, ast_MakeNumber(num_FromInt(1)), ast_MakeNumber(num_FromInt(2))));
    done = ast_Copy(root);
    simplify(done, SIMP_ALL); /*the light one leaves sqrt(-4), it's only a number so it's cheap*/
    if(!ast_Compare(root, done) && !is_number(done))
        add_pm(s, var, NULL, root, NULL);
    else
        ast_Cleanup(root);
    add_pm(s, var, NULL, ast_Copy(done), NULL);
    add_both(s, var, done);
    return !pcas_cancelled;
}

/*every tree in the canonical form, the lines are only ever drawn. 3X+5 and not 5+3X*/
static void shown(steps_t *s) {
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

static bool fits(side_t *side, Method m) {
    switch(m) {
    case METHOD_ROOT:
        return is_quadratic(side) && mp_rat_compare_zero(side[0].c[1]) == 0 && mp_rat_compare_zero(side[1].c[1]) == 0;
    case METHOD_OPTIMAL:
        return is_linear(side) || fits(side, METHOD_ROOT);
    default:
        return false;
    }
}

/*what optimal means for this one, the first method that fits. OPTIMAL itself for a linear one*/
static Method pick(side_t *side, Method m) {
    if(m == METHOD_OPTIMAL && fits(side, METHOD_ROOT))
        return METHOD_ROOT;
    return m;
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

bool steps_solve(pcas_ast_t *left, pcas_ast_t *right, Symbol var, Method m, steps_t *out) {
    side_t side[2];
    bool ok = false;

    out->n = 0;
    if(!read_sides(left, right, var, side))
        return false;
    if(fits(side, m)) {
        switch(pick(side, m)) {
        case METHOD_ROOT:
            ok = square_root(left, right, var, side, out);
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
        shown(out);
    return ok;
}

void steps_free(steps_t *s) {
    int i, j;

    for(i = 0; i < s->n; i++) {
        for(j = 0; j < s->line[i].eqs; j++) {
            ast_Cleanup(s->line[i].eq[j].left);
            ast_Cleanup(s->line[i].eq[j].right);
            ast_Cleanup(s->line[i].eq[j].pm);
            ast_Cleanup(s->line[i].eq[j].den);
        }
        ast_Cleanup(s->line[i].by);
    }
    s->n = 0;
}
