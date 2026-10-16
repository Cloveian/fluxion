#include "cas.h"

/*Handles gcd for AB, BC gcd = B*/
static pcas_ast_t *gcd_mult(pcas_ast_t *mult, pcas_ast_t *b) {
    unsigned i;
    pcas_ast_t *current_gcd, *copy;

    if(pcas_cancelled) /*Fluxion: stop here, the answer's being thrown away*/
        return NULL;

    copy = ast_Copy(b);

    current_gcd = ast_MakeOperator(OP_MULT);

    for(i = 0; i < ast_ChildLength(mult); i++) {
        pcas_ast_t *inner_gcd;
        pcas_ast_t *child = ast_ChildGet(mult, i);

        inner_gcd = gcd(child, copy);

        ast_ChildAppend(current_gcd, inner_gcd);

        replace_node(copy, ast_MakeBinary(OP_DIV, ast_Copy(copy), ast_Copy(inner_gcd)));

        simplify(copy, SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_RATIONAL | SIMP_EVAL);
    }

    ast_Cleanup(copy);

    return current_gcd;
}

/*Handles gcd for A + AB, A gcd = A*/
static pcas_ast_t *gcd_add(pcas_ast_t *add, pcas_ast_t *b) {
    unsigned i;
    pcas_ast_t *current_gcd;

    current_gcd = gcd(ast_ChildGet(add, 0), b);

    for(i = 1; i < ast_ChildLength(add); i++) {
        pcas_ast_t *temp_gcd, *child;

        child = ast_ChildGet(add, i);
        temp_gcd = gcd(current_gcd, child);

        ast_Cleanup(current_gcd);
        current_gcd = temp_gcd;
    }

    return current_gcd;
}

static pcas_ast_t *gcd_div(pcas_ast_t *div, pcas_ast_t *b) {
    pcas_ast_t *num1, *num2, *den1, *den2, *num_g, *den_g;

    if(pcas_cancelled) /*Fluxion: stop here, the answer's being thrown away*/
        return NULL;

    num1 = ast_ChildGet(div, 0);
    den1 = ast_ChildGet(div, 1);

    if(isoptype(b, OP_DIV)) {
        num2 = ast_ChildGet(b, 0);
        den2 = ast_ChildGet(b, 1);
    }
    else {
        num2 = b;
        den2 = ast_MakeNumber(num_FromInt(1));
    }

    num_g = gcd(num1, num2);
    den_g = gcd(den1, den2);

    if(!isoptype(b, OP_DIV))
        ast_Cleanup(den2);

    return ast_MakeBinary(OP_DIV, num_g, den_g);
}

/*gcd of both bases, raised to smallest power*/
static pcas_ast_t *gcd_pow(pcas_ast_t *pow, pcas_ast_t *b) {
    pcas_ast_t *base1, *power1;

    if(pcas_cancelled) /*Fluxion: stop here, the answer's being thrown away*/
        return NULL;

    base1 = ast_ChildGet(pow, 0);
    power1 = ast_ChildGet(pow, 1);

    if(pow == NULL || b == NULL || power1 == NULL) /*Fluxion: oom*/
        return NULL;

    /*We purposefully ignore A^X, A^Y and only yield when
    we can determine the numerical powers*/

    if(isoptype(b, OP_POW)) {
        pcas_ast_t *power2, *base2;

        power1 = ast_ChildGet(pow, 1);
        base2 = ast_ChildGet(b, 0);
        power2 = ast_ChildGet(b, 1);

        if(base2 == NULL || power2 == NULL) /*Fluxion: oom*/
            return NULL;

        if(power1->type == NODE_NUMBER && power2->type == NODE_NUMBER) {
            bool use_first;
            pcas_ast_t *current_gcd;

            use_first = mp_rat_compare(power1->op.num, power2->op.num) < 0;

            current_gcd = gcd(base1, base2);

            return ast_MakeBinary(OP_POW, current_gcd, use_first ? ast_Copy(power1) : ast_Copy(power2));
        }

    } else if(power1->type == NODE_NUMBER && mp_rat_compare_value(power1->op.num, 1, 1) >= 0 && ast_Compare(base1, b)) {
        return ast_Copy(b);
    }

    return ast_MakeNumber(num_FromInt(1));
}

