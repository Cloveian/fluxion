/*Fluxion: steps for a linear system, elimination with the whole system on every line. not in upstream PineappleCAS.*/

#include "steps.h"

typedef mp_rat matrix_t[SYSTEM_MAX][SYSTEM_MAX + 1]; /*a row is the coefficients, then the right side*/

static void matrix_free(matrix_t a, int n) {
    int i, j;

    for(i = 0; i < n; i++)
        for(j = 0; j <= n; j++) {
            num_Cleanup(a[i][j]);
            a[i][j] = NULL;
        }
}

/*the number a tree folds to, false if it isn't one*/
static bool read_number(pcas_ast_t *e, mp_rat out) {
    pcas_ast_t *c = ast_Copy(e);
    bool ok;

    if(c == NULL)
        return false;
    fold_fraction(c);
    ok = c->type == NODE_NUMBER;
    if(ok)
        mp_rat_copy(c->op.num, out);
    ast_Cleanup(c);
    return ok;
}

/*row i as ax + by + cz, the right side kept apart. NULL when every coefficient is 0*/
static pcas_ast_t *row_left(matrix_t a, int i, int n, const Symbol *vars) {
    pcas_ast_t *sum = ast_MakeOperator(OP_ADD), *t;
    int j;

    if(sum == NULL)
        return NULL;
    for(j = 0; j < n; j++) {
        if(mp_rat_compare_zero(a[i][j]) == 0)
            continue;
        t = ast_MakeSymbol(vars[j]);
        if(mp_rat_compare_value(a[i][j], 1, 1) != 0)
            t = ast_MakeBinary(OP_MULT, steps_number(a[i][j]), t);
        ast_ChildAppend(sum, t);
    }
    if(ast_ChildLength(sum) == 1) {
        t = ast_ChildRemoveIndex(sum, 0);
        ast_Cleanup(sum);
        return t;
    }
    if(ast_ChildLength(sum) == 0) {
        ast_Cleanup(sum);
        return NULL;
    }
    return sum;
}

/*left = right as equation i of line l, the first one makes the line. takes both*/
static step_t *put_eq(steps_t *s, step_t *l, int i, pcas_ast_t *left, pcas_ast_t *right) {
    if(i == 0) {
        l = steps_eq(s, left, right);
        if(l != NULL)
            l->list = true;
        return l;
    }
    l->eq[i].left = left;
    l->eq[i].right = right;
    l->eqs = i + 1;
    return l;
}

/*the system as one line, an equation each*/
static bool add_system(steps_t *s, matrix_t a, int n, const Symbol *vars) {
    step_t *l = NULL;
    int i;

    for(i = 0; i < n; i++) {
        pcas_ast_t *left = row_left(a, i, n, vars), *right = steps_number(a[i][n]);

        if(left == NULL) { /*0 = something, no one answer*/
            ast_Cleanup(right);
            return false;
        }
        l = put_eq(s, l, i, left, right);
        if(l == NULL)
            return false;
    }
    return true;
}

/*each entry of row i divided by the gcd of them all, the row stays the same equation*/
static void reduce_row(matrix_t a, int i, int n) {
    mp_int g = mp_int_alloc();
    int j;

    if(g == NULL)
        return;
    mp_int_init(g);
    for(j = 0; j <= n; j++)
        if(mp_rat_compare_zero(a[i][j]) != 0)
            mp_int_gcd(g, mp_rat_numer_ref(a[i][j]), g);
    if(mp_int_compare_value(g, 1) > 0) {
        mp_rat d = num_FromInt(0);

        mp_int_copy(g, mp_rat_numer_ref(d));
        for(j = 0; j <= n; j++)
            mp_rat_div(a[i][j], d, a[i][j]);
        num_Cleanup(d);
    }
    mp_int_free(g);
}

/*row i times f, and row k times g off it. a whole number each, no fractions come out*/
static void combine_rows(matrix_t a, int i, int k, int n, mp_rat f, mp_rat g) {
    mp_rat t = num_FromInt(0);
    int j;

    for(j = 0; j <= n; j++) {
        mp_rat_mul(a[i][j], f, a[i][j]);
        mp_rat_mul(a[k][j], g, t);
        mp_rat_sub(a[i][j], t, a[i][j]);
    }
    num_Cleanup(t);
}

/*|x| < |y|*/
static bool smaller(mp_rat x, mp_rat y) {
    mp_rat p = num_FromInt(0), q = num_FromInt(0);
    bool less;

    mp_rat_abs(x, p);
    mp_rat_abs(y, q);
    less = mp_rat_compare(p, q) < 0;
    num_Cleanup(p);
    num_Cleanup(q);
    return less;
}

static bool integers_all(matrix_t a, int n) {
    int i, j;

    for(i = 0; i < n; i++)
        for(j = 0; j <= n; j++)
            if(!mp_rat_is_integer(a[i][j]))
                return false;
    return true;
}

