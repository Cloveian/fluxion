/*
    Fluxion: solve_system(), not in PineappleCAS. linear systems the way you'd do
    them on paper, elimination. n equations in n unknowns, n up to SYSTEM_MAX,
    which is plenty for anything off a worksheet.

    the coefficient of x in a linear equation is just d/dx of it, and the constant
    is what's left with every variable set to 0. that's the whole reader, no term
    splitting. then "is it actually linear" is one check, the equation minus the
    pieces has to come out as 0. anything else isn't linear

    two of those in two letters go by substitution instead, see by_substitution.
    more than two nonlinear ones is SOLVE_UNKNOWN

    coefficients can have other letters in them. a letter pivot gets assumed
    nonzero, same as solve() does for a/x. a number pivot is taken first when
    there is one so that assumption is made as rarely as possible
*/

#include "cas.h"

#define SIMP_SYS (SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_RATIONAL | SIMP_EVAL | SIMP_LIKE_TERMS)

static bool zero(pcas_ast_t *e) {
    return e != NULL && is_ast_int(e, 0);
}

/*a (op) b, both copied, simplified. NULL when oom. number/number is done straight
  in imath, no tree to build and simplify for it (and the sign lands where we want)*/
static pcas_ast_t *combine(OperatorType op, pcas_ast_t *a, pcas_ast_t *b) {
    pcas_ast_t *e;

    if(op == OP_DIV && a->type == NODE_NUMBER && b->type == NODE_NUMBER && !zero(b)) {
        e = ast_MakeInt(0);
        if(e == NULL)
            return NULL;
        if(mp_rat_div(a->op.num, b->op.num, e->op.num) == MP_OK) {
            /*imath leaves the sign wherever it landed, 17/-28 prints as that*/
            if(mp_int_compare_zero(mp_rat_denom_ref(e->op.num)) < 0) {
                mp_int_neg(mp_rat_numer_ref(e->op.num), mp_rat_numer_ref(e->op.num));
                mp_int_neg(mp_rat_denom_ref(e->op.num), mp_rat_denom_ref(e->op.num));
            }
            return e;
        }
        ast_Cleanup(e);
    }
    e = ast_MakeBinary(op, ast_Copy(a), ast_Copy(b));
    if(e == NULL || pcas_cancelled)
        return e;
    simplify(e, SIMP_SYS);
    return e;
}

/*a - b, as a + (-1)b since the engine has no minus node*/
static pcas_ast_t *minus(pcas_ast_t *a, pcas_ast_t *b) {
    pcas_ast_t *neg = ast_MakeBinary(OP_MULT, ast_MakeInt(-1), ast_Copy(b)), *e;

    e = combine(OP_ADD, a, neg);
    ast_Cleanup(neg);
    return e;
}

/*row r of the matrix gets its coefficients and constant read off eq. false if eq
  isn't linear in the vars (or oom). the constant goes on the right, so the row
  reads coef . var = const*/
bool system_row(pcas_ast_t *eq, int n, const Symbol *vars, pcas_ast_t **row, pcas_ast_t **rhs) {
    pcas_ast_t *e = ast_Copy(eq), *check, *v, *z = ast_MakeInt(0);
    int i;

    if(e == NULL || z == NULL) {
        ast_Cleanup(e);
        ast_Cleanup(z);
        return false;
    }
    simplify(e, SIMP_SYS);
    expand(e, EXP_ALL);
    simplify(e, SIMP_SYS);

    for(i = 0; i < n; i++) {
        v = ast_MakeSymbol(vars[i]);
        row[i] = ast_Copy(e);
        if(v == NULL || row[i] == NULL || pcas_cancelled) {
            ast_Cleanup(v);
            ast_Cleanup(e);
            ast_Cleanup(z);
            return false;
        }
        derivative(row[i], v, v);
        simplify(row[i], SIMP_SYS);
        ast_Cleanup(v);
    }

    /*the constant, then the linear check. check = e - sum(coef var) - const*/
    *rhs = ast_Copy(e);
    check = ast_Copy(e);
    if(*rhs == NULL || check == NULL) {
        ast_Cleanup(check);
        ast_Cleanup(e);
        ast_Cleanup(z);
        return false;
    }
    for(i = 0; i < n; i++) {
        pcas_ast_t *term;

        v = ast_MakeSymbol(vars[i]);
        substitute(*rhs, v, z);
        term = combine(OP_MULT, row[i], v);
        ast_Cleanup(v);
        v = minus(check, term);
        ast_Cleanup(term);
        ast_Cleanup(check);
        check = v;
        if(check == NULL)
            break;
    }
    simplify(*rhs, SIMP_SYS);
    if(check != NULL) {
        v = minus(check, *rhs);
        ast_Cleanup(check);
        check = v;
    }
    ast_Cleanup(e);
    ast_Cleanup(z);
    if(check == NULL || pcas_cancelled || !zero(check)) {
        ast_Cleanup(check);
        return false;
    }
    ast_Cleanup(check);

    /*moved over to the right, ax + by + c = 0 is ax + by = -c*/
    {
        pcas_ast_t *neg = ast_MakeInt(-1), *r = combine(OP_MULT, neg, *rhs);

        ast_Cleanup(neg);
        ast_Cleanup(*rhs);
        *rhs = r;
    }
    return *rhs != NULL;
}