pcas_ast_t *gcd(pcas_ast_t *a, pcas_ast_t *b) {
    pcas_ast_t *ret = NULL;

    if(pcas_cancelled) /*Fluxion: stop here, the answer's being thrown away*/
        return NULL;

    if(a == NULL || b == NULL) /*Fluxion: oom*/
        return NULL;

    if(ast_Compare(a, b))
        return ast_Copy(a);

    if(a->type == NODE_NUMBER && b->type == NODE_NUMBER) {
        if(mp_rat_is_integer(a->op.num) && mp_rat_is_integer(b->op.num)) {
            mp_rat gcd;
            gcd = num_FromInt(1);

            mp_int_gcd(&a->op.num->num, &b->op.num->num, &gcd->num);

            return ast_MakeNumber(gcd);
        }
    }

    if(isoptype(a, OP_MULT))
        ret = gcd_mult(a, b);
    else if(isoptype(b, OP_MULT))
        ret = gcd_mult(b, a);
    else if(isoptype(a, OP_ADD))
        ret = gcd_add(a, b);
    else if(isoptype(b, OP_ADD))
        ret = gcd_add(b, a);
    else if(isoptype(a, OP_POW))
        ret = gcd_pow(a, b);
    else if(isoptype(b, OP_POW))
        ret = gcd_pow(b, a);
    else if(isoptype(a, OP_DIV))
        ret = gcd_div(a, b);
    else if(isoptype(b, OP_DIV))
        ret = gcd_div(b, a);

    if(ret != NULL) {
        /*Don't simplify identities*/
        simplify(ret, SIMP_ALL ^ SIMP_ID_ALL);
        return ret;
    }

    return ast_MakeNumber(num_FromInt(1));
}

bool factor_addition(pcas_ast_t *e, unsigned char flags) {
    pcas_ast_t *child;
    bool changed = false;

    /*Fluxion: cancelled, the simplifies below do nothing, and pairing up terms
    that aren't simplified any more grows them (took longer than finishing)*/
    if(pcas_cancelled || e == NULL || e->type != NODE_OPERATOR) /*Fluxion: flag first, e can be NULL under oom*/
        return false;

    for(child = ast_ChildGet(e, 0); child != NULL; child = child->next)
        changed |= factor_addition(child, flags);

    if(isoptype(e, OP_ADD)) {
        unsigned i, j;
        pcas_ast_t *g;

        for(i = 0; i < ast_ChildLength(e) && !pcas_check(); i++) {
            pcas_ast_t *a = ast_ChildGet(e, i);

            for(j = i + 1; j < ast_ChildLength(e) && !pcas_check(); j++) {
                pcas_ast_t *b = ast_ChildGet(e, j);

                g = gcd(a, b);

                if(g == NULL) /*Fluxion: oom*/
                    break;

                if(!is_ast_int(g, 1)) {
                    bool can_factor;
                    pcas_ast_t *append, *first, *second;

                    first = ast_MakeBinary(OP_DIV,
                                            ast_Copy(a),
                                            ast_Copy(g)
                                        );
                    second = ast_MakeBinary(OP_DIV,
                                            ast_Copy(b),
                                            ast_Copy(g)
                                        );

                    append = ast_MakeBinary(OP_MULT,
                                    ast_Copy(g),
                                    ast_MakeBinary(OP_ADD,
                                        first,
                                        second
                                    )
                                );

                    simplify(first, SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_RATIONAL | SIMP_EVAL);
                    simplify(second, SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_RATIONAL | SIMP_EVAL);

                    if(first == NULL || second == NULL) { /*Fluxion: oom*/
                        ast_Cleanup(append);
                        ast_Cleanup(g);
                        break;
                    }

                    if(first->type == NODE_NUMBER && second->type == NODE_NUMBER)
                        can_factor = flags & FAC_SIMPLE_ADDITION_EVALUATEABLE;
                    else
                        can_factor = flags & FAC_SIMPLE_ADDITION_NONEVALUATEABLE;

                    if(can_factor) {
                        simplify(append, SIMP_NORMALIZE | SIMP_RATIONAL | SIMP_EVAL);
                        ast_ChildAppend(e, append);

                        ast_Cleanup(ast_ChildRemove(e, a));
                        ast_Cleanup(ast_ChildRemove(e, b));

                        ast_Cleanup(g);

                        changed = true;
                        break;
                    } else {
                        ast_Cleanup(append);
                        ast_Cleanup(g);
                    }

                } else {
                    ast_Cleanup(g);
                }

            }
        }
    }

    /*Take care of add nodes with one child*/
    simplify(e, SIMP_COMMUTATIVE);

    return changed;
}

