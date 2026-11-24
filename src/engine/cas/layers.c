/*Fluxion: steps for f(x) = c as layers, one a line, for what the quadratic ones in steps.c don't take: sqrt(x+3) = 5, 2^x = 8, abs(x) = 4, ln(x) = 2. not in upstream PineappleCAS.*/

#include <math.h>
#include <string.h>

#include "steps.h"

/*the one child with the letter in it, NULL when it's in none or several*/
static pcas_ast_t *var_child(pcas_ast_t *e, Symbol var) {
    pcas_ast_t *c, *in = NULL;

    for(c = ast_ChildGet(e, 0); c != NULL; c = c->next) {
        if(!has_symbol(c, var))
            continue;
        if(in != NULL)
            return NULL;
        in = c;
    }
    return in;
}

/*the children of e that aren't in, under e's own operator. one left is just itself*/
static pcas_ast_t *others(pcas_ast_t *e, pcas_ast_t *in) {
    pcas_ast_t *out = rest_of(e, in, optype(e)), *only;

    if(out == NULL || ast_ChildLength(out) != 1)
        return out;
    only = ast_ChildRemoveIndex(out, 0);
    ast_Cleanup(out);
    return only;
}

/*the k of x^(1/k), the way normalize leaves a root*/
static bool root_index(pcas_ast_t *p, mp_small *k) {
    return isoptype(p, OP_DIV) && is_ast_int(ast_ChildGet(p, 0), 1) && whole_number(ast_ChildGet(p, 1), k) && *k >= 2;
}

/*next[m..2m) = the negatives of next[0..m), for +-. false when a line has no room for them*/
static bool both_signs(pcas_ast_t **next, int m) {
    int j;

    if(m * 2 > STEP_EQS_MAX)
        return false;
    for(j = 0; j < m; j++)
        next[m + j] = ast_MakeBinary(OP_MULT, ast_MakeInt(-1), ast_Copy(next[j]));
    return true;
}

/*a line with left = each right*/
static step_t *add_line(steps_t *s, pcas_ast_t *left, pcas_ast_t **right, int m) {
    step_t *l = steps_eq(s, ast_Copy(left), ast_Copy(right[0]));
    int j;

    for(j = 1; l != NULL && j < m; j++) {
        l->eq[j].left = ast_Copy(left);
        l->eq[j].right = ast_Copy(right[j]);
    }
    if(l != NULL)
        l->eqs = m;
    return l;
}

static bool matches(pcas_ast_t *a, pcas_ast_t *b) {
    double x, y;
    pcas_ast_t *d;
    bool same;

    if(approx_double(a, &x) && approx_double(b, &y))
        return fabs(x - y) <= 1e-6 * (1 + fabs(y));
    d = ast_MakeBinary(OP_ADD, ast_Copy(a), ast_MakeBinary(OP_MULT, ast_MakeInt(-1), ast_Copy(b)));
    simplify(d, SIMP_ALL);
    same = d != NULL && is_ast_int(d, 0);
    ast_Cleanup(d);
    return same;
}

/*the answers that solve agrees with go in the last line, the rest were made by squaring. false if it ends anywhere else than solve*/
static bool check_answers(pcas_ast_t *left, pcas_ast_t *right, Symbol var, steps_t *s, pcas_ast_t **cand, int m) {
    pcas_ast_t *roots[SOLVE_MAX] = {0}, *e, *keep[STEP_EQS_MAX];
    int n, i, j, kept = 0;
    bool ok;

    e = ast_MakeBinary(OP_ADD, ast_Copy(left), ast_MakeBinary(OP_MULT, ast_MakeInt(-1), ast_Copy(right)));
    n = solve_ex(e, var, roots, true);
    ast_Cleanup(e);
    for(i = 0; i < m; i++) {
        for(j = 0; j < n && !matches(cand[i], roots[j]); j++);
        if(j < n)
            keep[kept++] = cand[i];
    }
    ok = n > 0 && kept == n && !pcas_cancelled;
    for(i = 0; i < SOLVE_MAX; i++)
        ast_Cleanup(roots[i]);
    if(!ok || kept == m)
        return ok;
    steps_rule(&s->line[s->n - 1], "check the answers");
    ok = add_line(s, s->line[s->n - 1].eq[0].left, keep, kept) != NULL;
    return ok;
}