static void swap_rows(pcas_ast_t *m[SYSTEM_MAX][SYSTEM_MAX], pcas_ast_t **rhs, int n, int a, int b) {
    pcas_ast_t *t;
    int j;

    for(j = 0; j < n; j++) {
        t = m[a][j];
        m[a][j] = m[b][j];
        m[b][j] = t;
    }
    t = rhs[a];
    rhs[a] = rhs[b];
    rhs[b] = t;
}

/*a bottom that comes out 0, 1/(4-Y) or Y^-1 at Y = 4 or 0. looked for before
  simplifying since simplify happily turns 0(6/0) into 0*/
static bool zero_bottom(pcas_ast_t *e) {
    pcas_ast_t *k, *b = NULL;
    bool z;

    if(e->type != NODE_OPERATOR)
        return false;
    if(isoptype(e, OP_DIV))
        b = ast_Copy(ast_ChildGet(e, 1));
    else if(isoptype(e, OP_POW) && is_negative_for_sure(ast_ChildGet(e, 1)))
        b = ast_Copy(ast_ChildGet(e, 0));
    if(b != NULL) {
        simplify(b, SIMP_SYS);
        z = zero(b);
        ast_Cleanup(b);
        if(z)
            return true;
    }
    for(k = ast_ChildGet(e, 0); k != NULL; k = k->next)
        if(zero_bottom(k))
            return true;
    return false;
}

/*sum + |e| as a decimal. false when e has none*/
static bool add_abs(mp_rat sum, pcas_ast_t *e) {
    pcas_ast_t *v = approx_number(e);

    if(v == NULL)
        return false;
    mp_rat_abs(v->op.num, v->op.num);
    mp_rat_add(sum, v->op.num, sum);
    ast_Cleanup(v);
    return true;
}

/*|d| over 1e-5 (1 + |x| + |y|)^2. floats are ~7 *dih*gits and the terms grow like the
  values squared, so loose. past it is a for sure not 0*/
static bool past_loose(mp_rat d, pcas_ast_t **x) {
    mp_rat tol = mp_rat_alloc(), t = mp_rat_alloc();
    bool past;

    mp_rat_set_value(tol, 1, 1);
    add_abs(tol, x[0]);
    add_abs(tol, x[1]);
    mp_rat_mul(tol, tol, tol);
    mp_rat_set_value(t, 1, 100000);
    mp_rat_mul(tol, t, tol);
    past = mp_rat_compare_unsigned(d, tol) > 0;
    mp_rat_free(tol);
    mp_rat_free(t);
    return past;
}

/*|d| within 1e-5 (1 + |each term of e|). false when a term has no decimal*/
static bool tiny(pcas_ast_t *e, mp_rat d) {
    pcas_ast_t *k;
    mp_rat scale = mp_rat_alloc(), t = mp_rat_alloc();
    bool known = true;

    mp_rat_set_value(scale, 1, 1);
    if(isoptype(e, OP_ADD))
        for(k = ast_ChildGet(e, 0); known && k != NULL; k = k->next)
            known = add_abs(scale, k);
    else
        known = add_abs(scale, e);
    mp_rat_set_value(t, 1, 100000);
    mp_rat_mul(scale, t, scale);
    known = known && mp_rat_compare_unsigned(d, scale) <= 0;
    mp_rat_free(scale);
    mp_rat_free(t);
    return known;
}