/*
    Fluxion: polynomial factoring (FAC_POLYNOMIAL).

    a polynomial in one variable with rational coefficients is factored over the
    rationals, like the nspire's factor(): the number in front, the lowest power
    of the variable, then (qX - p) for every rational root p/q (rational root
    theorem). a part with no rational roots (X^2 + 1, X^2 - 2) is left as it is.
*/

#define POLY_MAX_DEGREE 32

/*roots are only searched for when the lowest and highest coefficients are at
most this, so listing their divisors stays quick on the calculator*/
#define POLY_MAX_ROOT_SEARCH 1000000L
#define POLY_MAX_DIVISORS 240   /*the most any number up to 1000000 has*/

/*all of the working state, on the heap: on the eZ80, a stack frame over 127
bytes makes every access to it several instructions long (this was 10 KB of
code with the arrays as locals)*/
typedef struct {
    Symbol var;
    int deg;
    mp_int c[POLY_MAX_DEGREE + 1];      /*c[i] is the coefficient of var^i*/
    mp_rat rc[POLY_MAX_DEGREE + 1];     /*while reading: the same, as fractions*/
    mp_rat scale;                       /*the number in front*/
    int low;                            /*the power of var taken out*/

    int found;                          /*roots p/q found, and how many times each*/
    mp_small fp[POLY_MAX_DEGREE], fq[POLY_MAX_DEGREE];
    int mult[POLY_MAX_DEGREE];

    mp_small dp[POLY_MAX_DIVISORS], dq[POLY_MAX_DIVISORS];
    mp_int tmp[3];
} poly_t;

static void poly_free(poly_t *p);

static poly_t *poly_new(void) {
    poly_t *p = pcas_calloc(1, sizeof(poly_t));
    int i;

    if(p == NULL)
        return NULL;

    p->scale = mp_rat_alloc();
    for(i = 0; i < 3; i++)
        p->tmp[i] = mp_int_alloc();
    if(p->scale != NULL && p->tmp[0] != NULL && p->tmp[1] != NULL && p->tmp[2] != NULL)
        return p;

    poly_free(p);
    return NULL;
}

static void poly_free(poly_t *p) {
    int i;

    for(i = 0; i <= POLY_MAX_DEGREE; i++) {
        if(p->c[i] != NULL)
            mp_int_free(p->c[i]);
        if(p->rc[i] != NULL)
            mp_rat_free(p->rc[i]);
    }
    for(i = 0; i < 3; i++)
        if(p->tmp[i] != NULL)
            mp_int_free(p->tmp[i]);
    if(p->scale != NULL)
        mp_rat_free(p->scale);
    free(p);
}

/*reads one term, a number times var^k. coef is multiplied by its number and
*k increased by its power. *var is the variable so far, SYM_INVALID if none yet.*/
static bool poly_term(pcas_ast_t *e, Symbol *var, mp_rat coef, int *k) {
    pcas_ast_t *child;

    if(pcas_cancelled) /*Fluxion: stop here, the answer's being thrown away*/
        return false;

    switch(e->type) {
    case NODE_NUMBER:
        return mp_rat_mul(coef, e->op.num, coef) == MP_OK;
    case NODE_SYMBOL:
        if(e->op.symbol == SYM_PI || e->op.symbol == SYM_EULER || e->op.symbol == SYM_IMAG)
            return false;
        if(*var != SYM_INVALID && *var != e->op.symbol)
            return false;
        *var = e->op.symbol;
        (*k)++;
        return true;
    case NODE_OPERATOR:
        break;
    }

    switch(optype(e)) {
    case OP_MULT:
        for(child = ast_ChildGet(e, 0); child != NULL; child = child->next)
            if(!poly_term(child, var, coef, k))
                return false;
        return true;

    case OP_DIV: {
        pcas_ast_t *den = ast_ChildGet(e, 1);

        if(den == NULL) /*Fluxion: oom*/
            return false;
        if(den->type != NODE_NUMBER || mp_rat_compare_zero(den->op.num) == 0)
            return false;
        if(!poly_term(ast_ChildGet(e, 0), var, coef, k))
            return false;
        return mp_rat_div(coef, den->op.num, coef) == MP_OK;
    }

    case OP_POW: {
        pcas_ast_t *base = ast_ChildGet(e, 0), *exp = ast_ChildGet(e, 1);
        mp_small num, den;

        if(base == NULL || exp == NULL) /*Fluxion: oom*/
            return false;
        if(base->type != NODE_SYMBOL || exp->type != NODE_NUMBER)
            return false;
        if(mp_rat_to_ints(exp->op.num, &num, &den) != MP_OK || den != 1 || num < 1 || num > POLY_MAX_DEGREE)
            return false;
        if(!poly_term(base, var, coef, k))
            return false;
        *k += (int)num - 1;
        return true;
    }

    default:
        return false;
    }
}

