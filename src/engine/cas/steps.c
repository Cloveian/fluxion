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

/*ax + b as you'd write it: x, -x, 3x, 3x+5, just 5*/
static pcas_ast_t *linear_side(mp_rat a, mp_rat b, Symbol var) {
    pcas_ast_t *x = NULL;

    if(mp_rat_compare_zero(a) != 0) {
        x = ast_MakeSymbol(var);
        if(mp_rat_compare_value(a, 1, 1) != 0)
            x = ast_MakeBinary(OP_MULT, number(a), x);
    }
    if(x == NULL)
        return number(b);
    if(mp_rat_compare_zero(b) == 0)
        return x;
    return ast_MakeBinary(OP_ADD, x, number(b));
}

static step_t *add_line(steps_t *s, pcas_ast_t *left, pcas_ast_t *right) {
    step_t *l;

    if(s->n == STEPS_MAX || left == NULL || right == NULL) {
        ast_Cleanup(left);
        ast_Cleanup(right);
        return NULL;
    }
    l = &s->line[s->n++];
    memset(l, 0, sizeof(*l));
    l->eq[0].left = left;
    l->eq[0].right = right;
    l->eqs = 1;
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

/*ax+b = cx+d. x stays on the side where there's more of it, like on paper*/
static bool linear(pcas_ast_t *left, pcas_ast_t *right, Symbol var, side_t *side, steps_t *s) {
    mp_rat a[2], b[2], size = num_FromInt(0), zero = num_FromInt(0);
    mp_int d = mp_int_alloc();
    pcas_ast_t *l, *r, *want[2];
    bool tidy;
    int k, o, i;
    char op;

    for(i = 0; i < 2; i++) {
        a[i] = side[i].c[1];
        b[i] = side[i].c[0];
    }
    k = mp_rat_compare_zero(a[1]) == 0 ? 0 : mp_rat_compare_zero(a[0]) == 0 ? 1 : mp_rat_compare(a[0], a[1]) >= 0 ? 0 : 1;
    o = 1 - k;

    /*as typed, but X_1 parses as X+-1*1 and shows a lone minus*/
    l = ast_Copy(left);
    r = ast_Copy(right);
    simplify(l, SIMP_NORMALIZE | SIMP_EVAL);
    simplify(r, SIMP_NORMALIZE | SIMP_EVAL);
    add_line(s, l, r);

    /*fractions: times every bottom first, x/2+1=4 is x+2=8*/
    mp_int_set_value(d, 1);
    for(i = 0; i < 2; i++) {
        mp_int_lcm(d, mp_rat_denom_ref(a[i]), d);
        mp_int_lcm(d, mp_rat_denom_ref(b[i]), d);
    }
    if(mp_int_compare_value(d, 1) != 0) {
        mp_rat m = num_FromInt(1);

        mp_int_copy(d, mp_rat_numer_ref(m));
        add_move(s, '*', number(m));
        for(i = 0; i < 2; i++) {
            mp_rat_mul(a[i], m, a[i]);
            mp_rat_mul(b[i], m, b[i]);
        }
        num_Cleanup(m);
        tidy = true;
    } else {
        /*2(x+3) or 3x+2x+1 gets a line of its own before anything moves*/
        l = ast_Copy(left);
        r = ast_Copy(right);
        want[0] = linear_side(a[0], b[0], var);
        want[1] = linear_side(a[1], b[1], var);
        simplify(l, SIMP_LIGHT);
        simplify(r, SIMP_LIGHT);
        tidy = !ast_Compare(l, want[0]) || !ast_Compare(r, want[1]);
        ast_Cleanup(l);
        ast_Cleanup(r);
        ast_Cleanup(want[0]);
        ast_Cleanup(want[1]);
    }
    if(tidy)
        add_line(s, linear_side(a[0], b[0], var), linear_side(a[1], b[1], var));

    /*the x term over, then the number, then divide*/
    if(mp_rat_compare_zero(a[o]) != 0) {
        op = take_away(a[o], size);
        add_move(s, op, linear_side(size, zero, var));
        mp_rat_sub(a[k], a[o], a[k]);
        mp_rat_set_value(a[o], 0, 1);
        add_line(s, linear_side(a[0], b[0], var), linear_side(a[1], b[1], var));
    }
    if(mp_rat_compare_zero(a[k]) == 0) /*5=5 or 5=3, that's the answer*/
        goto done;
    if(mp_rat_compare_zero(b[k]) != 0) {
        op = take_away(b[k], size);
        add_move(s, op, number(size));
        mp_rat_sub(b[o], b[k], b[o]);
        mp_rat_set_value(b[k], 0, 1);
        add_line(s, linear_side(a[0], b[0], var), linear_side(a[1], b[1], var));
    }
    if(mp_rat_compare_value(a[k], 1, 1) != 0) {
        add_move(s, '/', number(a[k]));
        mp_rat_div(b[o], a[k], b[o]);
        mp_rat_set_value(a[k], 1, 1);
        add_line(s, linear_side(a[0], b[0], var), linear_side(a[1], b[1], var));
    }
    if(k == 1) /*4/3 = x, written the other way round*/
        add_line(s, ast_MakeSymbol(var), number(b[0]));

done:
    num_Cleanup(size);
    num_Cleanup(zero);
    mp_int_free(d);
    return !pcas_cancelled;
}

/*every tree in the canonical form, the lines are only ever drawn. 3X+5 and not 5+3X*/
static void shown(steps_t *s) {
    int i, j;

    for(i = 0; i < s->n; i++) {
        for(j = 0; j < s->line[i].eqs; j++) {
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

bool steps_fit(pcas_ast_t *left, pcas_ast_t *right, Symbol var, Method m) {
    side_t side[2];
    bool fit;

    if(!read_sides(left, right, var, side))
        return false;
    fit = m == METHOD_OPTIMAL && is_linear(side);
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
    if(m == METHOD_OPTIMAL && is_linear(side))
        ok = linear(left, right, var, side, out);

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
