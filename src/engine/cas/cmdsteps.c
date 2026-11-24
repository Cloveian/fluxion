/*Fluxion: steps for tangent(, critical(, extrema(, inflect( and implicit(, built from the *dih*rivative and equation steps. not in upstream PineappleCAS.*/

#include "steps.h"

/*d/dvar of f (at the point at, or at var itself) as *dih*rivative steps, new lines on the end of out. the last line is the slope, as a tree: new. NULL when a rule doesn't fit*/
static pcas_ast_t *derive_lines(steps_t *out, pcas_ast_t *f, pcas_ast_t *at, Symbol var) {
    static steps_t t;
    pcas_ast_t *d = ast_MakeOperator(OP_DERIV), *slope;
    int i;
    bool first = out->n == 0;

    if(d == NULL)
        return NULL;
    ast_ChildAppend(d, ast_Copy(f));
    ast_ChildAppend(d, ast_MakeSymbol(var));
    ast_ChildAppend(d, at != NULL ? ast_Copy(at) : ast_MakeSymbol(var));
    if(!steps_derivative(d, first ? out : &t)) {
        ast_Cleanup(d);
        return NULL;
    }
    ast_Cleanup(d);
    if(!first) {
        if(out->n + t.n > STEPS_MAX) {
            steps_free(&t);
            return NULL;
        }
        for(i = 0; i < t.n; i++)
            out->line[out->n++] = t.line[i];
        t.n = 0;
    }
    slope = ast_Copy(out->line[out->n - 1].eq[0].left);
    return slope;
}

/*left = right as a line, shown the way it was built*/
static step_t *put_eq(steps_t *s, pcas_ast_t *left, pcas_ast_t *right) {
    step_t *l = steps_eq(s, left, right);

    if(l != NULL)
        l->as_is = true;
    return l;
}

/*e = 0 and its solving steps after it, or just var = the roots when there are no steps for it. false if solve can't*/
static bool zeros_lines(steps_t *out, pcas_ast_t *e, Symbol var) {
    static steps_t t;
    pcas_ast_t *zero = ast_MakeInt(0), *roots[SOLVE_MAX] = {NULL};
    step_t *l;
    int n, i;
    bool ok = false;

    if(zero == NULL)
        return false;
    steps_rule(&out->line[out->n - 1], "set it to 0");
    l = steps_eq(out, ast_Copy(e), ast_Copy(zero));
    if(l == NULL)
        goto done;
    if(steps_solve(e, zero, var, METHOD_OPTIMAL, &t) && steps_append(out, &t)) {
        ok = true;
        goto done;
    }
    n = solve_ex(e, var, roots, false);
    if(n < 1 || n > STEP_EQS_MAX)
        goto done;
    steps_rule(l, "solve");
    l = steps_eq(out, ast_MakeSymbol(var), roots[0]);
    for(i = 1; l != NULL && i < n; i++) {
        l->eq[i].left = ast_MakeSymbol(var);
        l->eq[i].right = roots[i];
        l->eqs = i + 1;
    }
    for(i = 0; i < n; i++) /*taken by the line, or freed below when there was none*/
        roots[i] = l == NULL ? roots[i] : NULL;
    ok = l != NULL;
done:
    for(i = 0; i < SOLVE_MAX; i++)
        ast_Cleanup(roots[i]);
    ast_Cleanup(zero);
    return ok;
}

/*y = m(x - a) + y0, worked out into y = mx + b*/
static bool tangent_lines(steps_t *out, pcas_ast_t *f, pcas_ast_t *a, Symbol var) {
    pcas_ast_t *m, *y0, *x = ast_MakeSymbol(var), *y, *line, *sub;
    Symbol ys = var == SYM_Y ? SYM_Z : SYM_Y;

    if((m = derive_lines(out, f, a, var)) == NULL) {
        ast_Cleanup(x);
        return false;
    }
    /*the point, y at x = a*/
    steps_rule(&out->line[out->n - 1], "the point");
    sub = ast_Copy(f);
    substitute(sub, x, a);
    y0 = ast_Copy(sub);
    simplify(y0, SIMP_ALL);
    if(steps_expr(out, sub) == NULL || steps_expr(out, ast_Copy(y0)) == NULL) {
        ast_Cleanup(m);
        ast_Cleanup(y0);
        ast_Cleanup(x);
        return false;
    }
    /*y - y0 = m(x - a)*/
    steps_rule(&out->line[out->n - 1], "point-slope form");
    y = ast_MakeSymbol(ys);
    put_eq(out, ast_MakeBinary(OP_ADD, ast_Copy(y), ast_MakeBinary(OP_MULT, ast_MakeInt(-1), ast_Copy(y0))),
           ast_MakeBinary(OP_MULT, ast_Copy(m), ast_MakeBinary(OP_ADD, ast_Copy(x), ast_MakeBinary(OP_MULT, ast_MakeInt(-1), ast_Copy(a)))));
    steps_rule(&out->line[out->n - 1], "solve for y");
    line = ast_MakeBinary(OP_ADD, ast_MakeBinary(OP_MULT, ast_Copy(m), ast_Copy(x)),
                          ast_MakeBinary(OP_ADD, ast_MakeBinary(OP_MULT, ast_MakeInt(-1), ast_MakeBinary(OP_MULT, ast_Copy(m), ast_Copy(a))), ast_Copy(y0)));
    simplify(line, SIMP_ALL);
    steps_eq(out, y, line);
    ast_Cleanup(m);
    ast_Cleanup(y0);
    ast_Cleanup(x);
    return !pcas_cancelled;
}

