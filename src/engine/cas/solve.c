/*
    Fluxion: solve(), not in PineappleCAS.

    solves e = 0 for one variable over the reals, like the nspire's solve():
      var gone after simplifying   every value (0 = 0) or none (1 = 0)
      linear in var                var = -b/a. a and b can have other variables in them
      polynomial with numbers      factored (factor.c), then each factor: linear, or a
                                   quadratic by the formula. a cubic or higher factor
                                   left over means it can't be solved (yet), never a
                                   partial answer.

    Your mom
*/

#include "cas.h"

#define SIMP_SOLVE (SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_RATIONAL | SIMP_EVAL | SIMP_LIKE_TERMS)
/*no identity passes in here, solve doesnt need them and theyre the slow ones*/

typedef struct {
    int deg;
    pcas_ast_t *c[3];   /*coefficients of var^0, var^1, var^2; NULL = 0*/
    bool numeric;       /*every coefficient (all degrees) is a number*/
} coefs_t;

static bool has_var(pcas_ast_t *e, Symbol var) {
    pcas_ast_t *child;

    if(e->type == NODE_SYMBOL)
        return e->op.symbol == var;
    if(e->type == NODE_OPERATOR)
        for(child = ast_ChildGet(e, 0); child != NULL; child = child->next)
            if(has_var(child, var))
                return true;
    return false;
}

static pcas_ast_t *number(mp_small n) {
    return ast_MakeNumber(num_FromInt(n));
}

static bool is_zero(pcas_ast_t *e) {
    return e == NULL || is_ast_int(e, 0);
}

/*splits a term into coefficient * var^k: the factors without var go into coef (a
product), *k counts the powers of var. false for anything else with var in it:
sin(X), 1/X, X^Y.*/
static bool split_term(pcas_ast_t *e, Symbol var, pcas_ast_t *coef, int *k) {
    pcas_ast_t *child;

    if(!has_var(e, var)) {
        ast_ChildAppend(coef, ast_Copy(e));
        return true;
    }

    if(e->type == NODE_SYMBOL) {
        (*k)++;
        return true;
    }

    if(isoptype(e, OP_MULT)) {
        for(child = ast_ChildGet(e, 0); child != NULL; child = child->next)
            if(!split_term(child, var, coef, k))
                return false;
        return true;
    }

    if(isoptype(e, OP_DIV) && !has_var(ast_ChildGet(e, 1), var)) {
        pcas_ast_t *top = ast_MakeOperator(OP_MULT);
        bool ok;

        ast_ChildAppend(top, number(1));
        ok = split_term(ast_ChildGet(e, 0), var, top, k);
        ast_ChildAppend(coef, ast_MakeBinary(OP_DIV, top, ast_Copy(ast_ChildGet(e, 1))));
        return ok;
    }

    if(isoptype(e, OP_POW)) {
        pcas_ast_t *base = ast_ChildGet(e, 0), *exp = ast_ChildGet(e, 1);
        mp_small num, den;

        if(base->type == NODE_SYMBOL && exp->type == NODE_NUMBER
           && mp_rat_to_ints(exp->op.num, &num, &den) == MP_OK && den == 1 && num >= 1 && num <= 64) {
            *k += (int)num;
            return true;
        }
    }

    return false;
}

static void coefs_free(coefs_t *p) {
    int i;

    for(i = 0; i < 3; i++) {
        if(p->c[i] != NULL)
            ast_Cleanup(p->c[i]);
        p->c[i] = NULL;
    }
}

static bool read_term(pcas_ast_t *term, Symbol var, coefs_t *p) {
    pcas_ast_t *coef = ast_MakeOperator(OP_MULT);
    int k = 0;

    if(pcas_cancelled || coef == NULL) { /*oom, or already unwinding*/
        ast_Cleanup(coef);
        return false;
    }
    ast_ChildAppend(coef, number(1));
    if(!split_term(term, var, coef, &k)) {
        ast_Cleanup(coef);
        return false;
    }
    simplify(coef, SIMP_SOLVE);

    if(k > p->deg)
        p->deg = k;
    if(coef->type != NODE_NUMBER)
        p->numeric = false;

    if(k > 2) {
        ast_Cleanup(coef);
    } else {
        if(p->c[k] == NULL)
            p->c[k] = ast_MakeOperator(OP_ADD);
        ast_ChildAppend(p->c[k], coef);
    }
    return true;
}

