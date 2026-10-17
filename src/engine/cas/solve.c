/*
    Fluxion: solve(), not in PineappleCAS.

    solves e = 0 for one variable over the reals, like the nspire's solve(). solve_ex
    with complex on is more its csolve(): quadratics give their complex roots too.
      var gone after simplifying   every value (0 = 0) or none (1 = 0)
      linear in var                var = -b/a. a and b can have other variables in them
      polynomial with numbers      factored (factor.c), then each factor: linear, a
                                   quadratic by the formula, or a cubic by cardano / the
                                   cos form. a quartic or higher factor left over means
                                   it can't be solved (yet), never a partial answer.
                                   one exception, a cardano cubic only gives its real
                                   root even with complex on, the pair is too slow to
                                   draw on the calculator for now.

    Your mom
*/

#include "cas.h"

#define SIMP_SOLVE (SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_RATIONAL | SIMP_EVAL | SIMP_LIKE_TERMS)
/*no identity passes in here, solve doesnt need them and theyre the slow ones*/

typedef struct {
    int deg;
    pcas_ast_t *c[3];   /*coefficients of var^0, var^1, var^2; NULL = 0*/
    pcas_ast_t *lead;   /*coefficient of var^deg, whatever deg is (asym wants it)*/
    bool numeric;       /*every coefficient (all degrees) is a number*/
} coefs_t;