/*an x on both sides now (a root squared away): the quadratic steps carry on from the last line, its first line is that one. fit first, solve would come back in here for a shape it can't read*/
static bool hand_off(pcas_ast_t *L, pcas_ast_t *R, Symbol var, steps_t *s) {
    static steps_t t; /*~1 KB, off the stack*/
    step_t *last = &s->line[s->n - 1];
    int k, j;

    if(!steps_fit(L, R, var, METHOD_OPTIMAL) || !steps_solve(L, R, var, METHOD_OPTIMAL, &t))
        return false;
    if(s->n + t.n - 1 > STEPS_MAX || t.n < 2) {
        steps_free(&t);
        return false;
    }
    last->move = t.line[0].move;
    last->by = t.line[0].by;
    memcpy(last->rules, t.line[0].rules, sizeof(last->rules));
    for(j = 0; j < t.line[0].eqs; j++) {
        ast_Cleanup(t.line[0].eq[j].left);
        ast_Cleanup(t.line[0].eq[j].right);
        ast_Cleanup(t.line[0].eq[j].pm);
        ast_Cleanup(t.line[0].eq[j].den);
    }
    for(k = 1; k < t.n; k++)
        s->line[s->n++] = t.line[k];
    t.n = 0;
    return true;
}

bool steps_layers(pcas_ast_t *left, pcas_ast_t *right, Symbol var, steps_t *out) {
    pcas_ast_t *L = ast_Copy(left), *R[STEP_EQS_MAX] = {NULL}, *tmp;
    pcas_ast_t *by = NULL, *next[STEP_EQS_MAX] = {NULL}; /*this layer's, freed at done if it bails halfway*/
    int m = 1, i, j;
    bool ok = false, handed = false;

    out->n = 0;
    R[0] = ast_Copy(right);
    simplify(L, SIMP_NORMALIZE | SIMP_EVAL);
    simplify(R[0], SIMP_NORMALIZE | SIMP_EVAL);
    if(has_symbol(R[0], var) && !has_symbol(L, var)) {
        tmp = L;
        L = R[0];
        R[0] = tmp;
    }
    if(!has_symbol(L, var) || add_line(out, L, R, 1) == NULL)
        goto done;

    while((L->type != NODE_SYMBOL || has_symbol(R[0], var)) && !pcas_cancelled) {
        pcas_ast_t *in, *a, *b;
        const char *rule = NULL;
        char move = 0;
        bool exact = false; /*the log and root results get the full simplify*/
        mp_small n;
        step_t *prev;
        int m2 = m;

        /*an x on both sides: sqrt(2x+3) = x squares first, anything else goes to the quadratic steps*/
        if(has_symbol(R[0], var) && !(isoptype(L, OP_POW) && root_index(ast_ChildGet(L, 1), &n))) {
            if(m != 1 || !hand_off(L, R[0], var, out))
                goto done;
            handed = true;
            break;
        }
        if(L->type != NODE_OPERATOR || (in = var_child(L, var)) == NULL)
            goto done;
        a = ast_ChildGet(L, 0);
        b = ast_ChildGet(L, 1);

        switch(optype(L)) {
        case OP_ADD:
            by = others(L, in);
            for(j = 0; j < m; j++)
                next[j] = ast_MakeBinary(OP_ADD, ast_Copy(R[j]), ast_MakeBinary(OP_MULT, ast_MakeInt(-1), ast_Copy(by)));
            move = '_';
            if(is_negative_for_sure(by)) {
                absolute_val(by);
                move = '+';
            }
            break;
        case OP_MULT:
            by = others(L, in);
            for(j = 0; j < m; j++)
                next[j] = ast_MakeBinary(OP_DIV, ast_Copy(R[j]), ast_Copy(by));
            move = '/';
            break;
        case OP_DIV:
            if(in != a)
                goto done;
            by = ast_Copy(b);
            for(j = 0; j < m; j++)
                next[j] = ast_MakeBinary(OP_MULT, ast_Copy(R[j]), ast_Copy(b));
            move = '*';
            break;
        case OP_POW:
            exact = true;
            if(in == b) { /*a^x = v is x = log of v*/
                for(j = 0; j < m; j++) {
                    if(is_negative_for_sure(R[j]) || is_ast_int(R[j], 0))
                        goto done;
                    next[j] = ast_MakeBinary(OP_LOG, ast_Copy(a), ast_Copy(R[j]));
                }
                rule = "take the log of both sides";
            } else if(root_index(b, &n)) { /*a root, undone by the power*/
                for(j = 0; j < m; j++)
                    next[j] = ast_MakeBinary(OP_POW, ast_Copy(R[j]), ast_MakeInt(n));
                rule = n == 2 ? "square both sides" : n == 3 ? "cube both sides" : "raise both sides to the power";
            } else if(whole_number(b, &n) && n >= 2) {
                pcas_ast_t *k = ast_MakeBinary(OP_DIV, ast_MakeInt(1), ast_MakeInt(n));

                for(j = 0; j < m; j++) {
                    if(n % 2 == 0 && is_negative_for_sure(R[j])) {
                        ast_Cleanup(k);
                        goto done;
                    }
                    next[j] = ast_MakeBinary(OP_POW, ast_Copy(R[j]), ast_Copy(k));
                }
                ast_Cleanup(k);
                if(n % 2 == 0) { /*both signs of it*/
                    if(!both_signs(next, m))
                        goto done;
                    m2 = m * 2;
                }
                move = n == 2 ? 'r' : 'n';
                rule = n == 2 ? NULL : n % 2 == 0 ? "even root of both sides" : "root of both sides";
            } else {
                goto done;
            }
            break;
        case OP_LOG:
            if(in != b)
                goto done;
            exact = true;
            for(j = 0; j < m; j++)
                next[j] = ast_MakeBinary(OP_POW, ast_Copy(a), ast_Copy(R[j]));
            rule = "raise both sides to the base";
            break;
        case OP_ABS:
            exact = true;
            for(j = 0; j < m; j++) {
                if(is_negative_for_sure(R[j]))
                    goto done;
                next[j] = ast_Copy(R[j]);
            }
            if(!both_signs(next, m))
                goto done;
            m2 = m * 2;
            rule = "two cases";
            break;
        default:
            goto done;
        }

        for(j = 0; j < m2; j++) {
            if(next[j] == NULL)
                goto done;
            simplify(next[j], exact ? SIMP_ALL : SIMP_LIGHT | SIMP_LIKE_TERMS);
        }
        prev = &out->line[out->n - 1];
        if(rule != NULL)
            steps_rule(prev, rule);
        else
            prev->move = move;
        prev->by = by;
        by = NULL;
        for(j = 0; j < m; j++)
            ast_Cleanup(R[j]);
        for(j = 0; j < m2; j++) {
            R[j] = next[j];
            next[j] = NULL;
        }
        m = m2;
        tmp = ast_Copy(in);
        ast_Cleanup(L);
        L = tmp;
        if(add_line(out, L, R, m) == NULL)
            goto done;
    }
    if(pcas_cancelled || (L->type != NODE_SYMBOL && !handed))
        goto done;

    if(handed) { /*the answers are in the last line the quadratic steps ended on*/
        step_t *last = &out->line[out->n - 1];

        for(j = 0; j < last->eqs; j++)
            if(last->eq[j].pm != NULL || last->eq[j].right == NULL || last->eq[j].left->type != NODE_SYMBOL)
                goto done;
        for(j = 0; j < last->eqs; j++) {
            ast_Cleanup(R[j]);
            R[j] = ast_Copy(last->eq[j].right);
        }
        m = last->eqs;
    }
    ok = check_answers(left, right, var, out, R, m);

done:
    ast_Cleanup(L);
    ast_Cleanup(by);
    for(i = 0; i < STEP_EQS_MAX; i++) {
        ast_Cleanup(R[i]);
        ast_Cleanup(next[i]);
    }
    if(ok)
        steps_shown(out);
    else
        steps_free(out);
    return ok;
}