/*reads e (simplified and expanded) as a polynomial in var. false if it isn't one.*/
static bool read_poly(pcas_ast_t *e, Symbol var, coefs_t *p) {
    pcas_ast_t *term;
    bool ok = true;
    int i;

    p->deg = 0;
    p->numeric = true;
    for(i = 0; i < 3; i++)
        p->c[i] = NULL;

    if(e == NULL || pcas_cancelled) /*oom, or already unwinding*/
        return false;

    if(isoptype(e, OP_ADD)) {
        for(term = ast_ChildGet(e, 0); ok && term != NULL; term = term->next)
            ok = read_term(term, var, p);
    } else {
        ok = read_term(e, var, p);
    }

    if(!ok) {
        coefs_free(p);
        return false;
    }

    for(i = 0; i < 3; i++)
        if(p->c[i] != NULL)
            simplify(p->c[i], SIMP_SOLVE);
    if(pcas_cancelled) { /*coefs half simplified, nobody downstream can read those*/
        coefs_free(p);
        return false;
    }
    return true;
}

/*-c[0] / c[1]*/
static pcas_ast_t *linear_root(coefs_t *p) {
    pcas_ast_t *r;

    if(is_zero(p->c[0]))
        return number(0);

    r = ast_MakeBinary(OP_DIV,
            ast_MakeBinary(OP_MULT, number(-1), ast_Copy(p->c[0])),
            ast_Copy(p->c[1]));
    simplify(r, SIMP_SOLVE);
    return r;
}

/*mid + sign*sqrt(q). as -b/2a +- sqrt(d/4a^2) rather than (-b +- sqrt(d))/2a: the
2a cancels inside the root, so X^2=2 gives sqrt(2), not sqrt(8)/2 (the engine
doesn't take square factors out of roots)*/
/*>w<*/
static pcas_ast_t *quadratic_root(mp_rat mid, mp_rat q, int sign) {
    pcas_ast_t *r = ast_MakeOperator(OP_ADD);

    ast_ChildAppend(r, ast_MakeNumber(num_Copy(mid)));
    if(sign != 0)
        ast_ChildAppend(r, ast_MakeBinary(OP_MULT, number(sign),
            ast_MakeBinary(OP_POW, ast_MakeNumber(num_Copy(q)), ast_MakeNumber(num_FromFraction(1, 2)))));
    simplify(r, SIMP_SOLVE);
    return r;
}

/*out[n] = r, unless r never got made (oom). the sweep found sort_roots tripping on
a NULL in the middle of the list*/
static int add_root(pcas_ast_t **out, int n, pcas_ast_t *r) {
    if(r != NULL)
        out[n++] = r;
    return n;
}

/*real roots of a*X^2 + b*X + c, all numbers, added at out[n]. returns the new count.*/
static int solve_quadratic(coefs_t *p, pcas_ast_t **out, int n) {
    mp_rat a, b, c, mid, q, t;

    /*if read_poly got cut short (cancel, oom) the coefs are still sums, and
    op.num off an operator node is a garbage pointer. the sweep found that one*/
    if(pcas_cancelled)
        return SOLVE_UNKNOWN;
    a = num_Copy(p->c[2]->op.num);
    b = is_zero(p->c[1]) ? num_FromInt(0) : num_Copy(p->c[1]->op.num);
    c = is_zero(p->c[0]) ? num_FromInt(0) : num_Copy(p->c[0]->op.num);
    mid = num_FromInt(0);
    q = num_FromInt(0);
    t = num_FromInt(0);

    /*mid = -b/2a, q = mid^2 - c/a (= (b^2 - 4ac)/4a^2)*/
    mp_rat_set_value(t, -2, 1);
    mp_rat_div(b, a, mid);
    mp_rat_div(mid, t, mid);
    mp_rat_mul(mid, mid, q);
    mp_rat_div(c, a, t);
    mp_rat_sub(q, t, q);

    if(mp_rat_compare_zero(q) == 0) {
        n = add_root(out, n, quadratic_root(mid, q, 0));
    } else if(mp_rat_compare_zero(q) > 0) {
        n = add_root(out, n, quadratic_root(mid, q, -1)); /*smallest first*/
        n = add_root(out, n, quadratic_root(mid, q, 1));
    }

    num_Cleanup(a);
    num_Cleanup(b);
    num_Cleanup(c);
    num_Cleanup(mid);
    num_Cleanup(q);
    num_Cleanup(t);
    return n;
}