/*reads a sum of terms into integer coefficients: e = scale * (c[0] + c[1]X + ...).
false if e isn't a polynomial of degree 1 or more in one variable.*/
static bool poly_read(pcas_ast_t *e, poly_t *p) {
    mp_rat coef = p->scale;     /*free until the end*/
    mp_int lcm;
    pcas_ast_t *term;
    bool ok;
    int i;

    if(pcas_cancelled) /*Fluxion: stop here, the answer's being thrown away*/
        return false;
    if(e == NULL) /*Fluxion: oom*/
        return false;
    ok = isoptype(e, OP_ADD);

    p->var = SYM_INVALID;
    p->deg = -1;

    for(term = ok ? ast_ChildGet(e, 0) : NULL; ok && term != NULL; term = term->next) {
        int k = 0;

        mp_rat_set_value(coef, 1, 1);
        ok = poly_term(term, &p->var, coef, &k) && k <= POLY_MAX_DEGREE;
        if(ok && p->rc[k] == NULL)
            ok = (p->rc[k] = mp_rat_alloc()) != NULL;
        if(ok)
            ok = mp_rat_add(p->rc[k], coef, p->rc[k]) == MP_OK;
    }

    for(i = 0; ok && i <= POLY_MAX_DEGREE; i++)
        if(p->rc[i] != NULL && mp_rat_compare_zero(p->rc[i]) != 0)
            p->deg = i;
    if(!ok || p->var == SYM_INVALID || p->deg < 1)
        return false;

    /*clear the denominators: times their lcm, which scale takes back out*/
    /*fart3-(slowed+reverb).flac*/
    lcm = mp_rat_denom_ref(p->scale);
    mp_int_set_value(mp_rat_numer_ref(p->scale), 1);
    mp_int_set_value(lcm, 1);
    for(i = 0; i <= p->deg; i++)
        if(p->rc[i] != NULL)
            mp_int_lcm(lcm, mp_rat_denom_ref(p->rc[i]), lcm);

    for(i = 0; i <= p->deg; i++) {
        if((p->c[i] = mp_int_alloc()) == NULL)
            return false;
        if(p->rc[i] != NULL) {
            mp_int_div(lcm, mp_rat_denom_ref(p->rc[i]), p->c[i], NULL);
            mp_int_mul(p->c[i], mp_rat_numer_ref(p->rc[i]), p->c[i]);
        }
    }
    return true;
}

/*moves the gcd of the coefficients (with the leading one's sign) into scale, and
the lowest power of the variable into low. p is then primitive, with c[0] != 0.*/
static void poly_primitive(poly_t *p) {
    mp_int g = p->tmp[0];
    int i;

    mp_int_zero(g);
    for(i = 0; i <= p->deg; i++)
        if(mp_int_compare_zero(p->c[i]) != 0)
            mp_int_gcd(g, p->c[i], g);
    if(mp_int_compare_zero(p->c[p->deg]) < 0)
        mp_int_neg(g, g);

    for(i = 0; i <= p->deg; i++)
        mp_int_div(p->c[i], g, p->c[i], NULL);
    mp_rat_mul_int(p->scale, g, p->scale);

    for(p->low = 0; mp_int_compare_zero(p->c[p->low]) == 0; p->low++);
    if(p->low > 0) {
        for(i = 0; i < p->low; i++)
            mp_int_free(p->c[i]);
        for(i = 0; i <= p->deg - p->low; i++)
            p->c[i] = p->c[i + p->low];
        for(; i <= p->deg; i++)
            p->c[i] = NULL;
        p->deg -= p->low;
    }
}

static mp_small small_gcd(mp_small a, mp_small b) {
    while(b != 0) {
        mp_small t = a % b;
        a = b;
        b = t;
    }
    return a;
}

/*positive divisors of n > 0. returns how many.*/
static int divisors(mp_small n, mp_small *out) {
    mp_small d;
    int count = 0;

    for(d = 1; d * d <= n; d++) {
        if(n % d == 0) {
            out[count++] = d;
            if(d != n / d)
                out[count++] = n / d;
        }
    }
    return count;
}