/*the letters only under + * and positive whole powers, or over a bottom without them*/
static bool polynomial(pcas_ast_t *e, const Symbol *vars) {
    pcas_ast_t *k, *p;

    if(e->type != NODE_OPERATOR || (!has_symbol(e, vars[0]) && !has_symbol(e, vars[1])))
        return true;
    if(isoptype(e, OP_POW)) {
        p = ast_ChildGet(e, 1);
        return p->type == NODE_NUMBER && mp_rat_is_integer(p->op.num) && mp_rat_compare_zero(p->op.num) > 0
            && polynomial(ast_ChildGet(e, 0), vars);
    }
    if(isoptype(e, OP_DIV)) {
        p = ast_ChildGet(e, 1);
        return !has_symbol(p, vars[0]) && !has_symbol(p, vars[1]) && polynomial(ast_ChildGet(e, 0), vars);
    }
    if(!isoptype(e, OP_ADD) && !isoptype(e, OP_MULT))
        return false;
    for(k = ast_ChildGet(e, 0); k != NULL; k = k->next)
        if(!polynomial(k, vars))
            return false;
    return true;
}

/*eq with the pair put in, false only when it's for sure not 0. undefined there, or
  an i turning up (a root of a negative, outside the real domain) is a no too. a letter
  left in it is a keep. poly: x came out of polynomials the whole way*/
static bool fits(pcas_ast_t *eq, const Symbol *vars, pcas_ast_t **x, bool poly) {
    pcas_ast_t *e = ast_Copy(eq), *d, *s;
    bool ok;
    int i;

    if(e == NULL)
        return true;
    for(i = 0; i < 2; i++) {
        s = ast_MakeSymbol(vars[i]);
        if(s != NULL)
            substitute(e, s, x[i]);
        ast_Cleanup(s);
    }
    if(zero_bottom(e)) {
        ast_Cleanup(e);
        return false;
    }
    /*a sure 0 by decimal skips the exact way, ~1.2M work on the cardano pairs of y=x^3,
      x^2=y+3. only when everything solved on the way was a polynomial, those roots are
      exact so the pair really fits and a near 0 is one. any root in there and isolate or
      squaring can make pairs up a decimal can't tell (x = 10000, y = -100 misses y = sqrt(x)
      by just 200, x^3+1000000=999992 at x = 2 by 16). and only the yes side, unsimplifed
      x^2=2000000 with y=x^3-2828427124 has ~300 of float noise*/
    d = poly ? approx_number(e) : NULL;
    if(d != NULL) {
        bool sure = tiny(e, d->op.num);

        ast_Cleanup(d);
        if(sure) {
            ast_Cleanup(e);
            return true;
        }
    }
    simplify(e, SIMP_SYS);
    expand(e, EXP_ALL);
    simplify(e, SIMP_SYS);
    if(pcas_cancelled) {
        ast_Cleanup(e);
        return true;
    }
    /*big whole powers left in, 9901^2 and 330033^2 when a root came out whole. a decimal of those is past the float noise*/
    if(!zero(e) && !has_imaginary_node(e)) {
        eval(e, EVAL_EASY | EVAL_POWERS_FIT);
        simplify(e, SIMP_SYS);
    }
    if(has_imaginary_node(e) || (e->type == NODE_NUMBER && !zero(e))) {
        ast_Cleanup(e);
        return false;
    }
    if(zero(e)) {
        ast_Cleanup(e);
        return true;
    }

    d = approx_number(e);
    ast_Cleanup(e);
    if(d == NULL)
        return true;
    ok = !past_loose(d->op.num, x);
    ast_Cleanup(d);
    return ok;
}

/*below 0 for sure, or as a decimal past the float noise. 1-2^(3/2) is a sum, is_negative_for_sure can't tell*/
static bool negative(pcas_ast_t *e) {
    pcas_ast_t *d;
    bool neg;

    if(is_negative_for_sure(e))
        return true;
    /*all numbers, then the exact sign. 1-10000001^2/10000000^2 is -2e-7, under the float noise allowence*/
    if((d = ast_Copy(e)) != NULL) {
        eval(d, EVAL_EASY | EVAL_POWERS_FIT);
        simplify(d, SIMP_EVAL | SIMP_RATIONAL);
        neg = d->type == NODE_NUMBER && mp_rat_compare_zero(d->op.num) < 0;
        if(d->type == NODE_NUMBER) {
            ast_Cleanup(d);
            return neg;
        }
        ast_Cleanup(d);
    }
    if((d = approx_number(e)) == NULL)
        return false;
    neg = mp_rat_compare_zero(d->op.num) < 0 && !tiny(e, d->op.num);
    ast_Cleanup(d);
    return neg;
}

