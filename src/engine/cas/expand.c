#include "cas.h"

pcas_ast_t *combine(pcas_ast_t *add, pcas_ast_t *b) {
    unsigned i, j;
    pcas_ast_t *expanded = ast_MakeOperator(OP_ADD);

    if(isoptype(b, OP_ADD)) {

        for(i = 0; i < ast_ChildLength(add); i++) {
            for(j = 0; j < ast_ChildLength(b); j++) {

                ast_ChildAppend(expanded, ast_MakeBinary(OP_MULT,
                                            ast_Copy(ast_ChildGet(add, i)),
                                            ast_Copy(ast_ChildGet(b, j))
                                        ));
            }
        }

    } else {

        for(i = 0; i < ast_ChildLength(add); i++) {
            ast_ChildAppend(expanded, ast_MakeBinary(OP_MULT,
                                        ast_Copy(ast_ChildGet(add, i)),
                                        ast_Copy(b)
                                    ));
        }
    }

    return expanded;
}

/*(A/B)^C is A^C/B^C for a whole C, or a B over 0. otherwise sqrt(1/X) and 1/sqrt(X) differ below 0*/
static bool div_splits(pcas_ast_t *base, pcas_ast_t *power) {
    return is_whole_node(power) || known_positive(ast_ChildGet(base, 1));
}

/*Fluxion: (AB)^C only comes apart over factors that cant be negative, sqrt(-3X^3Y) is sqrt(3)abs(X)sqrt(-XY) and sqrt(XY) stays whole, both could be under 0. NULL if nothing comes out*/
static pcas_ast_t *split_product(pcas_ast_t *base, pcas_ast_t *power) {
    pcas_ast_t *out = ast_MakeOperator(OP_MULT), *rest = ast_MakeOperator(OP_MULT), *c, *p, *r;
    mp_small n;
    unsigned i;

    if(out == NULL || rest == NULL) { /*oom*/
        if(out != NULL)
            ast_Cleanup(out);
        if(rest != NULL)
            ast_Cleanup(rest);
        return NULL;
    }
    for(i = 0; i < ast_ChildLength(base); i++) {
        c = ast_ChildGet(base, i);
        if(known_positive(c)) {
            ast_ChildAppend(out, ast_MakeBinary(OP_POW, ast_Copy(c), ast_Copy(power)));
        } else if(c->type == NODE_NUMBER && mp_rat_compare_zero(c->op.num) < 0) { /*-3 is 3 and a -1*/
            if(mp_rat_compare_value(c->op.num, -1, 1) != 0) {
                p = ast_Copy(c);
                mp_rat_abs(p->op.num, p->op.num);
                ast_ChildAppend(out, ast_MakeBinary(OP_POW, p, ast_Copy(power)));
            }
            ast_ChildAppend(rest, ast_MakeNumber(num_FromInt(-1)));
        } else if(isoptype(c, OP_POW) && is_whole_node(ast_ChildGet(c, 1))
                  && mp_int_to_int(mp_rat_numer_ref(ast_ChildGet(c, 1)->op.num), &n) == MP_OK && n >= 2) { /*X^3 is X^2 and an X*/
            ast_ChildAppend(out, ast_MakeBinary(OP_POW,
                                    ast_MakeBinary(OP_POW, ast_Copy(ast_ChildGet(c, 0)), ast_MakeNumber(num_FromInt((int)(n - n % 2)))),
                                    ast_Copy(power)));
            if(n % 2)
                ast_ChildAppend(rest, ast_Copy(ast_ChildGet(c, 0)));
        } else {
            ast_ChildAppend(rest, ast_Copy(c));
        }
    }
    if(ast_ChildLength(out) == 0) {
        ast_Cleanup(out);
        ast_Cleanup(rest);
        return NULL;
    }
    if(ast_ChildLength(rest) > 0) {
        r = ast_ChildLength(rest) == 1 ? ast_Copy(ast_ChildGet(rest, 0)) : ast_Copy(rest);
        ast_ChildAppend(out, ast_MakeBinary(OP_POW, r, ast_Copy(power)));
    }
    ast_Cleanup(rest);
    return out;
}