/*is p/q a root? Horner on q^deg * P(p/q), so it stays in integers (no float rounding bugs)*/
/*integer? i hardly know 'er bwahahhhaaa*/
/*you need to leave!!*/
static bool poly_is_root(poly_t *poly, mp_small p, mp_small q) {
    mp_int v = poly->tmp[0], qpow = poly->tmp[1], t = poly->tmp[2];
    int i;

    mp_int_copy(poly->c[poly->deg], v);
    mp_int_set_value(qpow, 1);
    for(i = poly->deg - 1; i >= 0; i--) {
        mp_int_mul_value(v, p, v);
        mp_int_mul_value(qpow, q, qpow);
        mp_int_mul(poly->c[i], qpow, t);
        mp_int_add(v, t, v);
    }
    return mp_int_compare_zero(v) == 0;
}

/*some rational root p/q (q > 0) of a primitive poly with c[0] != 0, by the
rational root theorem: p divides c[0], q divides c[deg]*/
static bool poly_find_root(poly_t *poly, mp_small *p, mp_small *q) {
    mp_small a0, an, *dp = poly->dp, *dq = poly->dq;
    int np, nq, i, j, sign;

    if(mp_int_to_int(poly->c[0], &a0) != MP_OK || mp_int_to_int(poly->c[poly->deg], &an) != MP_OK)
        return false;
    if(a0 < 0)
        a0 = -a0;
    if(a0 > POLY_MAX_ROOT_SEARCH || an > POLY_MAX_ROOT_SEARCH)
        return false;

    np = divisors(a0, dp);
    nq = divisors(an, dq);
    for(j = 0; j < nq; j++) {
        for(i = 0; i < np; i++) {
            if(small_gcd(dp[i], dq[j]) != 1)
                continue;
            for(sign = 1; sign >= -1; sign -= 2) {
                if(poly_is_root(poly, sign * dp[i], dq[j])) {
                    *p = sign * dp[i];
                    *q = dq[j];
                    return true;
                }
            }
        }
    }
    return false;
}

/*poly /= (qX - p), for a root p/q. synthetic division, top down:
b[n-1] = c[n]/q, b[i-1] = (c[i] + p*b[i])/q, each b[i] kept in c[i+1]*/
static void poly_divide(poly_t *poly, mp_small p, mp_small q) {
    mp_int t = poly->tmp[0];
    int i, n = poly->deg;

    mp_int_div_value(poly->c[n], q, poly->c[n], NULL);
    for(i = n - 1; i >= 1; i--) {
        mp_int_mul_value(poly->c[i + 1], p, t);
        mp_int_add(poly->c[i], t, poly->c[i]);
        mp_int_div_value(poly->c[i], q, poly->c[i], NULL);
    }

    mp_int_free(poly->c[0]);
    for(i = 0; i < n; i++)
        poly->c[i] = poly->c[i + 1];
    poly->c[n] = NULL;
    poly->deg--;
}

static pcas_ast_t *int_number(mp_int z) {
    mp_rat r = mp_rat_alloc();

    mp_int_copy(z, mp_rat_numer_ref(r));
    return ast_MakeNumber(r);
}

static pcas_ast_t *power_or_self(pcas_ast_t *e, int n) {
    return n == 1 ? e : ast_MakeBinary(OP_POW, e, ast_MakeNumber(num_FromInt(n)));
}

/*qX - p*/
static pcas_ast_t *poly_linear(Symbol var, mp_small p, mp_small q) {
    pcas_ast_t *x = ast_MakeSymbol((char)var);

    if(q != 1)
        x = ast_MakeBinary(OP_MULT, ast_MakeNumber(num_FromInt(q)), x);
    return ast_MakeBinary(OP_ADD, x, ast_MakeNumber(num_FromInt(-p)));
}

static pcas_ast_t *poly_to_ast(poly_t *poly) {
    pcas_ast_t *sum = ast_MakeOperator(OP_ADD);
    int i;

    for(i = poly->deg; i >= 0; i--) {
        pcas_ast_t *term;

        if(mp_int_compare_zero(poly->c[i]) == 0)
            continue;

        if(i == 0) {
            term = int_number(poly->c[i]);
        } else {
            term = power_or_self(ast_MakeSymbol((char)poly->var), i);
            if(mp_int_compare_value(poly->c[i], 1) != 0)
                term = ast_MakeBinary(OP_MULT, int_number(poly->c[i]), term);
        }
        ast_ChildAppend(sum, term);
    }
    return sum;
}