/*a power that's an even root, 1/2 or 3/2. comes as a number or as a 1/2 tree*/
static bool even_root(pcas_ast_t *p) {
    mp_rat r;
    bool even;

    if(p->type == NODE_NUMBER)
        return !mp_rat_is_integer(p->op.num) && mp_int_is_even(mp_rat_denom_ref(p->op.num));
    if(!isoptype(p, OP_DIV) || ast_ChildGet(p, 0)->type != NODE_NUMBER || ast_ChildGet(p, 1)->type != NODE_NUMBER
       || zero(ast_ChildGet(p, 1)))
        return false;
    r = num_FromInt(0);
    even = r != NULL && mp_rat_div(ast_ChildGet(p, 0)->op.num, ast_ChildGet(p, 1)->op.num, r) == MP_OK
        && !mp_rat_is_integer(r) && mp_int_is_even(mp_rat_denom_ref(r));
    num_Cleanup(r);
    return even;
}

/*an i, or an even root of a negative. simplify leaves (-39996)^(1/2) like that,
  has_imaginary_node only sees an i*/
static bool not_real(pcas_ast_t *e) {
    pcas_ast_t *k, *p;

    if(has_imaginary_node(e))
        return true;
    if(e->type != NODE_OPERATOR)
        return false;
    p = ast_ChildGet(e, 1);
    if(isoptype(e, OP_POW) && even_root(p) && negative(ast_ChildGet(e, 0)))
        return true;
    if(isoptype(e, OP_ROOT) && ast_ChildGet(e, 0)->type == NODE_NUMBER && mp_rat_is_integer(ast_ChildGet(e, 0)->op.num)
       && mp_int_is_even(mp_rat_numer_ref(ast_ChildGet(e, 0)->op.num)) && negative(p))
        return true;
    for(k = ast_ChildGet(e, 0); k != NULL; k = k->next)
        if(not_real(k))
            return true;
    return false;
}

/*the pair x goes in as set *sets unless it's already there or doesn't fit both
  equations (squaring makes some up). x is the set's now either way. false when
  there's no room. poly goes on to fits()*/
static bool add_set(pcas_ast_t **eqs, const Symbol *vars, pcas_ast_t **x, pcas_ast_t *out[][SYSTEM_MAX], int *sets, bool poly) {
    pcas_ast_t *d;
    int k, i, same;

    for(k = 0; k < *sets; k++) {
        for(same = 0, i = 0; i < 2; i++) {
            d = minus(out[k][i], x[i]);
            if(zero(d))
                same++;
            ast_Cleanup(d);
        }
        if(same == 2)
            break;
    }
    if(k < *sets || zero_bottom(x[0]) || zero_bottom(x[1]) || not_real(x[0]) || not_real(x[1])
       || !fits(eqs[0], vars, x, poly) || !fits(eqs[1], vars, x, poly)) {
        ast_Cleanup(x[0]);
        ast_Cleanup(x[1]);
        return true;
    }
    if(*sets == SYSTEM_SETS) {
        ast_Cleanup(x[0]);
        ast_Cleanup(x[1]);
        return false;
    }
    out[*sets][0] = x[0];
    out[*sets][1] = x[1];
    (*sets)++;
    return true;
}

/*smallest first letter first, by the decimal like solve sorts. letters stay put*/
static void sort_sets(pcas_ast_t *out[][SYSTEM_MAX], int sets) {
    pcas_ast_t *val[SYSTEM_SETS], *a, *b, *rv;
    int i, j;

    for(i = 0; i < sets; i++)
        val[i] = approx_number(out[i][0]);
    for(i = 1; i < sets; i++) {
        a = out[i][0];
        b = out[i][1];
        rv = val[i];
        for(j = i; j > 0 && rv != NULL && val[j - 1] != NULL
                   && mp_rat_compare(val[j - 1]->op.num, rv->op.num) > 0; j--) {
            out[j][0] = out[j - 1][0];
            out[j][1] = out[j - 1][1];
            val[j] = val[j - 1];
        }
        out[j][0] = a;
        out[j][1] = b;
        val[j] = rv;
    }
    for(i = 0; i < sets; i++)
        ast_Cleanup(val[i]);
}

