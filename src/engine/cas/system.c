/*
    Fluxion: solve_system(), not in PineappleCAS. linear systems the way you'd do
    them on paper, elimination. n equations in n unknowns, n up to SYSTEM_MAX,
    which is plenty for anything off a worksheet.

    the coefficient of x in a linear equation is just d/dx of it, and the constant
    is what's left with every variable set to 0. that's the whole reader, no term
    splitting. then "is it actually linear" is one check, the equation minus the
    pieces has to come out as 0. anything else is SOLVE_UNKNOWN (nonlinear ones
    are a todo, substitution probably)

    coefficients can have other letters in them. a letter pivot gets assumed
    nonzero, same as solve() does for a/x. a number pivot is taken first when
    there is one so that assumption is made as rarely as possible
*/

#include "cas.h"

#define SIMP_SYS (SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_RATIONAL | SIMP_EVAL | SIMP_LIKE_TERMS)

static bool zero(pcas_ast_t *e) {
    return e != NULL && is_ast_int(e, 0);
}

static pcas_ast_t *number(mp_small n) {
    return ast_MakeNumber(num_FromInt(n));
}

/*a (op) b, both copied, simplified. NULL when oom. number/number is done straight
  in imath, no tree to build and simplify for it (and the sign lands where we want)*/
static pcas_ast_t *combine(OperatorType op, pcas_ast_t *a, pcas_ast_t *b) {
    pcas_ast_t *e;

    if(op == OP_DIV && a->type == NODE_NUMBER && b->type == NODE_NUMBER && !zero(b)) {
        e = number(0);
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
    pcas_ast_t *neg = ast_MakeBinary(OP_MULT, number(-1), ast_Copy(b)), *e;

    e = combine(OP_ADD, a, neg);
    ast_Cleanup(neg);
    return e;
}

/*row r of the matrix gets its coefficients and constant read off eq. false if eq
  isn't linear in the vars (or oom). the constant goes on the right, so the row
  reads coef . var = const*/
static bool read_row(pcas_ast_t *eq, int n, const Symbol *vars, pcas_ast_t **row, pcas_ast_t **rhs) {
    pcas_ast_t *e = ast_Copy(eq), *check, *v, *z = number(0);
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
        pcas_ast_t *neg = number(-1), *r = combine(OP_MULT, neg, *rhs);

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

int solve_system(pcas_ast_t **eqs, int n, const Symbol *vars, pcas_ast_t **out) {
    pcas_ast_t *m[SYSTEM_MAX][SYSTEM_MAX], *rhs[SYSTEM_MAX];
    bool pivoted[SYSTEM_MAX];
    int i, j, col, r, result = n;

    if(n < 1 || n > SYSTEM_MAX || pcas_cancelled)
        return SOLVE_UNKNOWN;
    for(i = 0; i < n; i++) {
        rhs[i] = NULL;
        pivoted[i] = false;
        for(j = 0; j < n; j++)
            m[i][j] = NULL;
    }

    for(i = 0; i < n; i++)
        if(!read_row(eqs[i], n, vars, m[i], &rhs[i])) {
            result = SOLVE_UNKNOWN;
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
            out[i] = combine(OP_DIV, rhs[i], m[i][i]);
            if(out[i] == NULL || pcas_cancelled) {
                result = SOLVE_UNKNOWN;
                break;
            }
        }

done:
    if(result <= 0 && result != SOLVE_ALL)
        for(i = 0; i < n; i++) {
            ast_Cleanup(out[i]);
            out[i] = NULL;
        }
    for(i = 0; i < n; i++) {
        ast_Cleanup(rhs[i]);
        for(j = 0; j < n; j++)
            ast_Cleanup(m[i][j]);
    }
    if(pcas_cancelled)
        result = SOLVE_UNKNOWN;
    return result;
}