/*roots of one factor of the factored polynomial, added at out[n]. returns the
new count, or SOLVE_UNKNOWN.*/
static int solve_factor(pcas_ast_t *f, Symbol var, pcas_ast_t **out, int n) {
    pcas_ast_t *child;
    coefs_t p;

    if(pcas_cancelled || f == NULL) /*oom, or already unwinding*/
        return SOLVE_UNKNOWN;

    if(!has_var(f, var))
        return n;

    if(isoptype(f, OP_MULT)) {
        for(child = ast_ChildGet(f, 0); child != NULL && n >= 0; child = child->next)
            n = solve_factor(child, var, out, n);
        return n;
    }
    if(isoptype(f, OP_DIV))
        return has_var(ast_ChildGet(f, 1), var) ? SOLVE_UNKNOWN : solve_factor(ast_ChildGet(f, 0), var, out, n);
    if(isoptype(f, OP_POW))
        return solve_factor(ast_ChildGet(f, 0), var, out, n);   /*(X+2)^2: X+2 = 0*/

    if(!read_poly(f, var, &p))
        return SOLVE_UNKNOWN;

    if(p.deg == 1)
        n = add_root(out, n, linear_root(&p));
    else if(p.deg == 2 && p.numeric)
        n = solve_quadratic(&p, out, n);
    else
        n = SOLVE_UNKNOWN;      /*TODO cubics someday, cardano looks awful though*/

    coefs_free(&p);
    return n;
}

/*smallest first, where both are numbers. others keep their order.*/
static void sort_roots(pcas_ast_t **out, int n) {
    int i, j;

    for(i = 1; i < n; i++) {
        pcas_ast_t *r = out[i];

        for(j = i; j > 0 && r->type == NODE_NUMBER && out[j - 1]->type == NODE_NUMBER
                   && mp_rat_compare(out[j - 1]->op.num, r->op.num) > 0; j--)
            out[j] = out[j - 1];
        out[j] = r;
    }
}

int solve(pcas_ast_t *expr, Symbol var, pcas_ast_t **out) {
    pcas_ast_t *e = ast_Copy(expr);
    coefs_t p;
    int n = SOLVE_UNKNOWN, i;

    if(e == NULL) /*Fluxion: oom*/
        return SOLVE_UNKNOWN;

    simplify(e, SIMP_SOLVE);
    expand(e, EXP_ALL);
    simplify(e, SIMP_SOLVE);
    if(pcas_cancelled) { /*half expanded, don't read it as a polynomial*/
        ast_Cleanup(e);
        return SOLVE_UNKNOWN;
    }

    if(read_poly(e, var, &p)) {
        if(p.deg == 0) {
            if(is_zero(p.c[0]))
                n = SOLVE_ALL;
            else if(p.c[0]->type == NODE_NUMBER)
                n = 0;
        } else if(p.deg == 1) {
            out[0] = linear_root(&p);
            n = 1;
        } else if(p.numeric && p.deg <= SOLVE_MAX) {
            factor(e, FAC_POLYNOMIAL);
            n = solve_factor(e, var, out, 0);
        }
        coefs_free(&p);
    }

    if(pcas_cancelled) /*half a root list isn't an answer either*/
        n = SOLVE_UNKNOWN;

    if(n == SOLVE_UNKNOWN) {
        /*roots found before giving up aren't all of them*/
        for(i = 0; i < SOLVE_MAX && out[i] != NULL; i++) {
            ast_Cleanup(out[i]);
            out[i] = NULL;
        }
    } else if(n > 0) {
        sort_roots(out, n);
    }

    ast_Cleanup(e);
    return n;
}