/*the factored form of e, or NULL if e isn't a polynomial or doesn't factor*/
static pcas_ast_t *poly_factor(pcas_ast_t *e) {
    poly_t *poly;
    pcas_ast_t *result = NULL;
    int pieces, i;

    if(pcas_cancelled) /*Fluxion: stop here, the answer's being thrown away*/
        return NULL;
    poly = poly_new();
    if(poly == NULL)
        return NULL;
    if(!poly_read(e, poly)) {
        poly_free(poly);
        return NULL;
    }
    poly_primitive(poly);

    while(poly->deg >= 1) {
        mp_small p, q;

        if(!poly_find_root(poly, &p, &q))
            break;
        poly_divide(poly, p, q);

        /*a root found again is one more power of its factor*/
        for(i = 0; i < poly->found && (poly->fp[i] != p || poly->fq[i] != q); i++);
        if(i == poly->found) {
            poly->fp[i] = p;
            poly->fq[i] = q;
            poly->mult[i] = 0;
            poly->found++;
        }
        poly->mult[i]++;
    }

    /*only a change if it's now a product of two or more things*/
    pieces = (mp_rat_compare_value(poly->scale, 1, 1) != 0) + (poly->low > 0) + (poly->deg >= 1);
    for(i = 0; i < poly->found; i++)
        pieces += poly->mult[i];

    if(pieces >= 2) {
        result = ast_MakeOperator(OP_MULT);

        if(mp_rat_compare_value(poly->scale, 1, 1) != 0) {
            ast_ChildAppend(result, ast_MakeNumber(poly->scale));
            poly->scale = NULL;
        }
        if(poly->low > 0)
            ast_ChildAppend(result, power_or_self(ast_MakeSymbol((char)poly->var), poly->low));
        for(i = 0; i < poly->found; i++)
            ast_ChildAppend(result, power_or_self(poly_linear(poly->var, poly->fp[i], poly->fq[i]), poly->mult[i]));
        if(poly->deg >= 1)
            ast_ChildAppend(result, poly_to_ast(poly));
    }

    poly_free(poly);
    return result;
}

/*factors e as one polynomial if it is one (products expanded first). if not,
the polynomials in its top-level parts: the top and bottom of a fraction, the
factors of a product, the base of a power. not inside functions: sin(X^2 _ 1)
stays as it is.*/
static bool factor_polynomial(pcas_ast_t *e) {
    pcas_ast_t *copy, *result, *child;
    bool changed = false;

    if(pcas_cancelled) /*Fluxion: stop here, the answer's being thrown away*/
        return false;

    if(e->type != NODE_OPERATOR)
        return false;

    copy = ast_Copy(e);
    expand(copy, EXP_ALL);
    simplify(copy, SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_RATIONAL | SIMP_EVAL | SIMP_LIKE_TERMS);
    result = poly_factor(copy);
    ast_Cleanup(copy);

    if(result != NULL) {
        simplify(result, SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_RATIONAL | SIMP_EVAL);
        replace_node(e, result);
        return true;
    }

    if(isoptype(e, OP_DIV) || isoptype(e, OP_MULT)) {
        for(child = ast_ChildGet(e, 0); child != NULL; child = child->next)
            changed |= factor_polynomial(child);
    } else if(isoptype(e, OP_POW)) {
        changed = factor_polynomial(ast_ChildGet(e, 0));
    }

    /*a factored part is a product inside this one: flatten it*/
    if(changed)
        simplify(e, SIMP_COMMUTATIVE);
    return changed;
}

/*Fluxion: X^n as (X, n) for a whole number n, anything else as (it, 1)*/
static pcas_ast_t *power_base(pcas_ast_t *e, mp_small *n) {
    mp_small den;

    if(isoptype(e, OP_POW) && ast_ChildGet(e, 1)->type == NODE_NUMBER
       && mp_rat_to_ints(ast_ChildGet(e, 1)->op.num, n, &den) == MP_OK && den == 1 && *n > 0)
        return ast_ChildGet(e, 0);
    *n = 1;
    return e;
}

/*Fluxion: f becomes base^n (1 if n is 0)*/
static void set_power(pcas_ast_t *f, pcas_ast_t *base, mp_small n) {
    pcas_ast_t *b = ast_Copy(base);

    replace_node(f, n == 0 ? ast_MakeNumber(num_FromInt(1)) : power_or_self(b, (int)n));
    if(n == 0)
        ast_Cleanup(b);
}

static pcas_ast_t *as_product(pcas_ast_t *e) {
    pcas_ast_t *m;

    if(isoptype(e, OP_MULT))
        return e;
    m = ast_MakeOperator(OP_MULT);
    ast_ChildAppend(m, e);
    return m;
}