/*f'' at each of the zeros of f', the number against 0: under is a max, over a min. false when there are none to show or one isn't a number*/
static bool sign_lines(steps_t *out, pcas_ast_t *d1, pcas_ast_t *d2, Symbol var) {
    pcas_ast_t *roots[SOLVE_MAX] = {NULL}, *x = ast_MakeSymbol(var), *v;
    step_t *l = NULL;
    int n = solve_ex(d1, var, roots, false), i;
    double dv;
    bool ok = n >= 1 && n <= STEP_EQS_MAX;

    steps_rule(&out->line[out->n - 1], "f'' there, under 0 is a max and over 0 a min");
    for(i = 0; ok && i < n; i++) {
        v = ast_Copy(d2);
        substitute(v, x, roots[i]);
        simplify(v, SIMP_ALL);
        ok = approx_double(v, &dv);
        if(!ok) {
            ast_Cleanup(v);
            break;
        }
        if(i == 0) {
            l = steps_eq(out, v, ast_MakeInt(0));
            ok = l != NULL;
        } else {
            l->eq[i].left = v;
            l->eq[i].right = ast_MakeInt(0);
            l->eqs = i + 1;
        }
        if(ok)
            l->eq[i].rel = dv < 0 ? '<' : '>';
    }
    for(i = 0; i < SOLVE_MAX; i++)
        ast_Cleanup(roots[i]);
    ast_Cleanup(x);
    return ok;
}

/*dy/dx = -(dF/dx)/(dF/dy): both partials with their steps (the other letter stays put), then the quotient*/
static bool implicit_lines(steps_t *out, pcas_ast_t *f, Symbol y, Symbol x) {
    pcas_ast_t *fx = derive_lines(out, f, NULL, x), *fy = NULL, *ans;

    if(fx != NULL) {
        steps_rule(&out->line[out->n - 1], "now with respect to y");
        fy = derive_lines(out, f, NULL, y);
    }
    if(fx == NULL || fy == NULL || is_ast_int(fy, 0)) {
        ast_Cleanup(fx);
        ast_Cleanup(fy);
        return false;
    }
    steps_rule(&out->line[out->n - 1], "dy/dx = -(dF/dx)/(dF/dy)");
    ans = ast_MakeBinary(OP_DIV, ast_MakeBinary(OP_MULT, ast_MakeInt(-1), fx), fy);
    simplify(ans, SIMP_ALL);
    return steps_expr(out, ans) != NULL;
}

bool steps_command(int kind, pcas_ast_t *f, pcas_ast_t *a, Symbol var, steps_t *out) {
    pcas_ast_t *d1, *d2 = NULL;
    bool ok = false;

    out->n = 0;
    if(kind == STEPS_IMPLICIT) {
        ok = a != NULL && a->type == NODE_SYMBOL && implicit_lines(out, f, a->op.symbol, var);
    } else if(kind == STEPS_TANGENT) {
        ok = tangent_lines(out, f, a, var);
    } else if((d1 = derive_lines(out, f, NULL, var)) != NULL) {
        if(kind == STEPS_CRITICAL) {
            ok = zeros_lines(out, d1, var);
        } else if(kind == STEPS_INFLECT) {
            steps_rule(&out->line[out->n - 1], "the slope of that");
            d2 = derive_lines(out, d1, NULL, var);
            ok = d2 != NULL && zeros_lines(out, d2, var);
        } else if(zeros_lines(out, d1, var)) {
            steps_rule(&out->line[out->n - 1], "the slope of the slope");
            d2 = derive_lines(out, d1, NULL, var);
            ok = d2 != NULL && sign_lines(out, d1, d2, var);
        }
        ast_Cleanup(d1);
        ast_Cleanup(d2);
    }
    if(!ok || pcas_cancelled) {
        steps_free(out);
        return false;
    }
    steps_shown(out);
    return true;
}