bool has_symbol(const pcas_ast_t *e, Symbol s) {
    const pcas_ast_t *child;

    if(e->type == NODE_SYMBOL)
        return e->op.symbol == s;
    if(e->type == NODE_OPERATOR)
        for(child = e->op.operator.base; child != NULL; child = child->next)
            if(has_symbol(child, s))
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

    if(!has_symbol(e, var)) {
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

    if(isoptype(e, OP_DIV) && !has_symbol(ast_ChildGet(e, 1), var)) {
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

/*a/b with both numbers becomes the one rational. normalize splits every
  rational into a/b, so right after a simplify is where this is needed*/
static void fold_fraction(pcas_ast_t *e) {
    pcas_ast_t *a, *b, *r;

    if(!isoptype(e, OP_DIV))
        return;
    a = ast_ChildGet(e, 0);
    b = ast_ChildGet(e, 1);
    if(a->type != NODE_NUMBER || b->type != NODE_NUMBER || is_zero(b))
        return;
    r = number(0);
    if(r == NULL || mp_rat_div(a->op.num, b->op.num, r->op.num) != MP_OK) {
        ast_Cleanup(r);
        return;
    }
    replace_node(e, r);
}

static void coefs_free(coefs_t *p) {
    int i;

    for(i = 0; i < 3; i++) {
        if(p->c[i] != NULL)
            ast_Cleanup(p->c[i]);
        p->c[i] = NULL;
    }
    if(p->lead != NULL)
        ast_Cleanup(p->lead);
    p->lead = NULL;
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
    fold_fraction(coef); /*1/2 is a number, not a tree, or x^2/2 isn't "numeric"*/

    if(k > p->deg) {
        p->deg = k;
        ast_Cleanup(p->lead);
        p->lead = ast_MakeOperator(OP_ADD);
    }
    if(k == p->deg)
        ast_ChildAppend(p->lead, ast_Copy(coef));
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
    p->lead = ast_MakeOperator(OP_ADD);
    for(i = 0; i < 3; i++)
        p->c[i] = NULL;

    if(e == NULL || pcas_cancelled || p->lead == NULL) { /*oom, or already unwinding*/
        coefs_free(p);
        return false;
    }

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
    simplify(p->lead, SIMP_SOLVE);
    if(pcas_cancelled) { /*coefs half simplified, nobody downstream can read those*/
        coefs_free(p);
        return false;
    }
    return true;
}

int poly_degree(pcas_ast_t *expr, Symbol var, pcas_ast_t **lead) {
    pcas_ast_t *e = ast_Copy(expr);
    coefs_t p;
    int deg = -1;

    if(e == NULL)
        return -1;
    simplify(e, SIMP_SOLVE);
    expand(e, EXP_ALL);
    simplify(e, SIMP_SOLVE);
    if(!pcas_cancelled && read_poly(e, var, &p)) {
        deg = p.deg;
        if(lead != NULL) {
            *lead = p.lead;
            p.lead = NULL;
        }
        coefs_free(&p);
    }
    ast_Cleanup(e);
    return deg;
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
/*q < 0 goes in as i*sqrt(-q), so it comes out with an i and not as sqrt(-3)*/
/*>w<*/
static pcas_ast_t *quadratic_root(mp_rat mid, mp_rat q, int sign) {
    pcas_ast_t *r = ast_MakeOperator(OP_ADD), *s, *qn;

    ast_ChildAppend(r, ast_MakeNumber(num_Copy(mid)));
    if(sign != 0) {
        qn = ast_MakeNumber(num_Copy(q));
        s = ast_MakeBinary(OP_POW, qn, ast_MakeNumber(num_FromFraction(1, 2)));
        if(mp_rat_compare_zero(q) < 0) {
            mp_rat_neg(qn->op.num, qn->op.num);
            s = ast_MakeBinary(OP_MULT, ast_MakeSymbol(SYM_IMAG), s);
        }
        ast_ChildAppend(r, ast_MakeBinary(OP_MULT, number(sign), s));
    }
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

/*roots of a*X^2 + b*X + c, all numbers, added at out[n]. the complex ones only if
complex. returns the new count.*/
static int solve_quadratic(coefs_t *p, pcas_ast_t **out, int n, bool complex) {
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
    } else if(mp_rat_compare_zero(q) > 0 || complex) {
        /*smallest first. complex ones have no decimal so sort_roots leaves them, -i first*/
        n = add_root(out, n, quadratic_root(mid, q, -1));
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

/*the coef as a tree, 0 when its missing*/
static pcas_ast_t *coef(coefs_t *p, int k) {
    return p->c[k] != NULL ? ast_Copy(p->c[k]) : number(0);
}

/*sqrt(d) with any square number factor pulled out front, sqrt(4(y_3)) as
2sqrt(y_3). left to simplify the 4 just gets distributed back in and it stays a
lump. only the number child of a product, a plain number sqrt works already*/
static pcas_ast_t *root_of(pcas_ast_t *d, long *k) {
    pcas_ast_t *num = NULL;
    mp_small n, den;
    long s;
    unsigned i;

    *k = 1;
    if(isoptype(d, OP_MULT))
        for(i = 0; i < ast_ChildLength(d) && num == NULL; i++)
            if(ast_ChildGet(d, i)->type == NODE_NUMBER)
                num = ast_ChildGet(d, i);
    if(num == NULL || mp_rat_to_ints(num->op.num, &n, &den) != MP_OK || den != 1 || n < 4)
        return ast_MakeBinary(OP_POW, d, ast_MakeNumber(num_FromFraction(1, 2)));

    for(s = 1; (s + 1) * (s + 1) <= n; s++)
        ;
    for(*k = s; *k > 1 && n % (*k * *k) != 0; (*k)--)
        ;
    if(*k > 1)
        mp_rat_set_value(num->op.num, n / (*k * *k), 1);
    return ast_MakeBinary(OP_POW, d, ast_MakeNumber(num_FromFraction(1, 2)));
}

static long gcd_small(long a, long b) {
    if(a < 0) a = -a;
    if(b < 0) b = -b;
    while(b != 0) {
        long t = a % b;
        a = b;
        b = t;
    }
    return a;
}

/*the coef as a plain number if it is one (or missing, 0). false if it's letters*/
static bool coef_small(coefs_t *p, int i, long *v) {
    mp_small n, den;

    if(p->c[i] == NULL) {
        *v = 0;
        return true;
    }
    if(p->c[i]->type != NODE_NUMBER || mp_rat_to_ints(p->c[i]->op.num, &n, &den) != MP_OK || den != 1)
        return false;
    *v = n;
    return true;
}

/*a x^2 + b x + c with a b c being expressions in other letters, y = x^2 + 3 solved
for x. the plain formula built as a tree, (-b +- sqrt(b^2 - 4ac)) / 2a, and
simplify sorts it out. the discriminant gets factored first so sqrt(4y_12) comes
out as 2sqrt(y_3) and not a lump*/
static int solve_quadratic_sym(coefs_t *p, pcas_ast_t **out, int n) {
    int sign, k;
    long a, b, g = 1;
    pcas_ast_t *disc;
    bool double_root;

    /*a negative in front only ever gives /-2 or /(-pi), so flip the lot*/
    if(p->c[2] != NULL && is_negative_for_sure(p->c[2]))
        for(k = 0; k < 3; k++)
            if(p->c[k] != NULL) {
                p->c[k] = ast_MakeBinary(OP_MULT, number(-1), p->c[k]);
                simplify(p->c[k], SIMP_SOLVE);
            }

    disc = ast_MakeBinary(OP_ADD,
        ast_MakeBinary(OP_POW, coef(p, 1), number(2)),
        ast_MakeBinary(OP_MULT, number(-4), ast_MakeBinary(OP_MULT, coef(p, 2), coef(p, 0))));
    simplify(disc, SIMP_SOLVE);
    factor(disc, FAC_ALL);
    double_root = is_ast_int(disc, 0);  /*(x_sqrt2)^2, say it once*/

    for(sign = double_root ? 1 : -1; sign <= 1 && !pcas_cancelled; sign += 2) {
        pcas_ast_t *r, *root;
        long rk;

        root = root_of(ast_Copy(disc), &rk);

        /*(-4 +- 2sqrt(..))/4 with a and b numbers, simplify won't cancel the 2
        on its own, so take the gcd out here. c can be whatever*/
        if(coef_small(p, 2, &a) && coef_small(p, 1, &b))
            g = gcd_small(gcd_small(b, rk), 2 * a);
        if(g < 1)
            g = 1;
        r = ast_MakeBinary(OP_DIV,
            ast_MakeBinary(OP_ADD,
                ast_MakeBinary(OP_MULT, number(-1), g > 1 ? number((int)(b / g)) : coef(p, 1)),
                ast_MakeBinary(OP_MULT, number(sign * (int)(rk / g)), root)),
            g > 1 ? number((int)(2 * a / g)) : ast_MakeBinary(OP_MULT, number(2), coef(p, 2)));
        simplify(r, SIMP_SOLVE);
        n = add_root(out, n, r);
    }
    ast_Cleanup(disc);
    return n;
}

/*a numeric coef as one rational, missing is 0. normalize can leave it as a/b so
that too. false for anything else*/
static bool coef_rat(pcas_ast_t *e, mp_rat r) {
    if(e == NULL) {
        mp_rat_zero(r);
        return true;
    }
    if(e->type == NODE_NUMBER)
        return mp_rat_copy(e->op.num, r) == MP_OK;
    if(isoptype(e, OP_DIV) && ast_ChildGet(e, 0)->type == NODE_NUMBER && ast_ChildGet(e, 1)->type == NODE_NUMBER
       && !is_zero(ast_ChildGet(e, 1)))
        return mp_rat_div(ast_ChildGet(e, 0)->op.num, ast_ChildGet(e, 1)->op.num, r) == MP_OK;
    return false;
}

/*a*b into *out, false if it doesnt fit a long (24 bit int on the calculator, long is 32)*/
static bool mul_small(mp_small a, mp_small b, mp_small *out) {
    mp_small x = a < 0 ? -a : a;

    if(x != 0 && (b > MP_SMALL_MAX / x || b < -(MP_SMALL_MAX / x)))
        return false;
    *out = a * b;
    return true;
}

/*the kth powers in *m taken out, f up to 1000, k 2 or 3. 54 and 3 leaves 2, gives 3.
only small factors get tried, the calculator has to do this too*/
static mp_small take_powers(mp_small *m, int k) {
    mp_small f, fk, front = 1;

    for(f = 2; f <= 1000; f++) {
        fk = k == 2 ? f * f : f * f * f;
        if(fk > *m)
            break;
        while(*m % fk == 0) {
            *m /= fk;
            front *= f;
        }
    }
    return front;
}

/*splits r^(1/k) into front * m^(1/k) / d with all three whole, r > 0. false on overflow*/
static bool root_parts(mp_rat r, int k, mp_small *front, mp_small *m, mp_small *d) {
    mp_small n;
    int i;

    if(mp_rat_to_ints(r, &n, d) != MP_OK || n <= 0)
        return false;
    for(*m = n, i = 1; i < k; i++)
        if(!mul_small(*m, *d, m))
            return false;
    *front = take_powers(m, k);
    return true;
}

/*r^(1/k), r >= 0, k 2 or 3, as the root of n*d^(k-1) over d so the bottom is
whole, with the kth powers taken out front: sqrt(31/108) is sqrt(93)/18, cbrt(16)
is 2cbrt(2). simplify leaves those alone. a plain number if it comes out even*/
static pcas_ast_t *root_rat(mp_rat r, int k) {
    mp_small front, m, d;

    if(mp_rat_compare_zero(r) == 0)
        return number(0);
    if(!root_parts(r, k, &front, &m, &d)) /*too big to bother*/
        return ast_MakeBinary(OP_POW, ast_MakeNumber(num_Copy(r)), ast_MakeNumber(num_FromFraction(1, k)));
    if(m == 1)
        return ast_MakeNumber(num_FromFraction(front, d));
    return ast_MakeBinary(OP_DIV,
        ast_MakeBinary(OP_MULT, number(front), ast_MakeBinary(OP_POW, number(m), ast_MakeNumber(num_FromFraction(1, k)))),
        number(d));
}

/*cbrt((a + b sqrt(m)) / d) as c cbrt(a' + b' sqrt(m)) / d, whole numbers inside.
the full simplify hangs on cbrt(sqrt(93)/18 - 1/2) - cbrt(sqrt(93)/18 + 1/2), the
canonical form (the app runs it to show the answer) goes round and round in gcd().
with the inside whole it doesnt, at least on the ones i tried. NULL if a number
gets too big for a long*/
static pcas_ast_t *cbrt_surd(mp_small a, mp_small b, mp_small m, mp_small d) {
    mp_small dd, g, c;

    if(!mul_small(d, d, &dd) || !mul_small(a, dd, &a) || !mul_small(b, dd, &b))
        return NULL;
    g = gcd_small(a, b);
    c = take_powers(&g, 3);
    a /= c * c * c;
    b /= c * c * c;
    return ast_MakeBinary(OP_DIV,
        ast_MakeBinary(OP_MULT, number(c),
            ast_MakeBinary(OP_ROOT, number(3),
                ast_MakeBinary(OP_ADD, number(a),
                    ast_MakeBinary(OP_MULT, number(b), ast_MakeBinary(OP_POW, number(m), ast_MakeNumber(num_FromFraction(1, 2))))))),
        number(d));
}

/*cbrt(mid + sg*sqrt(disc)) as sign*cbrt(|...|), NULL when it's 0. approx gives up
on x^(1/3) of a negative, and then sort_roots can't place it. the sign comes from
mid^2 against disc, no decimals*/
static pcas_ast_t *cardano_term(mp_rat mid, mp_rat disc, int sg) {
    mp_rat t = num_FromInt(0);
    pcas_ast_t *s, *in = NULL, *r;
    mp_small f, m, e, mn, md, d, a, b;
    int sign, cmp;

    if(t == NULL)
        return NULL;
    mp_rat_mul(mid, mid, t);
    cmp = mp_rat_compare(t, disc);
    if(mp_rat_compare_zero(mid) == 0 || mp_rat_compare_zero(mid) == sg || cmp < 0)
        sign = sg;
    else
        sign = cmp == 0 ? 0 : mp_rat_compare_zero(mid);
    if(sign == 0) {
        num_Cleanup(t);
        return NULL;
    }

    s = root_rat(disc, 2);
    if(s == NULL) {
        num_Cleanup(t);
        return NULL;
    }
    if(s->type == NODE_NUMBER) {
        /*disc was a square, so its all one number and the cube root can come out too*/
        if(sg > 0)
            mp_rat_add(mid, s->op.num, t);
        else
            mp_rat_sub(mid, s->op.num, t);
        if(sign < 0)
            mp_rat_neg(t, t);
        ast_Cleanup(s);
        in = root_rat(t, 3);
    } else {
        /*sign*mid + sign*sg*f sqrt(m)/e over one whole bottom d*/
        if(root_parts(disc, 2, &f, &m, &e) && mp_rat_to_ints(mid, &mn, &md) == MP_OK
           && mul_small(md / gcd_small(md, e), e, &d)
           && mul_small(sign * mn, d / md, &a) && mul_small(sign * sg * f, d / e, &b))
            in = cbrt_surd(a, b, m, d);
        if(in != NULL) {
            ast_Cleanup(s);
        } else {
            /*too big for longs, the fraction stays inside then*/
            mp_rat_copy(mid, t);
            if(sign < 0)
                mp_rat_neg(t, t);
            in = ast_MakeBinary(OP_ROOT, number(3),
                ast_MakeBinary(OP_ADD, ast_MakeNumber(num_Copy(t)), ast_MakeBinary(OP_MULT, number(sign * sg), s)));
        }
    }
    num_Cleanup(t);
    r = ast_MakeBinary(OP_MULT, number(sign), in);
    return r;
}

/*(-1 + sg*i*sqrt(3))/2, a cube root of 1*/
static pcas_ast_t *unity(int sg) {
    return ast_MakeBinary(OP_DIV,
        ast_MakeBinary(OP_ADD, number(-1),
            ast_MakeBinary(OP_MULT, number(sg),
                ast_MakeBinary(OP_MULT, ast_MakeSymbol(SYM_IMAG),
                    ast_MakeBinary(OP_POW, number(3), ast_MakeNumber(num_FromFraction(1, 2)))))),
        number(2));
}

/*wu*U + wv*V + shift, either cbrt term can be missing (NULL)*/
static pcas_ast_t *cardano_root(pcas_ast_t *u, pcas_ast_t *v, int wu, int wv, mp_rat shift) {
    pcas_ast_t *r = ast_MakeOperator(OP_ADD);

    if(r == NULL)
        return NULL;
    if(u != NULL)
        ast_ChildAppend(r, wu == 0 ? ast_Copy(u) : ast_MakeBinary(OP_MULT, unity(wu), ast_Copy(u)));
    if(v != NULL)
        ast_ChildAppend(r, wv == 0 ? ast_Copy(v) : ast_MakeBinary(OP_MULT, unity(wv), ast_Copy(v)));
    ast_ChildAppend(r, ast_MakeNumber(num_Copy(shift)));
    simplify(r, SIMP_SOLVE);
    return r;
}

/*true if e is some rational c times pi (pi, n pi, pi/m or n pi/m), which is what a
folded acos comes back as*/
static bool pi_part(pcas_ast_t *e, mp_rat c) {
    pcas_ast_t *top = e, *x, *y;

    if(isoptype(e, OP_DIV)) {
        if(ast_ChildGet(e, 1)->type != NODE_NUMBER || is_zero(ast_ChildGet(e, 1)))
            return false;
        top = ast_ChildGet(e, 0);
    }
    if(top->type == NODE_SYMBOL && top->op.symbol == SYM_PI) {
        mp_rat_set_value(c, 1, 1);
    } else if(isoptype(top, OP_MULT) && ast_ChildLength(top) == 2) {
        x = ast_ChildGet(top, 0);
        y = ast_ChildGet(top, 1);
        if(x->type != NODE_NUMBER) {
            x = y;
            y = ast_ChildGet(top, 0);
        }
        if(x->type != NODE_NUMBER || y->type != NODE_SYMBOL || y->op.symbol != SYM_PI)
            return false;
        mp_rat_copy(x->op.num, c);
    } else {
        return false;
    }
    if(top != e)
        mp_rat_div(c, ast_ChildGet(e, 1)->op.num, c);
    return true;
}

/*a*X^3 + b*X^2 + c*X + d, all numbers, added at out[n]. X = t - b/3a gives
t^3 + pt + q, and the sign of disc = (q/2)^2 + (p/3)^3 says which way:
below 0 three real roots, the cos one (cardano would want cube roots of complex
numbers for those). above 0 one real root by cardano, and the two complex ones
if complex. 0 is a repeated root, factor() takes those out before here so that's
just in case. returns the new count*/
static int solve_cubic(coefs_t *p, pcas_ast_t **out, int n, bool complex) {
    mp_rat a, b, c, d, pp, q, disc, mid, shift, t, k;
    int i;

    if(pcas_cancelled)
        return SOLVE_UNKNOWN;
    a = num_FromInt(0);
    b = num_FromInt(0);
    c = num_FromInt(0);
    d = num_FromInt(0);
    pp = num_FromInt(0);
    q = num_FromInt(0);
    disc = num_FromInt(0);
    mid = num_FromInt(0);
    shift = num_FromInt(0);
    t = num_FromInt(0);
    k = num_FromInt(0);
    if(a == NULL || b == NULL || c == NULL || d == NULL || pp == NULL || q == NULL || disc == NULL
       || mid == NULL || shift == NULL || t == NULL || k == NULL
       || !coef_rat(p->lead, a) || !coef_rat(p->c[2], b) || !coef_rat(p->c[1], c) || !coef_rat(p->c[0], d)
       || mp_rat_compare_zero(a) == 0) {
        n = SOLVE_UNKNOWN;
        goto done;
    }

    /*monic first, then p = c - b^2/3, q = 2b^3/27 - bc/3 + d*/
    mp_rat_div(b, a, b);
    mp_rat_div(c, a, c);
    mp_rat_div(d, a, d);
    mp_rat_mul(b, b, t);
    mp_rat_set_value(k, 1, 3);
    mp_rat_mul(t, k, t);
    mp_rat_sub(c, t, pp);
    mp_rat_expt(b, 3, q);
    mp_rat_set_value(k, 2, 27);
    mp_rat_mul(q, k, q);
    mp_rat_mul(b, c, t);
    mp_rat_set_value(k, 1, 3);
    mp_rat_mul(t, k, t);
    mp_rat_sub(q, t, q);
    mp_rat_add(q, d, q);

    /*shift = -b/3, mid = -q/2, disc = (q/2)^2 + (p/3)^3*/
    mp_rat_set_value(k, -1, 3);
    mp_rat_mul(b, k, shift);
    mp_rat_set_value(k, -1, 2);
    mp_rat_mul(q, k, mid);
    mp_rat_mul(mid, mid, disc);
    mp_rat_set_value(k, 1, 3);
    mp_rat_mul(pp, k, t);
    mp_rat_expt(t, 3, t);
    mp_rat_add(disc, t, disc);

    if(mp_rat_compare_zero(disc) < 0) {
        /*2 sqrt(-p/3) cos(acos((3q/2p) sqrt(-3/p))/3 - 2pi k/3), k = 0 1 2. p < 0 here*/
        pcas_ast_t *amp, *arg, *theta;
        mp_rat cpi;
        bool nice, folded;

        mp_rat_set_value(k, -1, 3);
        mp_rat_mul(pp, k, t);
        amp = ast_MakeBinary(OP_MULT, number(2), root_rat(t, 2));
        mp_rat_set_value(k, 3, 2);
        mp_rat_div(q, pp, t);
        mp_rat_mul(t, k, t);
        mp_rat_set_value(k, -3, 1);
        mp_rat_div(k, pp, k);
        arg = ast_MakeBinary(OP_MULT, ast_MakeNumber(num_Copy(t)), root_rat(k, 2));
        simplify(arg, SIMP_SOLVE);
        theta = ast_MakeUnary(OP_COS_INV, arg);

        /*the identity passes, but only on the acos and only when it can fold: acos(-1/2)
        is 2pi/3. those all have arg^2 = 1/4, 1/2 or 3/4, and arg^2 is just t^2*k. for
        anything else it was a ms on the PC for nothing*/
        cpi = num_FromInt(0);
        nice = false;
        if(cpi != NULL) {
            mp_rat_mul(t, t, cpi);
            mp_rat_mul(cpi, k, cpi);
            nice = mp_rat_compare_value(cpi, 1, 4) == 0 || mp_rat_compare_value(cpi, 1, 2) == 0
                || mp_rat_compare_value(cpi, 3, 4) == 0;
        }
        simplify(theta, nice ? SIMP_ALL : SIMP_SOLVE);
        folded = nice && theta != NULL && pi_part(theta, cpi);

        for(i = 0; i < 3 && !pcas_cancelled; i++) {
            pcas_ast_t *angle, *r;

            if(folded) {
                /*(c pi - 2pi k)/3 done here, the light simplify leaves 2pi/9 - 4pi/3
                alone. then into 0..pi, cos(-4pi/9) is cos(4pi/9)*/
                mp_rat_set_value(k, -2 * i, 1);
                mp_rat_add(cpi, k, t);
                mp_rat_set_value(k, 1, 3);
                mp_rat_mul(t, k, t);
                mp_rat_set_value(k, 2, 1);
                while(mp_rat_compare_zero(t) < 0)
                    mp_rat_add(t, k, t);
                while(mp_rat_compare(t, k) >= 0)
                    mp_rat_sub(t, k, t);
                if(mp_rat_compare_value(t, 1, 1) > 0)
                    mp_rat_sub(k, t, t);
                angle = ast_MakeBinary(OP_MULT, ast_MakeNumber(num_Copy(t)), ast_MakeSymbol(SYM_PI));
            } else {
                angle = ast_MakeBinary(OP_DIV, ast_Copy(theta), number(3));
                if(i > 0)
                    angle = ast_MakeBinary(OP_ADD, angle,
                        ast_MakeBinary(OP_DIV, ast_MakeBinary(OP_MULT, number(-2 * i), ast_MakeSymbol(SYM_PI)), number(3)));
            }
            r = ast_MakeBinary(OP_ADD,
                ast_MakeBinary(OP_MULT, ast_Copy(amp), ast_MakeUnary(OP_COS, angle)),
                ast_MakeNumber(num_Copy(shift)));
            simplify(r, SIMP_SOLVE);
            n = add_root(out, n, r);
        }
        ast_Cleanup(amp);
        ast_Cleanup(theta);
        num_Cleanup(cpi);
    } else if(mp_rat_compare_zero(disc) > 0) {
        /*cbrt(-q/2 + sqrt(disc)) + cbrt(-q/2 - sqrt(disc)). the complex pair is
        the two cbrts times the other cube roots of 1, the -i one first*/
        pcas_ast_t *u = cardano_term(mid, disc, 1), *v = cardano_term(mid, disc, -1);

        n = add_root(out, n, cardano_root(u, v, 0, 0, shift));
        if(complex) {
            n = add_root(out, n, cardano_root(u, v, -1, 1, shift));
            n = add_root(out, n, cardano_root(u, v, 1, -1, shift));
        }
        ast_Cleanup(u);
        ast_Cleanup(v);
    } else if(mp_rat_compare_zero(pp) == 0) {
        n = add_root(out, n, quadratic_root(shift, shift, 0)); /*p and q both 0, one triple root*/
    } else {
        /*t = 3q/p once and -3q/2p twice*/
        mp_rat_div(q, pp, t);
        mp_rat_set_value(k, 3, 1);
        mp_rat_mul(t, k, t);
        mp_rat_add(t, shift, k);
        n = add_root(out, n, quadratic_root(k, k, 0));
        mp_rat_div(q, pp, t);
        mp_rat_set_value(k, -3, 2);
        mp_rat_mul(t, k, t);
        mp_rat_add(t, shift, k);
        n = add_root(out, n, quadratic_root(k, k, 0));
    }

done:
    num_Cleanup(a);
    num_Cleanup(b);
    num_Cleanup(c);
    num_Cleanup(d);
    num_Cleanup(pp);
    num_Cleanup(q);
    num_Cleanup(disc);
    num_Cleanup(mid);
    num_Cleanup(shift);
    num_Cleanup(t);
    num_Cleanup(k);
    return n;
}

/*roots of one factor of the factored polynomial, added at out[n]. returns the
new count, or SOLVE_UNKNOWN.*/
static int solve_factor(pcas_ast_t *f, Symbol var, pcas_ast_t **out, int n, bool complex) {
    pcas_ast_t *child;
    coefs_t p;

    if(pcas_cancelled || f == NULL) /*oom, or already unwinding*/
        return SOLVE_UNKNOWN;

    if(!has_symbol(f, var))
        return n;

    if(isoptype(f, OP_MULT)) {
        for(child = ast_ChildGet(f, 0); child != NULL && n >= 0; child = child->next)
            n = solve_factor(child, var, out, n, complex);
        return n;
    }
    if(isoptype(f, OP_DIV))
        return has_symbol(ast_ChildGet(f, 1), var) ? SOLVE_UNKNOWN : solve_factor(ast_ChildGet(f, 0), var, out, n, complex);
    if(isoptype(f, OP_POW))
        return solve_factor(ast_ChildGet(f, 0), var, out, n, complex);   /*(X+2)^2: X+2 = 0*/

    if(!read_poly(f, var, &p))
        return SOLVE_UNKNOWN;

    if(p.deg == 1)
        n = add_root(out, n, linear_root(&p));
    else if(p.deg == 2 && p.numeric)
        n = solve_quadratic(&p, out, n, complex);
    else if(p.deg == 2)
        n = solve_quadratic_sym(&p, out, n);
    else if(p.deg == 3 && p.numeric)
        n = solve_cubic(&p, out, n, false); /*the complex pair takes minutes to draw on the calculator, real only for now*/
    else
        n = SOLVE_UNKNOWN;      /*quartics someday, ferrari looks even worse*/

    coefs_free(&p);
    return n;
}

/*not a polynomial in the variable, but it only turns up once: y = 2^x, sqrt(x) = 2,
1/x = 2. so peel whatever's wrapped round it, one layer at a time, mirroring each
layer onto the other side. an even power gives both signs. trig gives the first
answer only, the rest of those is the trig solving item*/

static int count_var(pcas_ast_t *e, Symbol var) {
    pcas_ast_t *c;
    int n = 0;

    if(e->type == NODE_SYMBOL)
        return e->op.symbol == var;
    if(e->type == NODE_OPERATOR)
        for(c = ast_ChildGet(e, 0); c != NULL; c = c->next)
            n += count_var(c, var);
    return n;
}

/*the other children of e, copied, as a sum or a product*/
static pcas_ast_t *rest_of(pcas_ast_t *e, pcas_ast_t *skip, OperatorType op) {
    pcas_ast_t *r = ast_MakeOperator(op), *c;

    for(c = ast_ChildGet(e, 0); c != NULL; c = c->next)
        if(c != skip)
            ast_ChildAppend(r, ast_Copy(c));
    return r;
}

static bool even_number(pcas_ast_t *e) {
    mp_small n, den;

    return e->type == NODE_NUMBER && mp_rat_to_ints(e->op.num, &n, &den) == MP_OK && den == 1 && n % 2 == 0;
}

static pcas_ast_t *turns(int k);

/*x^2/2 - x/2 - 15 = 0 times 2, so factor() gets whole numbers to work with (it
  can't read a fraction node as a coefficient). lcm of the number denominators
  of the terms, then expand the lot. the roots don't change, it's = 0*/
static void clear_denominators(pcas_ast_t **e) {
    pcas_ast_t *x = *e, *term, *scaled;
    mp_rat l = num_FromInt(1);

    if(l == NULL)
        return;
    for(term = isoptype(x, OP_ADD) ? ast_ChildGet(x, 0) : x; term != NULL; term = term == x ? NULL : term->next) {
        pcas_ast_t *den = isoptype(term, OP_DIV) ? ast_ChildGet(term, 1) : NULL;

        if(den != NULL && den->type == NODE_NUMBER && mp_rat_is_integer(den->op.num))
            mp_int_lcm(&l->num, &den->op.num->num, &l->num);
    }
    if(mp_rat_compare_value(l, 1, 1) > 0) {
        scaled = ast_MakeBinary(OP_MULT, ast_MakeNumber(num_Copy(l)), *e);
        if(scaled != NULL) {
            *e = scaled;
            expand(*e, EXP_ALL);
            simplify(*e, SIMP_SOLVE);
        }
    }
    num_Cleanup(l);
}

static int isolate(pcas_ast_t *e, Symbol var, pcas_ast_t **out) {
    pcas_ast_t *side = e, *other[SOLVE_MAX];
    int m = 1, i;

    other[0] = number(0);
    while(side->type != NODE_SYMBOL && !pcas_cancelled) {
        pcas_ast_t *a, *b, *in = NULL, *c;
        OperatorType op;
        bool half = false;

        if(side->type != NODE_OPERATOR)
            goto nope;
        op = optype(side);
        a = ast_ChildGet(side, 0);
        b = ast_ChildGet(side, 1);
        for(c = a; c != NULL; c = c->next)
            if(has_symbol(c, var))
                in = c;
        if(in == NULL)
            goto nope;

        for(i = 0; i < m; i++) {
            pcas_ast_t *o = other[i], *t = NULL;

            switch(op) {
            case OP_ADD:  t = ast_MakeBinary(OP_ADD, o, ast_MakeBinary(OP_MULT, number(-1), rest_of(side, in, OP_ADD))); break;
            case OP_MULT: t = ast_MakeBinary(OP_DIV, o, rest_of(side, in, OP_MULT)); break;
            case OP_DIV:  t = in == a ? ast_MakeBinary(OP_MULT, o, ast_Copy(b)) : ast_MakeBinary(OP_DIV, ast_Copy(a), o); break;
            case OP_POW:  /*the root of it, or a log of it*/
                t = in == a ? ast_MakeBinary(OP_POW, o, ast_MakeBinary(OP_DIV, number(1), ast_Copy(b)))
                            : ast_MakeBinary(OP_LOG, ast_Copy(a), o);
                break;
            case OP_ROOT: if(in == b) t = ast_MakeBinary(OP_POW, o, ast_Copy(a)); break;
            case OP_LOG:  if(in == b) t = ast_MakeBinary(OP_POW, ast_Copy(a), o); break;
            /*the whole family, not just the principal one. the second one for
              sin and cos gets added after the loop, unless the two are half a
              turn apart (sin = 0, cos = 0), then it's one family every pi*/
            case OP_SIN: case OP_COS: case OP_TAN: {
                pcas_ast_t *inv = ast_MakeUnary(op == OP_SIN ? OP_SIN_INV : op == OP_COS ? OP_COS_INV : OP_TAN_INV, o);

                simplify(inv, SIMP_ALL);
                if(op == OP_SIN && is_zero(inv))
                    half = true;
                if(op == OP_COS && isoptype(inv, OP_DIV) && is_ast_int(ast_ChildGet(inv, 1), 2)
                   && ast_ChildGet(inv, 0)->type == NODE_SYMBOL && ast_ChildGet(inv, 0)->op.symbol == SYM_PI)
                    half = true;
                t = ast_MakeBinary(OP_ADD, inv, turns(op == OP_TAN || half ? 1 : 2));
                break;
            }
            case OP_SIN_INV: t = ast_MakeUnary(OP_SIN, o); break;
            case OP_COS_INV: t = ast_MakeUnary(OP_COS, o); break;
            case OP_TAN_INV: t = ast_MakeUnary(OP_TAN, o); break;
            default: break;
            }
            if(t == NULL)
                goto nope;
            other[i] = t;
        }
        if(op == OP_POW && in == a && even_number(b)) {
            int k = m;

            for(i = 0; i < k && m < SOLVE_MAX; i++)
                other[m++] = ast_MakeBinary(OP_MULT, number(-1), ast_Copy(other[i]));
        }
        if((op == OP_SIN || op == OP_COS) && !half) {
            /*sin: pi - asin(v), cos: -acos(v), each plus the turns again. the
              asin is the first child of what the switch just built*/
            int k = m;

            for(i = 0; i < k && m < SOLVE_MAX; i++) {
                pcas_ast_t *neg = ast_MakeBinary(OP_MULT, number(-1), ast_Copy(ast_ChildGet(other[i], 0)));

                other[m++] = ast_MakeBinary(OP_ADD, op == OP_SIN ? ast_MakeBinary(OP_ADD, ast_MakeSymbol(SYM_PI), neg) : neg, turns(2));
            }
        }
        side = in;
    }
    if(pcas_cancelled)
        goto nope;

    for(i = 0; i < m; i++) {
        /*the works, so asin(1/2) ends up pi/6. tried the light one for the trig
          families, it leaves pi - pi/6 and (pi/3 + 2pi n)/2 alone, so no*/
        simplify(other[i], SIMP_ALL);
        out[i] = other[i];
    }
    return m;

nope:
    for(i = 0; i < m; i++)
        ast_Cleanup(other[i]);
    return SOLVE_UNKNOWN;
}

/*k pi n, the trig families' integer. n is just the letter n, so an equation
  with its own n in it is going to get confusing (the nspire has n1, n2.. todo)*/
static pcas_ast_t *turns(int k) {
    pcas_ast_t *t = ast_MakeOperator(OP_MULT);

    if(t == NULL)
        return NULL;
    ast_ChildAppend(t, number(k));
    ast_ChildAppend(t, ast_MakeSymbol(SYM_PI));
    ast_ChildAppend(t, ast_MakeSymbol(SYM_N));
    return t;
}

/*e as a decimal, a NUMBER node. approx() gives rationals but normalize splits
  them straight back into a/b trees, so this puts the fraction back together.
  NULL if there's still a letter in it*/
pcas_ast_t *approx_number(pcas_ast_t *e) {
    pcas_ast_t *a = ast_Copy(e), *r = NULL;

    if(a == NULL)
        return NULL;
    approx(a);
    simplify(a, SIMP_SOLVE);
    if(a->type == NODE_NUMBER) {
        r = a;
        a = NULL;
    } else if(isoptype(a, OP_DIV) && ast_ChildGet(a, 0)->type == NODE_NUMBER && ast_ChildGet(a, 1)->type == NODE_NUMBER
              && !is_zero(ast_ChildGet(a, 1))) {
        r = number(0);
        if(r != NULL && mp_rat_div(ast_ChildGet(a, 0)->op.num, ast_ChildGet(a, 1)->op.num, r->op.num) != MP_OK) {
            ast_Cleanup(r);
            r = NULL;
        }
    }
    ast_Cleanup(a);
    return r;
}

/*the family with one integer in for n, simplified the light way. it's a sum of
  a few pi's (or plain numbers in degrees), the identity passes have nothing to
  do there and they're the slow part. NULL when oom*/
static pcas_ast_t *at_n(pcas_ast_t *root, pcas_ast_t *sym, int k) {
    pcas_ast_t *c = ast_Copy(root), *v = number(k);

    if(c == NULL || v == NULL) {
        ast_Cleanup(c);
        ast_Cleanup(v);
        return NULL;
    }
    substitute(c, sym, v);
    ast_Cleanup(v);
    simplify(c, SIMP_SOLVE);
    return c;
}

/*r towards zero, as a long. 0 if imath can't*/
static long whole_part(mp_rat r) {
    mp_int q = mp_int_alloc();
    mp_small v = 0;

    if(q == NULL)
        return 0;
    mp_int_init(q);
    if(mp_int_div(mp_rat_numer_ref(r), mp_rat_denom_ref(r), q, NULL) == MP_OK)
        mp_int_to_int(q, &v);
    mp_int_free(q);
    return (long)v;
}

int solve_in_range(pcas_ast_t **roots, int n, pcas_ast_t *hi) {
    pcas_ast_t *keep[SOLVE_MAX], *val[SOLVE_MAX], *h = approx_number(hi), *sym = ast_MakeSymbol(SYM_N);
    int kept = 0, i, j, k;

    if(h == NULL || sym == NULL || pcas_cancelled) {
        ast_Cleanup(h);
        ast_Cleanup(sym);
        return n;
    }

    for(i = 0; i < n; i++) {
        if(!has_symbol(roots[i], SYM_N)) {
            if(kept < SOLVE_MAX) {
                keep[kept] = roots[i];
                val[kept++] = NULL;
            } else {
                ast_Cleanup(roots[i]);
            }
            continue;
        }
        /*n = 0 and n = 1 give the period, and from that which n's can land in
          range at all (one past each end to be safe). plugging in -4..4 and
          simplifying the lot was 5 ms on the PC, half a minute on the calculator*/
        {
            pcas_ast_t *c0 = at_n(roots[i], sym, 0), *c1 = at_n(roots[i], sym, 1);
            pcas_ast_t *v0 = c0 != NULL ? approx_number(c0) : NULL, *v1 = c1 != NULL ? approx_number(c1) : NULL;
            long lo = -4, up = 4; /*if theres no period to go on, the old spread*/

            if(v0 != NULL && v1 != NULL) {
                mp_rat p = num_FromInt(0), t = num_FromInt(0);

                if(p != NULL && t != NULL) {
                    mp_rat_sub(v1->op.num, v0->op.num, p);
                    if(mp_rat_compare_zero(p) > 0) {
                        mp_rat_neg(v0->op.num, t);
                        mp_rat_div(t, p, t);
                        lo = whole_part(t) - 1;
                        mp_rat_sub(h->op.num, v0->op.num, t);
                        mp_rat_div(t, p, t);
                        up = whole_part(t) + 1;
                        if(up - lo > 16) /*sin(100x), no*/
                            up = lo + 16;
                    }
                }
                num_Cleanup(p);
                num_Cleanup(t);
            }

            for(k = (int)lo; k <= up && !pcas_cancelled; k++) {
                pcas_ast_t *c, *a;
                bool in = false, dup = false;

                if(k == 0 && c0 != NULL) {
                    c = c0;
                    a = v0;
                    c0 = NULL;
                    v0 = NULL;
                } else if(k == 1 && c1 != NULL) {
                    c = c1;
                    a = v1;
                    c1 = NULL;
                    v1 = NULL;
                } else {
                    c = at_n(roots[i], sym, k);
                    a = c != NULL ? approx_number(c) : NULL;
                }
                if(a != NULL)
                    in = mp_rat_compare_zero(a->op.num) >= 0 && mp_rat_compare(a->op.num, h->op.num) < 0;
                /*sin(x) = 1 gives pi/2 from both families, keep one*/
                for(j = 0; in && j < kept; j++)
                    if(val[j] != NULL && mp_rat_compare(val[j]->op.num, a->op.num) == 0)
                        dup = true;
                if(in && !dup && kept < SOLVE_MAX) {
                    simplify(c, SIMP_ALL); /*the full one only for the keepers, pi/4 + pi/2 wants to be 3pi/4*/
                    keep[kept] = c;
                    val[kept++] = a;
                } else {
                    ast_Cleanup(c);
                    ast_Cleanup(a);
                }
            }
            ast_Cleanup(c0);
            ast_Cleanup(c1);
            ast_Cleanup(v0);
            ast_Cleanup(v1);
        }
        ast_Cleanup(roots[i]);
    }

    /*smallest first, by the decimal. the ones without a value (no n in them) stay put*/
    for(i = 1; i < kept; i++) {
        pcas_ast_t *r = keep[i], *rv = val[i];

        for(j = i; j > 0 && rv != NULL && val[j - 1] != NULL && mp_rat_compare(val[j - 1]->op.num, rv->op.num) > 0; j--) {
            keep[j] = keep[j - 1];
            val[j] = val[j - 1];
        }
        keep[j] = r;
        val[j] = rv;
    }
    for(i = 0; i < kept; i++) {
        roots[i] = keep[i];
        ast_Cleanup(val[i]);
    }
    for(i = kept; i < n; i++)
        roots[i] = NULL;
    ast_Cleanup(h);
    ast_Cleanup(sym);
    return kept;
}

/*smallest first, by the decimal. letters stay where they are*/
static void sort_roots(pcas_ast_t **out, int n) {
    pcas_ast_t *val[SOLVE_MAX];
    int i, j;

    /*by the decimal, so 1/2 and sqrt(2) sort too. letters stay where they are*/
    for(i = 0; i < n; i++)
        val[i] = approx_number(out[i]);
    for(i = 1; i < n; i++) {
        pcas_ast_t *r = out[i], *rv = val[i];

        for(j = i; j > 0 && rv != NULL && val[j - 1] != NULL
                   && mp_rat_compare(val[j - 1]->op.num, rv->op.num) > 0; j--) {
            out[j] = out[j - 1];
            val[j] = val[j - 1];
        }
        out[j] = r;
        val[j] = rv;
    }
    for(i = 0; i < n; i++)
        ast_Cleanup(val[i]);
}

int solve(pcas_ast_t *expr, Symbol var, pcas_ast_t **out) {
    return solve_ex(expr, var, out, false);
}

/*Fluxion: var under a fraction bar, (3x-y)/(x+y) = 5/8. clear_denominators only takes numbers off,
so it goes over one denominator here and the top gets solved. a root that makes the bottom 0
was never a root (x/x = 0 has none)*/
static int solve_over(pcas_ast_t *expr, Symbol var, pcas_ast_t **out, bool complex) {
    pcas_ast_t *c = ast_Copy(expr), *bottom, *sym;
    int n = SOLVE_UNKNOWN, i, k;

    if(c == NULL)
        return SOLVE_UNKNOWN;
    simplify(c, SIMP_SOLVE);
    comdenom(c, false);
    if(!pcas_cancelled && !has_symbol(c, var)) {
        /*nothing with var left, x/(x-1) = 1/(x-1) ends up 1 = 0. the x = 1 that made it cancel is out anyway*/
        n = is_zero(c) ? SOLVE_ALL : c->type == NODE_NUMBER ? 0 : SOLVE_UNKNOWN;
        ast_Cleanup(c);
        return n;
    }
    if(pcas_cancelled || !isoptype(c, OP_DIV) || !has_symbol(ast_ChildGet(c, 1), var)) {
        ast_Cleanup(c);
        return SOLVE_UNKNOWN;
    }
    bottom = ast_ChildGet(c, 1);
    n = solve_ex(ast_ChildGet(c, 0), var, out, complex);

    sym = ast_MakeSymbol(var);
    for(i = k = 0; n > 0 && i < n; i++) {
        pcas_ast_t *b = ast_Copy(bottom);
        bool zero;

        substitute(b, sym, out[i]);
        simplify(b, SIMP_ALL);
        zero = is_zero(b);
        ast_Cleanup(b);
        if(zero)
            ast_Cleanup(out[i]);
        else
            out[k++] = out[i];
    }
    if(n > 0) {
        for(i = k; i < n; i++)
            out[i] = NULL;
        n = k;
    }
    ast_Cleanup(sym);
    ast_Cleanup(c);
    return n;
}

int solve_ex(pcas_ast_t *expr, Symbol var, pcas_ast_t **out, bool complex) {
    pcas_ast_t *e = ast_Copy(expr);
    coefs_t p;
    int n = SOLVE_UNKNOWN, i;

    if(e == NULL) /*Fluxion: oom*/
        return SOLVE_UNKNOWN;

    simplify(e, SIMP_SOLVE);
    expand(e, EXP_ALL);
    simplify(e, SIMP_SOLVE);
    clear_denominators(&e);
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
            n = solve_factor(e, var, out, 0, complex);
        } else if(p.deg == 2) {
            n = solve_quadratic_sym(&p, out, 0);   /*letters in the coefs, no factoring those*/
        }
        coefs_free(&p);
    }

    if(n == SOLVE_UNKNOWN && !pcas_cancelled && count_var(e, var) == 1)
        n = isolate(e, var, out);   /*not a polynomial in it, but it's only there once*/

    if(n == SOLVE_UNKNOWN && !pcas_cancelled)
        n = solve_over(expr, var, out, complex);

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