/*Fluxion: one fraction: factor the top and bottom, take out what they share. finally works*/
static bool cancel_fraction(pcas_ast_t *e) {
    pcas_ast_t *num = ast_Copy(ast_ChildGet(e, 0)), *den = ast_Copy(ast_ChildGet(e, 1));
    pcas_ast_t *df, *nf;
    bool changed = false;

    /*same shape as factor_polynomial()'s factors, so they compare: X_3 parses as X+-1*3*/
    simplify(num, SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_RATIONAL | SIMP_EVAL);
    simplify(den, SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_RATIONAL | SIMP_EVAL);
    factor_polynomial(num);
    factor_polynomial(den);
    num = as_product(num);
    den = as_product(den);

    for(df = ast_ChildGet(den, 0); df != NULL; df = df->next) {
        mp_small nd, nn;
        pcas_ast_t *bd = power_base(df, &nd);

        if(bd->type == NODE_NUMBER || isoptype(bd, OP_DIV))
            continue;

        for(nf = ast_ChildGet(num, 0); nf != NULL; nf = nf->next) {
            pcas_ast_t *bn = power_base(nf, &nn);

            if(ast_Compare(bn, bd)) {
                mp_small k = nn < nd ? nn : nd;

                /*bd goes away with df, so copy it first*/
                pcas_ast_t *base = ast_Copy(bd);

                set_power(nf, base, nn - k);
                set_power(df, base, nd - k);
                ast_Cleanup(base);
                changed = true;
                break;
            }
        }
    }

    if(changed) {
        replace_node(ast_ChildGet(e, 0), num);
        replace_node(ast_ChildGet(e, 1), den);
        simplify(e, SIMP_COMMUTATIVE | SIMP_EVAL);
    } else {
        ast_Cleanup(num);
        ast_Cleanup(den);
    }
    return changed;
}

bool cancel_fractions(pcas_ast_t *e) {
    pcas_ast_t *child;
    bool changed = false;

    if(pcas_cancelled) /*Fluxion: stop here, the answer's being thrown away*/
        return false;

    if(e->type != NODE_OPERATOR)
        return false;

    for(child = ast_ChildGet(e, 0); child != NULL; child = child->next)
        changed |= cancel_fractions(child);

    if(isoptype(e, OP_DIV))
        changed |= cancel_fraction(e);
    return changed;
}

/*Fluxion: the factor in prod with f's base (the number, if f is one), or NULL*/
/*number? i hardly know 'er bwaahaweha*/
/*he..he...*/
static pcas_ast_t *find_factor(pcas_ast_t *prod, pcas_ast_t *f) {
    mp_small n;
    pcas_ast_t *g, *b = power_base(f, &n);

    for(g = ast_ChildGet(prod, 0); g != NULL; g = g->next) {
        if(b->type == NODE_NUMBER ? g->type == NODE_NUMBER : ast_Compare(power_base(g, &n), b))
            return g;
    }
    return NULL;
}

/*Fluxion: a term's denominator as a product of factors, NULL if it has none*/
static pcas_ast_t *term_den(pcas_ast_t *t) {
    pcas_ast_t *den;

    if(!isoptype(t, OP_DIV))
        return NULL;
    den = ast_Copy(ast_ChildGet(t, 1));
    simplify(den, SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_RATIONAL | SIMP_EVAL);
    factor_polynomial(den);
    return as_product(den);
}

/*Fluxion: lcm takes in den: each base at the larger power, the numbers' lcm*/
/*UwU*/
static void lcm_merge(pcas_ast_t *lcm, pcas_ast_t *den) {
    pcas_ast_t *f;

    for(f = ast_ChildGet(den, 0); f != NULL; f = f->next) {
        pcas_ast_t *g = find_factor(lcm, f);
        mp_small nf, ng;

        if(g == NULL) {
            ast_ChildAppend(lcm, ast_Copy(f));
        } else if(f->type == NODE_NUMBER) {
            mp_int_lcm(mp_rat_numer_ref(g->op.num), mp_rat_numer_ref(f->op.num), mp_rat_numer_ref(g->op.num));
        } else {
            pcas_ast_t *base = ast_Copy(power_base(f, &nf));

            power_base(g, &ng);
            if(nf > ng)
                set_power(g, base, nf);
            ast_Cleanup(base);
        }
    }
}