static bool half(pcas_ast_t *e) {
    if(e->type == NODE_NUMBER)
        return mp_rat_compare_value(e->op.num, 1, 2) == 0;
    return isoptype(e, OP_DIV) && is_ast_int(ast_ChildGet(e, 0), 1) && is_ast_int(ast_ChildGet(e, 1), 2);
}

/*sqrt(a)sqrt(b) into sqrt(ab), only where a and b come out >= 0 as decimals since
  that's when it's true. solve gives the circle's x as sqrt(5+y)sqrt(20-4y)/2 and at
  y = 2sqrt(5) the two roots never cancel down to sqrt(5) on their own*/
static void merge_roots(pcas_ast_t *e) {
    pcas_ast_t *k, *next, *first = NULL, *d, *m;
    bool ok;

    if(e->type != NODE_OPERATOR)
        return;
    for(k = ast_ChildGet(e, 0); k != NULL; k = k->next)
        merge_roots(k);
    if(!isoptype(e, OP_MULT))
        return;
    for(k = ast_ChildGet(e, 0); k != NULL; k = next) {
        next = k->next;
        if(!isoptype(k, OP_POW) || !half(ast_ChildGet(k, 1)))
            continue;
        d = approx_number(ast_ChildGet(k, 0));
        ok = d != NULL && mp_rat_compare_zero(d->op.num) >= 0;
        ast_Cleanup(d);
        if(!ok)
            continue;
        if(first == NULL) {
            first = k;
            continue;
        }
        ast_ChildRemove(e, k);
        m = ast_MakeBinary(OP_MULT, ast_ChildRemoveIndex(first, 0), ast_ChildRemoveIndex(k, 0));
        if(m != NULL)
            ast_ChildInsert(first, m, 0);
        ast_Cleanup(k);
    }
}

/*solve(), and when that gives up, the top of e over one denominator. 6/Y + Y = 5 is
  what substitution makes all the time and solve() can't do a letter on the bottom.
  the roots that zero the bottom are made up, fits() drops those*/
static int solve_top(pcas_ast_t *e, Symbol var, pcas_ast_t **out) {
    pcas_ast_t *c;
    int r = solve(e, var, out);

    if(r != SOLVE_UNKNOWN || pcas_cancelled || (c = ast_Copy(e)) == NULL)
        return r;
    simplify(c, SIMP_SYS);
    comdenom(c, false);
    if(!pcas_cancelled && isoptype(c, OP_DIV) && has_symbol(ast_ChildGet(c, 1), var))
        r = solve(ast_ChildGet(c, 0), var, out);
    ast_Cleanup(c);
    return r;
}

/*eqs[i] solved for vars[v], each of those put into the other one, that solved for
  the other letter, and back into the first for vars[v]. every pair checked in both.
  SOLVE_UNKNOWN when solve() gives up anywhere, or on a trig family (an n in the
  roots that the equations didn't have), the pairs would need an n each*/