/*every row times what clears its bottoms*/
static void clear_fractions(matrix_t a, int n) {
    mp_int lcm = mp_int_alloc();
    int i, j;

    for(i = 0; i < n; i++) {
        mp_rat d = num_FromInt(0);

        mp_int_init_value(lcm, 1);
        for(j = 0; j <= n; j++)
            mp_int_lcm(lcm, mp_rat_denom_ref(a[i][j]), lcm);
        mp_int_copy(lcm, mp_rat_numer_ref(d));
        for(j = 0; j <= n; j++)
            mp_rat_mul(a[i][j], d, a[i][j]);
        num_Cleanup(d);
    }
    mp_int_free(lcm);
}

/*row r times the pivot number, the pivot row times its own off it, both over their gcd*/
static void cancel_row(matrix_t a, int r, int col, int n) {
    mp_int g = mp_int_alloc();
    mp_rat f = num_FromInt(0), h = num_FromInt(0);

    mp_int_gcd(mp_rat_numer_ref(a[col][col]), mp_rat_numer_ref(a[r][col]), g);
    mp_int_copy(mp_rat_numer_ref(a[col][col]), mp_rat_numer_ref(f));
    mp_int_copy(mp_rat_numer_ref(a[r][col]), mp_rat_numer_ref(h));
    mp_int_div(mp_rat_numer_ref(f), g, mp_rat_numer_ref(f), NULL);
    mp_int_div(mp_rat_numer_ref(h), g, mp_rat_numer_ref(h), NULL);
    combine_rows(a, r, col, n, f, h);
    reduce_row(a, r, n);
    mp_int_free(g);
    num_Cleanup(f);
    num_Cleanup(h);
}

/*the smallest number in column col from row col down goes on top, the rest of the column goes to 0. false when there's no one answer*/
static bool clear_column(steps_t *out, matrix_t a, int n, int col, const Symbol *vars) {
    int pivot = -1, r, j;
    bool cleared = false;

    for(r = col; r < n; r++) {
        if(mp_rat_compare_zero(a[r][col]) == 0)
            continue;
        if(pivot < 0 || smaller(a[r][col], a[pivot][col]))
            pivot = r;
    }
    if(pivot < 0)
        return false;
    if(pivot != col) {
        for(j = 0; j <= n; j++) {
            mp_rat t = a[col][j];

            a[col][j] = a[pivot][j];
            a[pivot][j] = t;
        }
        steps_rule(&out->line[out->n - 1], "swap two equations");
        if(!add_system(out, a, n, vars))
            return false;
    }
    for(r = 0; r < n; r++) {
        if(r == col || mp_rat_compare_zero(a[r][col]) == 0)
            continue;
        cancel_row(a, r, col, n);
        cleared = true;
    }
    if(cleared) {
        steps_rule(&out->line[out->n - 1], "cancel a letter from the others");
        return add_system(out, a, n, vars);
    }
    return true;
}

