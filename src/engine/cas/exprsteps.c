/*Fluxion: steps for expand( and factor(, the way you'd do them on paper. not in upstream PineappleCAS.*/

#include "steps.h"

#define ROUNDS_MAX 8
#define POWER_MAX 6 /*(x+1)^6 is 7 terms, written out as a product that's already a lot of lines*/

/*the first product with a sum in it, or a sum to a power: the node, NULL for none*/
static pcas_ast_t *find_open(pcas_ast_t *e) {
    pcas_ast_t *c, *f;
    mp_small n;

    if(e->type != NODE_OPERATOR)
        return NULL;
    if(isoptype(e, OP_POW) && isoptype(ast_ChildGet(e, 0), OP_ADD) && whole_number(ast_ChildGet(e, 1), &n) && n >= 2 && n <= POWER_MAX)
        return e;
    if(isoptype(e, OP_MULT))
        for(c = ast_ChildGet(e, 0); c != NULL; c = c->next)
            if(isoptype(c, OP_ADD))
                return e;
    for(c = ast_ChildGet(e, 0); c != NULL; c = c->next)
        if((f = find_open(c)) != NULL)
            return f;
    return NULL;
}

/*a letter, or a letter to a whole power: its symbol and the power*/
static bool letter_power(pcas_ast_t *e, Symbol *sym, mp_small *n) {
    if(e->type == NODE_SYMBOL && e->op.symbol != SYM_EULER && e->op.symbol != SYM_PI) {
        *sym = e->op.symbol;
        *n = 1;
        return true;
    }
    if(isoptype(e, OP_POW) && ast_ChildGet(e, 0)->type == NODE_SYMBOL && whole_number(ast_ChildGet(e, 1), n) && *n >= 1) {
        *sym = ast_ChildGet(e, 0)->op.symbol;
        return true;
    }
    return false;
}

/*x x in a product as x^2, and x x^2 as x^3. the simplify that does it also folds 1/4+x/2+x^2/4 back into a square*/
static void squash(pcas_ast_t *prod) {
    pcas_ast_t *a, *b;
    Symbol s, t;
    mp_small n, m;

    for(a = ast_ChildGet(prod, 0); a != NULL; a = a->next) {
        if(!letter_power(a, &s, &n))
            continue;
        for(b = a->next; b != NULL; b = b->next) {
            if(!letter_power(b, &t, &m) || s != t)
                continue;
            replace_node(a, ast_MakeBinary(OP_POW, ast_MakeSymbol(s), ast_MakeInt(n + m)));
            ast_Cleanup(ast_ChildRemoveIndex(prod, ast_ChildIndexOf(prod, b)));
            squash(prod);
            return;
        }
    }
}

static void squash_all(pcas_ast_t *e) {
    pcas_ast_t *c;

    if(e->type != NODE_OPERATOR)
        return;
    for(c = ast_ChildGet(e, 0); c != NULL; c = c->next)
        squash_all(c);
    if(isoptype(e, OP_MULT))
        squash(e);
}

/*node written one step further: (a+b)^3 as (a+b)(a+b)^2, c(a+b)(d) as ca d + cb d. the rule's name*/
static const char *open_up(pcas_ast_t *node) {
    pcas_ast_t *c, *t, *f, *sum, *term;
    mp_small n;

    if(isoptype(node, OP_POW)) {
        whole_number(ast_ChildGet(node, 1), &n);
        replace_node(node, ast_MakeBinary(OP_MULT, ast_Copy(ast_ChildGet(node, 0)),
                                          n == 2 ? ast_Copy(ast_ChildGet(node, 0)) : ast_MakeBinary(OP_POW, ast_Copy(ast_ChildGet(node, 0)), ast_MakeInt(n - 1))));
        return "write the power as a product";
    }
    for(c = ast_ChildGet(node, 0); c != NULL && !isoptype(c, OP_ADD); c = c->next)
        ;
    if((sum = ast_MakeOperator(OP_ADD)) == NULL)
        return NULL;
    for(t = ast_ChildGet(c, 0); t != NULL; t = t->next) {
        term = ast_MakeOperator(OP_MULT);
        if(!is_ast_int(t, 1)) /*1(x+2) is just (x+2)*/
            ast_ChildAppend(term, ast_Copy(t));
        for(f = ast_ChildGet(node, 0); f != NULL; f = f->next)
            if(f != c)
                ast_ChildAppend(term, ast_Copy(f));
        ast_ChildAppend(sum, term);
    }
    replace_node(node, sum);
    return "distribute";
}

bool steps_expand(pcas_ast_t *e, steps_t *out) {
    pcas_ast_t *cur = ast_Copy(e), *node, *done;
    step_t *l;
    const char *rule;
    int rounds;

    out->n = 0;
    if(cur == NULL)
        return false;
    simplify(cur, SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_EVAL);
    l = steps_expr(out, ast_Copy(cur));
    for(rounds = 0; l != NULL && rounds < ROUNDS_MAX && (node = find_open(cur)) != NULL && !pcas_cancelled; rounds++) {
        if((rule = open_up(node)) == NULL)
            break;
        simplify(cur, SIMP_LIGHT);
        squash_all(cur);
        steps_rule(l, rule);
        l = steps_expr(out, ast_Copy(cur));
    }
    /*what's left is the sum of products with no brakets: the powers and like terms*/
    done = ast_Copy(cur);
    expand(done, EXP_ALL);
    simplify(done, SIMP_LIGHT | SIMP_LIKE_TERMS);
    ast_Cleanup(cur);
    if(l == NULL || pcas_cancelled || out->n < 2) { /*nothing to open, or no room*/
        ast_Cleanup(done);
        steps_free(out);
        return false;
    }
    steps_rule(&out->line[out->n - 1], "multiply out and add like terms");
    steps_expr(out, done);
    steps_shown(out);
    return true;
}

/*the one letter e has, false for none or more than one*/
static bool only_letter(pcas_ast_t *e, Symbol *var) {
    Symbol c;
    int found = 0;

    for(c = SYM_A; c <= SYM_Z; c++)
        if(has_symbol(e, c)) {
            *var = c;
            found++;
        }
    return found == 1;
}

/*the polynomial's own factoring lines up to the product (the equation steps carry on to x = ...), each one just the expression*/
bool steps_factor(pcas_ast_t *e, steps_t *out) {
    pcas_ast_t *zero = ast_MakeInt(0);
    Symbol var = SYM_X;
    bool ok = false;
    int k, i;

    out->n = 0;
    if(zero == NULL || !only_letter(e, &var)) {
        ast_Cleanup(zero);
        return false;
    }
    if(steps_fit(e, zero, var, METHOD_FACTOR))
        ok = steps_solve(e, zero, var, METHOD_FACTOR, out);
    else
        ok = steps_poly(e, zero, var, out);
    ast_Cleanup(zero);
    if(!ok)
        return false;
    for(k = 1; k < out->n && !isoptype(out->line[k].eq[0].left, OP_MULT) && !isoptype(out->line[k].eq[0].left, OP_POW); k++)
        ;
    if(k == out->n) { /*never got to a product*/
        steps_free(out);
        return false;
    }
    steps_cut(out, k + 1);
    for(i = 0; i < out->n; i++) { /*= 0 is for the equation, a factored form has none*/
        step_eq_t *q = &out->line[i].eq[0];

        ast_Cleanup(q->right);
        q->right = NULL;
        out->line[i].eqs = 1;
        out->line[i].list = false;
        if(i == out->n - 1) {
            out->line[i].move = 0;
            out->line[i].by = NULL;
        }
    }
    return true;
}