static int by_substitution(pcas_ast_t **eqs, int i, int v, const Symbol *vars, pcas_ast_t *out[][SYSTEM_MAX]) {
    pcas_ast_t *first[SOLVE_MAX] = { NULL }, *second[SOLVE_MAX], *sv, *sw, *e, *x[2];
    int w = 1 - v, nf, ns, a, b, k, sets = 0, result = 0;
    bool fam = !has_symbol(eqs[0], SYM_N) && !has_symbol(eqs[1], SYM_N); /*an n is solve's unless they had one*/
    bool both = polynomial(eqs[0], vars) && polynomial(eqs[1], vars), poly;

    sv = ast_MakeSymbol(vars[v]);
    sw = ast_MakeSymbol(vars[w]);
    nf = sv != NULL && sw != NULL ? solve_top(eqs[i], vars[v], first) : SOLVE_UNKNOWN;
    if(nf == SOLVE_ALL) /*eqs[i] says nothing about vars[v], so try another way*/
        result = SOLVE_UNKNOWN;
    else if(nf < 0)
        result = nf;

    for(a = 0; a < nf && result == 0; a++) {
        if(fam && has_symbol(first[a], SYM_N)) {
            result = SOLVE_UNKNOWN;
            break;
        }
        e = ast_Copy(eqs[1 - i]);
        if(e == NULL) {
            result = SOLVE_UNKNOWN;
            break;
        }
        substitute(e, sv, first[a]);
        poly = both && polynomial(e, vars); /*a sqrt from the first solve in here and isolate can make roots up*/
        for(b = 0; b < SOLVE_MAX; b++)
            second[b] = NULL;
        ns = solve_top(e, vars[w], second);
        ast_Cleanup(e);
        if(ns < 0) /*SOLVE_ALL is a whole curve of them, the unknown is unknown*/
            result = ns;

        for(b = 0; b < ns && result == 0; b++) {
            if(fam && has_symbol(second[b], SYM_N)) {
                result = SOLVE_UNKNOWN;
                break;
            }
            x[w] = ast_Copy(second[b]);
            x[v] = ast_Copy(first[a]);
            if(x[v] != NULL && x[w] != NULL) {
                substitute(x[v], sw, x[w]);
                simplify(x[v], SIMP_SYS);
                merge_roots(x[v]);
                expand(x[v], EXP_ALL);
                eval(x[v], EVAL_EASY | EVAL_POWERS_FIT); /*simplify leaves 1000^3 and 998^2, then like terms with a sqrt in them don't meet*/
                simplify(x[v], SIMP_SYS);
            }
            if(x[v] == NULL || x[w] == NULL || pcas_cancelled) {
                ast_Cleanup(x[v]);
                ast_Cleanup(x[w]);
                result = SOLVE_UNKNOWN;
                break;
            }
            if(!add_set(eqs, vars, x, out, &sets, poly))
                result = SOLVE_UNKNOWN; /*more than fit, and half of them isn't an answer*/
        }
        for(b = 0; b < ns; b++)
            ast_Cleanup(second[b]);
    }

    for(a = 0; a < nf; a++)
        ast_Cleanup(first[a]);
    ast_Cleanup(sv);
    ast_Cleanup(sw);
    if(pcas_cancelled)
        result = SOLVE_UNKNOWN;
    if(result == 0 && sets > 0) {
        sort_sets(out, sets);
        return sets;
    }
    for(k = 0; k < sets; k++) {
        ast_Cleanup(out[k][0]);
        ast_Cleanup(out[k][1]);
        out[k][0] = out[k][1] = NULL;
    }
    return result;
}

/*a line in one of the letters (number in front) goes first since solving that one is clean,
  then anything solve() can do*/
static int substitutions(pcas_ast_t **eqs, const Symbol *vars, pcas_ast_t *out[][SYSTEM_MAX]) {
    pcas_ast_t *lead;
    int pass, i, v, r;
    bool line;

    for(pass = 0; pass < 2; pass++)
        for(i = 0; i < 2; i++)
            for(v = 0; v < 2; v++) {
                lead = NULL;
                line = poly_degree(eqs[i], vars[v], &lead) == 1 && lead != NULL && lead->type == NODE_NUMBER;
                ast_Cleanup(lead);
                if(pcas_cancelled)
                    return SOLVE_UNKNOWN;
                if(line != (pass == 0))
                    continue;
                r = by_substitution(eqs, i, v, vars, out);
                if(r != SOLVE_UNKNOWN || pcas_cancelled)
                    return r;
            }
    return SOLVE_UNKNOWN;
}

/*two equations, two letters, not both lines. when one is a cubic or more in just one letter
  the other goes mod that one first, its roots are cos and cardano forms simplify can't fold
  back. x(y^3-3y-1)=1 with y^3=3y+1 is 3 pairs of 1/0 without it*/
static int nonlinear(pcas_ast_t **eqs, const Symbol *vars, pcas_ast_t *out[][SYSTEM_MAX]) {
    pcas_ast_t *cut[2], *rest;
    int i, v, r;

    for(i = 0; i < 2; i++)
        for(v = 0; v < 2; v++)
            if(!has_symbol(eqs[i], vars[1 - v]) && poly_degree(eqs[i], vars[v], NULL) >= 3
               && (rest = poly_remainder(eqs[1 - i], eqs[i], vars[v])) != NULL) {
                cut[i] = eqs[i];
                cut[1 - i] = rest;
                r = substitutions(cut, vars, out);
                ast_Cleanup(rest);
                return r;
            }
    return substitutions(eqs, vars, out);
}