/*Fluxion: mult (a copy of the lcm) becomes lcm / den. every factor of den is in it*/
static void lcm_divide(pcas_ast_t *mult, pcas_ast_t *den) {
    pcas_ast_t *f;

    for(f = ast_ChildGet(den, 0); f != NULL; f = f->next) {
        pcas_ast_t *g = find_factor(mult, f);
        mp_small nf, ng;

        if(f->type == NODE_NUMBER) {
            mp_int_div(mp_rat_numer_ref(g->op.num), mp_rat_numer_ref(f->op.num), mp_rat_numer_ref(g->op.num), NULL);
        } else {
            pcas_ast_t *base = ast_Copy(power_base(f, &nf));

            power_base(g, &ng);
            set_power(g, base, ng - nf);
            ast_Cleanup(base);
        }
    }
}

/*Fluxion: one sum with fractions in it becomes one fraction*/
static void comdenom_sum(pcas_ast_t *e, bool factored) {
    pcas_ast_t *t, *den, *num, *lcm = ast_MakeOperator(OP_MULT), *frac;

    for(t = ast_ChildGet(e, 0); t != NULL; t = t->next) {
        if((den = term_den(t)) != NULL) {
            lcm_merge(lcm, den);
            ast_Cleanup(den);
        }
    }

    /*each term's top times what its bottom is missing from the lcm*/
    num = ast_MakeOperator(OP_ADD);
    for(t = ast_ChildGet(e, 0); t != NULL; t = t->next) {
        pcas_ast_t *mult = ast_Copy(lcm);

        if((den = term_den(t)) != NULL) {
            lcm_divide(mult, den);
            ast_Cleanup(den);
            ast_ChildAppend(mult, ast_Copy(ast_ChildGet(t, 0)));
        } else {
            ast_ChildAppend(mult, ast_Copy(t));
        }
        ast_ChildAppend(num, mult);
    }

    expand(num, EXP_ALL);
    simplify(num, SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_RATIONAL | SIMP_EVAL | SIMP_LIKE_TERMS);
    simplify(lcm, SIMP_COMMUTATIVE | SIMP_EVAL);

    frac = ast_MakeBinary(OP_DIV, num, lcm);
    cancel_fraction(frac);
    num = ast_ChildGet(frac, 0);
    den = ast_ChildGet(frac, 1);

    /*like the nspire: comDenom expands the top and bottom, factor factors them*/
    if(factored) {
        factor(num, FAC_ALL);
        simplify(num, SIMP_COMMUTATIVE | SIMP_EVAL);
    } else {
        expand(num, EXP_ALL);
        simplify(num, SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_EVAL | SIMP_LIKE_TERMS);
        expand(den, EXP_ALL);
        simplify(den, SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_EVAL | SIMP_LIKE_TERMS);
    }
    replace_node(e, frac);
}

bool comdenom(pcas_ast_t *e, bool factored) {
    pcas_ast_t *child;
    bool changed = false;

    if(pcas_cancelled) /*Fluxion: stop here, the answer's being thrown away*/
        return false;

    if(e->type != NODE_OPERATOR)
        return false;

    /*inner sums first, so a sum sees its fractions as fractions*/
    for(child = ast_ChildGet(e, 0); child != NULL; child = child->next)
        changed |= comdenom(child, factored);

    if(isoptype(e, OP_ADD)) {
        for(child = ast_ChildGet(e, 0); child != NULL; child = child->next) {
            if(isoptype(child, OP_DIV)) {
                comdenom_sum(e, factored);
                changed = true;
                break;
            }
        }
    }

    /*X(Y+1/Y) is now X((Y^2+1)/Y): one fraction*/
    if(changed)
        simplify(e, SIMP_COMMUTATIVE | SIMP_RATIONAL);
    return changed;
}

bool factor(pcas_ast_t *e, unsigned char flags) {
    bool changed = false;

    if(pcas_cancelled)
        return false;

    /*Fluxion: polynomials first. pairing up terms by their gcd would split
    X^3+X^2_8X_12 into a worse sum, so it only gets what isn't a polynomial.*/
    if((flags & FAC_POLYNOMIAL) && factor_polynomial(e))
        return true;

    if(flags & (FAC_SIMPLE_ADDITION_EVALUATEABLE | FAC_SIMPLE_ADDITION_NONEVALUATEABLE)) {
        changed |= factor_addition(e, flags);

        /*Fluxion: and the polynomials that pulled out: X^2sin(X)_sin(X) -> (X^2_1)sin(X) -> (X_1)(X+1)sin(X)*/
        if(changed && (flags & FAC_POLYNOMIAL))
            factor_polynomial(e);
    }
    return changed;
}