/*two equations where one is a line in one letter: a letter that's the other one's, then the quadratic steps for the other letter, then back*/
static bool substitution(pcas_ast_t **eqs, const Symbol *vars, steps_t *out) {
    static steps_t t;
    pcas_ast_t *e[2] = {NULL}, *lead = NULL, *rv = NULL, *s, *v, *u, *r[SOLVE_MAX] = {NULL}, *rt[SOLVE_MAX] = {NULL}, *zero = NULL;
    mp_rat tmp = num_FromInt(0);
    int i, j, k, at = -1, which = -1, m, deg;
    step_t *l;
    bool ok = false;

    out->n = 0;
    for(i = 0; i < 2; i++) { /*simplified and expanded, how poly_degree wants them*/
        e[i] = ast_Copy(eqs[i]);
        simplify(e[i], SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_RATIONAL | SIMP_EVAL | SIMP_LIKE_TERMS);
        expand(e[i], EXP_ALL);
        simplify(e[i], SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_RATIONAL | SIMP_EVAL | SIMP_LIKE_TERMS);
    }
    for(i = 0; i < 2 && at < 0; i++)
        for(j = 0; j < 2 && at < 0; j++) {
            deg = poly_degree(e[i], vars[j], &lead);
            if(deg == 1 && lead != NULL && read_number(lead, tmp)) {
                at = i;
                which = j;
            }
            ast_Cleanup(lead);
            lead = NULL;
        }
    if(at < 0)
        goto done;

    /*the first line, both as they are*/
    l = steps_eq(out, ast_Copy(e[0]), ast_MakeInt(0));
    if(l == NULL)
        goto done;
    l->eq[1].left = ast_Copy(e[1]);
    l->eq[1].right = ast_MakeInt(0);
    l->eqs = 2;
    l->list = true;

    /*the line for letter v by itself*/
    if(solve_ex(e[at], vars[which], r, false) != 1 || r[0] == NULL)
        goto done;
    rv = r[0];
    r[0] = NULL;
    simplify(rv, SIMP_ALL);
    v = ast_MakeSymbol(vars[which]);
    u = ast_MakeSymbol(vars[1 - which]);
    steps_rule(&out->line[out->n - 1], "solve the line for one letter");
    l = steps_eq(out, ast_Copy(v), ast_Copy(rv));
    if(l == NULL)
        goto done;
    l->eq[1].left = ast_Copy(e[1 - at]);
    l->eq[1].right = ast_MakeInt(0);
    l->eqs = 2;
    l->list = true;

    /*in the other one*/
    s = ast_Copy(e[1 - at]);
    substitute(s, v, rv);
    steps_rule(l, "put it in the other one");
    zero = ast_MakeInt(0);
    if(steps_eq(out, ast_Copy(s), ast_Copy(zero)) == NULL || !steps_solve(s, zero, vars[1 - which], METHOD_OPTIMAL, &t) || !steps_append(out, &t)) {
        ast_Cleanup(s);
        goto done;
    }
    ast_Cleanup(s);

    /*the roots on the last line, each with the letter it gives*/
    l = &out->line[out->n - 1];
    for(k = 0; k < l->eqs; k++) {
        if(l->eq[k].pm != NULL || l->eq[k].right == NULL || l->eq[k].left->type != NODE_SYMBOL)
            goto done;
        rt[k] = ast_Copy(l->eq[k].right);
    }
    m = l->eqs;
    steps_rule(l, "the other letter for each");
    for(k = 0; k < m; k++) {
        pcas_ast_t *w = ast_Copy(rv), *first, *second;
        step_t *pair;

        substitute(w, u, rt[k]);
        simplify(w, SIMP_ALL);
        if(k > 0)
            steps_rule(&out->line[out->n - 1], "and the other");
        first = ast_MakeSymbol(vars[0]);
        second = ast_MakeSymbol(vars[1]);
        pair = steps_eq(out, first, which == 0 ? w : rt[k]);
        if(pair == NULL) {
            ast_Cleanup(second);
            ast_Cleanup(which == 0 ? rt[k] : w);
            rt[k] = NULL;
            goto done;
        }
        pair->eq[1].left = second;
        pair->eq[1].right = which == 0 ? rt[k] : w;
        pair->eqs = 2;
        pair->list = true;
        rt[k] = NULL;
    }
    ok = !pcas_cancelled;

done:
    num_Cleanup(tmp);
    for(i = 0; i < SOLVE_MAX; i++)
        ast_Cleanup(rt[i]);
    for(i = 0; i < 2; i++)
        ast_Cleanup(e[i]);
    ast_Cleanup(rv);
    ast_Cleanup(zero);
    for(i = 0; i < SOLVE_MAX; i++)
        ast_Cleanup(r[i]);
    if(ok)
        steps_shown(out);
    else
        steps_free(out);
    return ok;
}

bool steps_system(pcas_ast_t **eqs, int n, const Symbol *vars, steps_t *out) {
    matrix_t a = {{NULL}};
    bool ok = false;
    int i, j;

    out->n = 0;
    if(n < 2 || n > SYSTEM_MAX)
        return false;
    for(i = 0; i < n; i++)
        for(j = 0; j <= n; j++)
            a[i][j] = num_FromInt(0);
    for(i = 0; i < n; i++) {
        pcas_ast_t *row[SYSTEM_MAX] = {NULL}, *rhs = NULL;
        bool good = system_row(eqs[i], n, vars, row, &rhs) && read_number(rhs, a[i][n]);

        for(j = 0; j < n; j++) {
            good = good && row[j] != NULL && read_number(row[j], a[i][j]);
            ast_Cleanup(row[j]);
        }
        ast_Cleanup(rhs);
        if(!good) {
            if(n == 2) { /*not lines, a line and a curve is substitution*/
                matrix_free(a, n);
                return substitution(eqs, vars, out);
            }
            goto done;
        }
    }

    if(!add_system(out, a, n, vars))
        goto done;
    if(!integers_all(a, n)) {
        clear_fractions(a, n);
        steps_rule(&out->line[out->n - 1], "clear the fractions");
        if(!add_system(out, a, n, vars))
            goto done;
    }
    for(i = 0; i < n; i++)
        if(!clear_column(out, a, n, i, vars))
            goto done;

    /*one letter an equation now: a x = d, so x = d/a*/
    {
        bool all_one = true;
        step_t *l = NULL;

        for(i = 0; i < n; i++)
            all_one = all_one && mp_rat_compare_value(a[i][i], 1, 1) == 0;
        if(!all_one) {
            steps_rule(&out->line[out->n - 1], "divide each by its number");
            for(i = 0; i < n; i++) {
                mp_rat v = num_FromInt(0);

                mp_rat_div(a[i][n], a[i][i], v);
                l = put_eq(out, l, i, ast_MakeSymbol(vars[i]), steps_number(v));
                num_Cleanup(v);
                if(l == NULL)
                    goto done;
            }
        }
    }
    ok = !pcas_cancelled;

done:
    matrix_free(a, n);
    if(ok)
        steps_shown(out);
    else
        steps_free(out);
    return ok;
}