int solve_system(pcas_ast_t **eqs, int n, const Symbol *vars, pcas_ast_t *out[][SYSTEM_MAX]) {
    pcas_ast_t *m[SYSTEM_MAX][SYSTEM_MAX], *rhs[SYSTEM_MAX];
    bool pivoted[SYSTEM_MAX], linear = true;
    int i, j, col, r, result = 1;

    if(n < 1 || n > SYSTEM_MAX || pcas_cancelled)
        return SOLVE_UNKNOWN;
    for(i = 0; i < n; i++) {
        rhs[i] = NULL;
        pivoted[i] = false;
        for(j = 0; j < n; j++)
            m[i][j] = NULL;
    }

    for(i = 0; i < n; i++)
        if(!system_row(eqs[i], n, vars, m[i], &rhs[i])) {
            result = SOLVE_UNKNOWN;
            linear = false;
            goto done;
        }

    /*gauss-jordan, so no back substitution step. one column at a time*/
    for(col = 0; col < n && result > 0; col++) {
        int pivot = -1;

        for(r = col; r < n; r++) {
            if(m[r][col] == NULL || zero(m[r][col]))
                continue;
            if(m[r][col]->type == NODE_NUMBER) { /*a number beats a letter*/
                pivot = r;
                break;
            }
            if(pivot < 0)
                pivot = r;
        }
        if(pivot < 0)
            continue; /*this column's got nothing left, sorted out below*/
        swap_rows(m, rhs, n, col, pivot);
        pivoted[col] = true;

        for(r = 0; r < n; r++) {
            pcas_ast_t *f;

            if(r == col || zero(m[r][col]))
                continue;
            f = combine(OP_DIV, m[r][col], m[col][col]);
            for(j = 0; j < n; j++) {
                pcas_ast_t *t = combine(OP_MULT, f, m[col][j]), *d = minus(m[r][j], t);

                ast_Cleanup(t);
                ast_Cleanup(m[r][j]);
                m[r][j] = d;
            }
            {
                pcas_ast_t *t = combine(OP_MULT, f, rhs[col]), *d = minus(rhs[r], t);

                ast_Cleanup(t);
                ast_Cleanup(rhs[r]);
                rhs[r] = d;
            }
            ast_Cleanup(f);
            if(pcas_cancelled) {
                result = SOLVE_UNKNOWN;
                goto done;
            }
        }
    }

    /*a row of zeros means the equations overlapped. 0 = 5 is no solution, 0 = 0
      is infinitely many (and no solution wins if both turn up)*/
    for(r = 0; r < n && result > 0; r++) {
        bool empty = true;

        for(j = 0; j < n; j++)
            if(!zero(m[r][j]))
                empty = false;
        if(empty)
            result = zero(rhs[r]) ? SOLVE_ALL : 0;
    }
    for(r = 0; r < n && result > 0; r++)
        if(!pivoted[r])
            result = SOLVE_ALL;
    if(result == SOLVE_ALL)
        for(r = 0; r < n; r++)
            if(pivoted[r] == false && !zero(rhs[r])) {
                /*unreachable-ish, kept so a weird matrix cant say "many" with a 0 = 5 in it*/
                result = 0;
            }

    if(result > 0)
        for(i = 0; i < n; i++) {
            out[0][i] = combine(OP_DIV, rhs[i], m[i][i]);
            if(out[0][i] == NULL || pcas_cancelled) {
                result = SOLVE_UNKNOWN;
                break;
            }
        }

done:
    if(result <= 0 && result != SOLVE_ALL)
        for(i = 0; i < n; i++) {
            ast_Cleanup(out[0][i]);
            out[0][i] = NULL;
        }
    for(i = 0; i < n; i++) {
        ast_Cleanup(rhs[i]);
        for(j = 0; j < n; j++)
            ast_Cleanup(m[i][j]);
    }
    if(pcas_cancelled)
        return SOLVE_UNKNOWN;
    if(!linear && n == 2)
        result = nonlinear(eqs, vars, out);
    return result;
}