static bool _expand(pcas_ast_t *e, unsigned char flags) {
    unsigned i, j;

    bool did_change = false;
    bool intermediate_change = false;

    if(pcas_cancelled) /*Fluxion: stop here, the answer's being thrown away*/
        return false;

    for(i = 0; i < ast_ChildLength(e); i++) {
        pcas_ast_t *child = ast_ChildGet(e, i);

        /*Fluxion: a fraction under a power that wont split stays one, 3*(1/X) under sqrt never joined back up*/
        if(i == 0 && isoptype(e, OP_POW) && isoptype(child, OP_DIV) && !div_splits(child, ast_ChildGet(e, 1))) {
            for(j = 0; j < ast_ChildLength(child); j++)
                did_change |= _expand(ast_ChildGet(child, j), flags);
        } else {
            did_change |= _expand(child, flags);
        }
    }

    do {
        intermediate_change = false;
        simplify(e, SIMP_COMMUTATIVE);

        if(isoptype(e, OP_MULT)) {

            for(i = 0; i < ast_ChildLength(e); i++) {
                pcas_ast_t *ichild = ast_ChildGet(e, i);

                if(isoptype(ichild, OP_ADD)) {

                    for(j = 0; j < ast_ChildLength(e); j++) {
                        pcas_ast_t *jchild = ast_ChildGet(e, j);
                        if(i != j) {
                            bool should_expand;

                            /*Yes I am sure I can do this in one line in the if statement
                            but this is clearer*/

                            if(jchild->type == NODE_NUMBER)
                                should_expand = flags & EXP_DISTRIB_NUMBERS;
                            else if(isoptype(jchild, OP_ADD))
                                should_expand = flags & EXP_DISTRIB_ADDITION;
                            else
                                should_expand = flags & EXP_DISTRIB_MULTIPLICATION;

                            if(should_expand) {
                                pcas_ast_t *combined = combine(ichild, jchild);

                                /*Fluxion: collect like terms every step, not at the very end.
                                (X+1)^n used to build all 2^n raw terms first, ^7 took 5 min
                                on the calculator and ^8 ran out of memory. only when asked,
                                simplify's own expands want the shape left alone*/
                                if(flags & EXP_COLLECT_TERMS)
                                    simplify(combined, SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_RATIONAL | SIMP_EVAL | SIMP_LIKE_TERMS);

                                ast_ChildAppend(e, combined);

                                ast_Cleanup(ast_ChildRemove(e, ichild));
                                ast_Cleanup(ast_ChildRemove(e, jchild));

                                intermediate_change = true;
                                did_change = true;
                                break;
                            }

                        }
                    }

                    if(intermediate_change)
                        break;
                }

            }

        } 

        if((flags & EXP_DISTRIB_DIVISION) && isoptype(e, OP_DIV)) {
            pcas_ast_t *num, *den;

            num = ast_ChildGet(e, 0);
            den = ast_ChildGet(e, 1);

            /*Split A/B into A * (1/B)*/
            replace_node(e, ast_MakeBinary(OP_MULT, ast_Copy(num), ast_MakeBinary(OP_DIV, ast_MakeNumber(num_FromInt(1)), ast_Copy(den))));

            /*This could be dangerous, but we'll cross that bridge when we get there.*/
            _expand(e, EXP_DISTRIB_NUMBERS | EXP_DISTRIB_ADDITION | EXP_DISTRIB_MULTIPLICATION | EXP_DISTRIB_ADDITION | EXP_DISTRIB_POWERS);

            intermediate_change = true;
            did_change = true;
            continue;
        }

        if(isoptype(e, OP_POW)) {
            pcas_ast_t *base, *power;

            base = ast_ChildGet(e, 0);
            power = ast_ChildGet(e, 1);

            if(flags & EXP_DISTRIB_POWERS) {

                /*Change (AB)^2 to A^2B^2*/
                if(isoptype(base, OP_MULT)) {
                    pcas_ast_t *replacement;

                    if(is_whole_node(power)) {
                        replacement = ast_MakeOperator(OP_MULT);

                        for(j = 0; j < ast_ChildLength(base); j++) {
                            pcas_ast_t *cur = ast_ChildGet(base, j);

                            ast_ChildAppend(replacement, ast_MakeBinary(OP_POW,
                                                                        ast_Copy(cur),
                                                                        ast_Copy(power)
                            ));
                        }
                    } else {
                        replacement = split_product(base, power);
                    }

                    if(replacement != NULL) {
                        replace_node(e, replacement);

                        intermediate_change = true;
                        did_change = true;
                        continue;
                    }
                }

                /*Change (A/B)^2 to A^2/B^2&. a B under 0 needs the whole number*/
                else if(isoptype(base, OP_DIV) && div_splits(base, power)) {
                    pcas_ast_t *new_num, *new_den, *new_div;
                    new_num = ast_MakeBinary(OP_POW, ast_Copy(ast_ChildGet(base, 0)), ast_Copy(power));
                    new_den = ast_MakeBinary(OP_POW, ast_Copy(ast_ChildGet(base, 1)), ast_Copy(power));
                    new_div = ast_MakeBinary(OP_DIV, new_num, new_den);

                    replace_node(e, new_div);

                    intermediate_change = true;
                    did_change = true;
                    continue;
                }

            }

            if(flags & EXP_EXPAND_POWERS) {
                mp_int val;
                pcas_ast_t *replacement;

                /*Fluxion: the power has to be a number before it's read as one
                (sqrt(2+i) crashed here), and a huge one isn't multiplied out:
                (X+1)^(10^9) would be 10^9 copies*/
                if(isoptype(base, OP_ADD) && power->type == NODE_NUMBER && mp_rat_is_integer(power->op.num)
                   && mp_rat_compare_value(power->op.num, EXPAND_MAX_POWER, 1) <= 0) {
                    val = &power->op.num->num;
                    replacement = ast_MakeOperator(OP_MULT);

                    while(mp_int_compare_zero(val) > 0 && !pcas_check()) {

                        ast_ChildAppend(replacement, ast_Copy(base));

                        mp_int_sub_value(val, 1, val);
                    }

                    replace_node(e, replacement);

                    intermediate_change = true;
                    did_change = true;
                    continue;
                }
            }
        }

    } while(intermediate_change && !pcas_check());

    simplify(e, SIMP_NORMALIZE | SIMP_COMMUTATIVE);

    return did_change;
}

bool expand(pcas_ast_t *e, unsigned char flags) {
    bool changed = false;

    if(pcas_cancelled)
        return false;
    /*Fluxion: the plain polynomial parts get done flat first (poly.c), the tree
    way below is minutes on the calculator for (x+1)^8*/
    if(flags & EXP_EXPAND_POWERS)
        changed |= poly_expand(e);
    /*Expand powers first to make things faster*/
    if(flags & EXP_EXPAND_POWERS)
        changed |= _expand(e, EXP_EXPAND_POWERS);
    changed |= _expand(e, flags & ~EXP_EXPAND_POWERS);
    return changed;
}