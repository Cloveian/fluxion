#include "cas.h"
#include "identities.h"

/*Executes the SIMP_COMMUTATIVE flag*/
bool simplify_commutative(pcas_ast_t *e) {
    unsigned i;
    bool changed = false;

    if(e->type != NODE_OPERATOR)
        return false;

    if(ast_ChildLength(e) == 1
        && (optype(e) == OP_MULT || optype(e) == OP_ADD)) {
        replace_node(e, ast_ChildGet(e, 0));
        simplify_commutative(e);
        return true;
    }

    for(i = 0; i < ast_ChildLength(e); i++) {

        pcas_ast_t *child = ast_ChildGet(e, i);

        /*If this child is the same type as parent and
        the parent's type is commutative*/
        if(child->type == NODE_OPERATOR 
            && is_op_commutative(optype(e))
            && optype(child) == optype(e)) {

            /*Append children of child to end of parent*/
            ast_ChildGetLast(e)->next = child->op.operator.base;
            /*Remove child node*/
            ast_ChildRemoveIndex(e, i);
            /*Remove reference to our new children so we can dealloc it*/
            child->op.operator.base = NULL;
            ast_Cleanup(child);

            changed = true;
            i--;
        } else {
            changed |= simplify_commutative(child);
        }
    }

    return changed;
}

/*Change a to b and delete b*/
void replace_node(pcas_ast_t *a, pcas_ast_t *b) {

    if(pcas_cancelled) { /*Fluxion: stop here, the answer's being thrown away. b is ours to free*/
        /*unless it's still a's, eval_pow does replace_node(e, a)*/
        if(ast_ChildIndexOf(a, b) == (LSIZE)-1)
            ast_Cleanup(b);
        return;
    }

    if(a == NULL || b == NULL) /*Fluxion: oom*/
        return;

    if(a->type == NODE_OPERATOR) {

        while(opbase(a) != NULL) {
            pcas_ast_t *child = ast_ChildRemoveIndex(a, 0);
            if(child != b)
                ast_Cleanup(child);
        }

    } else if(a->type == NODE_NUMBER) {
        num_Cleanup(a->op.num);
    }

    a->type = b->type;
    a->op = b->op;

    free(b);
}

/*Fluxion: see cas.h. pcas_cancelled lives in error.c now, out of memory sets it too*/
bool (*pcas_poll)(void) = NULL;
unsigned long pcas_work;

bool pcas_check(void) {
    pcas_work++;
    if(!pcas_cancelled && pcas_poll != NULL && pcas_poll())
        pcas_cancelled = true;
    return pcas_cancelled;
}

/*Executes the SIMP_RATIONAL flag*/
bool simplify_rational(pcas_ast_t *e) {
    unsigned i;
    bool changed = false;

    if(pcas_cancelled) /*Fluxion: stop here, the answer's being thrown away*/
        return false;

    if(e == NULL) /*Fluxion: oom*/
        return false;

    if(e->type != NODE_OPERATOR)
        return false;

    for(i = 0; i < ast_ChildLength(e); i++) {
        pcas_ast_t *child = ast_ChildGet(e, i);
        if(pcas_cancelled) /*Fluxion: replace_node does nothing now, the i-- below would spin forever*/
            break;

        if(isoptype(child, OP_DIV)) {
            pcas_ast_t *child_num, *child_den;

            child_num = ast_ChildGet(child, 0);
            child_den = ast_ChildGet(child, 1);

            if(optype(e) == OP_DIV) {
                pcas_ast_t *e_num, *e_den;

                e_num = ast_ChildGet(e, 0);
                e_den = ast_ChildGet(e, 1);

                /*
                    If the child that is a division node is the numerator of this division node
                    (A/B)/C = A/(B*C)
                */
                if(i == 0) {

                    pcas_ast_t *new_num, *new_den;

                    new_num = ast_Copy(child_num);
                    new_den = ast_MakeBinary(OP_MULT,
                                                     ast_Copy(child_den),
                                                     ast_Copy(e_den));

                    replace_node(e, ast_MakeBinary(OP_DIV, new_num, new_den));

                    changed = true;
                    /*Decrement i so we continue to simplify this node*/
                    i--;
                }
                /*
                    If the child that is a division node is the denominator of this division node
                    A/(B/C) = (A*C)/B
                */ 
                else {

                    pcas_ast_t *new_num, *new_den;

                    new_num = ast_MakeBinary(OP_MULT,
                                                   ast_Copy(e_num),
                                                   ast_Copy(child_den));
                    new_den = ast_Copy(child_num);

                    replace_node(e, ast_MakeBinary(OP_DIV, new_num, new_den));

                    changed = true;
                    i--;
                }

            } 
            /*
                A * (B / C) = (A * B) / C
                (A / B) * C = (C * A) / B
            */
            else if(optype(e) == OP_MULT) {
                pcas_ast_t *mult, *num_copy, *den_copy;

                mult = ast_Copy(e);
                num_copy = ast_Copy(child_num);
                den_copy = ast_Copy(child_den);

                /*Multiply by division denominator*/
                ast_ChildAppend(mult, num_copy);

                /*Remove division node*/
                ast_Cleanup(ast_ChildRemoveIndex(mult, i));

                replace_node(e, ast_MakeBinary(OP_DIV, mult, den_copy));

                /*Continue to simplify the other multiplication nodes now that our type has changed*/
                simplify_rational(mult);
                simplify_rational(ast_ChildGetLast(e));

                changed = true;
                i--;
            } else {
                changed |= simplify_rational(child);
            }

        } else {
            changed |= simplify_rational(child);
        }
    }

    return changed;
}

/*Executes the SIMP_NORMALIZE flag*/
bool simplify_normalize(pcas_ast_t *e) {
    pcas_ast_t *child;
    bool changed = false;

    if(pcas_cancelled) /*Fluxion: stop here, the answer's being thrown away*/
        return false;

    if(e->type == NODE_OPERATOR) {

        for(child = opbase(e); child != NULL; child = child->next)
            changed |= simplify_normalize(child);

        if(optype(e) == OP_ROOT) {
            /*a root of b*/
            pcas_ast_t *a, *b;

            a = ast_Copy(ast_ChildGet(e, 0));
            b = ast_Copy(ast_ChildGet(e, 1));

            replace_node(e, ast_MakeBinary(OP_POW,
                                b,
                                ast_MakeBinary(OP_DIV,
                                    ast_MakeNumber(num_FromInt(1)),
                                    a
            )));

            changed = true;
        }

        /*Fluxion: a minus on the bottom goes up top, 17/-28 is -17/28. the
          fraction code keeps the sign wherever the arithmetic left it*/
        if(optype(e) == OP_DIV && ast_ChildGet(e, 1)->type == NODE_NUMBER
           && mp_rat_compare_zero(ast_ChildGet(e, 1)->op.num) < 0) {
            pcas_ast_t *num = ast_ChildGet(e, 0), *den = ast_ChildGet(e, 1);

            mp_rat_neg(den->op.num, den->op.num);
            if(num->type == NODE_NUMBER)
                mp_rat_neg(num->op.num, num->op.num);
            else
                replace_node(num, ast_MakeBinary(OP_MULT, ast_MakeNumber(num_FromInt(-1)), ast_Copy(num)));
            changed = true;
        }
    } else if(e->type == NODE_NUMBER && !mp_rat_is_integer(e->op.num)) {
        mp_rat num, den;

        num = num_FromInt(1);
        den = num_FromInt(1);

        mp_rat_reduce(e->op.num);

        mp_int_copy(&e->op.num->num, &num->num);
        mp_int_copy(&e->op.num->den, &den->num);

        replace_node(e, ast_MakeBinary(OP_DIV, ast_MakeNumber(num), ast_MakeNumber(den)));

        changed = true;
    }

    return changed;
}

bool has_imaginary_node(pcas_ast_t *e) {
    pcas_ast_t *child;

    if(e->type == NODE_SYMBOL && e->op.symbol == SYM_IMAG)
        return true;

    if(e->type == NODE_OPERATOR)
        for(child = ast_ChildGet(e, 0); child != NULL; child = child->next)
            if(has_imaginary_node(child))
                return true;

    return false;
}

/*Expects everything to be completely simplified*/
bool is_negative_for_sure(pcas_ast_t *a) {
    if(a == NULL) /*Fluxion: oom*/
        return false;

    if(a->type == NODE_NUMBER && mp_rat_compare_zero(a->op.num) < 0)
        return true;

    if(isoptype(a, OP_MULT)) {
        pcas_ast_t *child;
        for(child = ast_ChildGet(a, 0); child != NULL; child = child->next)
            if(is_negative_for_sure(child))
                return true;
    } else if(isoptype(a, OP_DIV)) {
        return is_negative_for_sure(ast_ChildGet(a, 0)) || is_negative_for_sure(ast_ChildGet(a, 1));
    }

    return false;
}

/*Returns true if changed. Expects completely simplified.*/
bool absolute_val(pcas_ast_t *e) {
    if(e == NULL) /*Fluxion: oom*/
        return false;

    if(e->type == NODE_NUMBER && mp_rat_compare_zero(e->op.num) < 0) {
        mp_rat_abs(e->op.num, e->op.num);
        return true;
    }

    if(isoptype(e, OP_MULT)) {
        pcas_ast_t *child;
        for(child = ast_ChildGet(e, 0); child != NULL; child = child->next)
            if(absolute_val(child))
                return true;
    } else if(isoptype(e, OP_DIV)) {
        return absolute_val(ast_ChildGet(e, 0)) || absolute_val(ast_ChildGet(e, 1));
    }

    return false;
}
#include <stdio.h>
/*Fluxion: how deep compare is in its own divide by the gcd. the tests never go past 1, a/g and b/g growing
  each time (-isqrt(3)-sqrt(99)-pi+sqrt(3)/2) went 400 terms deep*/
#define COMPARE_DEPTH_MAX 4
static int compare_depth;

/*returns negative if a < b, 0 if a=b, positive if a > b in terms of sorting order*/
int compare(pcas_ast_t *a, pcas_ast_t *b, bool add) {
    pcas_ast_t *temp;
    int multiplier = 1, order;

    if(pcas_cancelled) /*Fluxion: stop here, the answer's being thrown away*/
        return 0;

    if(a == NULL || b == NULL) /*Fluxion: oom*/
        return 0;

    if(add && gm_order(a, b, &order)) /*Fluxion: terms of a sum go by their letters*/
        return order;

    if(isoptype(b, OP_MULT)) {
        temp = a;
        a = b;
        b = temp;
        multiplier *= -1;
    }

    if(isoptype(a, OP_MULT) && compare_depth < COMPARE_DEPTH_MAX) { /*Fluxion*/
        pcas_ast_t *g = gcd(a, b);

        if(g != NULL && !is_ast_int(g, 1)) { /*Fluxion: NULL is an oom*/
            pcas_ast_t *newa, *newb;
            int val;

            /*Fluxion: monomials divide directly, see gm_div*/
            newa = gm_div(a, g);
            newb = gm_div(b, g);
            if(newa == NULL) {
                newa = ast_MakeBinary(OP_DIV, ast_Copy(a), ast_Copy(g));
                simplify(newa, SIMP_NORMALIZE | SIMP_RATIONAL | SIMP_EVAL);
            }
            if(newb == NULL) {
                newb = ast_MakeBinary(OP_DIV, ast_Copy(b), ast_Copy(g));
                simplify(newb, SIMP_NORMALIZE | SIMP_RATIONAL | SIMP_EVAL);
            }

            compare_depth++;
            val = compare(newa, newb, add);
            compare_depth--;

            ast_Cleanup(g);
            ast_Cleanup(newa);
            ast_Cleanup(newb);

            return multiplier * val;
        }

        ast_Cleanup(g);
    }

    /*Compare power bases to sort alphabetically when multiplying, and by degree when adding*/

    if(isoptype(b, OP_POW)) {
        temp = a;
        a = b;
        b = temp;
        multiplier *= -1;
    }
    if(isoptype(a, OP_POW)) {
        pcas_ast_t *abase, *apower;

        abase = ast_ChildGet(a, 0);
        apower = ast_ChildGet(a, 1);

        if(isoptype(b, OP_POW)) {
            pcas_ast_t *bbase, *bpower;

            bbase = ast_ChildGet(b, 0);
            bpower = ast_ChildGet(b, 1);

            return multiplier * (add ? -1 * compare(apower, bpower, add) : compare(abase, bbase, add));
        }

    }

    if(is_negative_for_sure(a) && !is_negative_for_sure(b))
        return multiplier * (add ? 1 : -1);
    else if(is_negative_for_sure(b) && !is_negative_for_sure(a))
        return multiplier * (add ? -1 : 1);

    /*NODE_NUMBER < NODE_SYMBOL < NODE_OPERATOR*/
    if(a->type < b->type) return multiplier * (add ? 1 : -1);
    else if(a->type > b->type) return multiplier * (add ? -1 : 1);

    /*
     * Don't deal with sorting multivariable polynomials
     * Maybe we'll revisit this in the future
     */
    if(isoptype(a, OP_POW))
        return 0;

    switch(a->type) {
    case NODE_NUMBER:   return multiplier * mp_rat_compare(a->op.num, b->op.num);
    case NODE_SYMBOL:   return multiplier * (a->op.symbol - b->op.symbol);
    case NODE_OPERATOR: return multiplier * ((optype(a) + (optype(a) < OP_POW ? AMOUNT_SYMBOLS : 0)) - (optype(b) + (optype(a) < OP_POW ? AMOUNT_SYMBOLS : 0)));
    }

    return 0;
}

/*
    Order multiplication and division.
    Change XAZ to AXZ and 1+sin(X)ZA5 to 5AZsin(X)+1
    Rationalize denominators.
    Change powers to roots.
    Combine a^5b^5 to (ab)^5.

    Use only when exporting and do not plan to simplify again
    or execute any other function on ast. Messes up all other algorithms.

    Sorting for addition and multiplication is O(n^2) by insertion sort
*/
/*Fluxion: an exponent p/q (a number, or a number over a number), small enough to use*/
static bool small_fraction(pcas_ast_t *power, mp_small *p, mp_small *q) {
    if(power->type == NODE_NUMBER) {
        if(mp_rat_to_ints(power->op.num, p, q) != MP_OK)
            return false;
    } else if(isoptype(power, OP_DIV)) {
        pcas_ast_t *top = ast_ChildGet(power, 0), *bottom = ast_ChildGet(power, 1);

        if(top->type != NODE_NUMBER || bottom->type != NODE_NUMBER
           || !mp_rat_is_integer(top->op.num) || !mp_rat_is_integer(bottom->op.num)
           || mp_int_to_int(&top->op.num->num, p) != MP_OK || mp_int_to_int(&bottom->op.num->num, q) != MP_OK)
            return false;
    } else {
        return false;
    }
    return *p > 0 && *q > 0 && *p <= 64 && *q <= 64;
}

/*Fluxion: what a root below gets multiplied by to leave its base, root(n, b)^(n-1). a square root is just itself*/
static pcas_ast_t *root_cofactor(pcas_ast_t *root) {
    mp_small p, q;

    if(small_fraction(ast_ChildGet(root, 0), &p, &q) && q == 1 && p > 2)
        return ast_MakeBinary(OP_POW, ast_Copy(root), ast_MakeInt((long)p - 1));
    return ast_Copy(root);
}

/*Fluxion: a whole number below against the number on top, (-2sqrt(2))/2 -> -sqrt(2)*/
static void cancel_numbers(pcas_ast_t *e) {
    pcas_ast_t *num = ast_ChildGet(e, 0), *den = ast_ChildGet(e, 1), *k = num;
    mp_rat g;

    if(den->type != NODE_NUMBER || !mp_rat_is_integer(den->op.num) || mp_rat_compare_zero(den->op.num) == 0)
        return;
    if(isoptype(num, OP_MULT))
        for(k = ast_ChildGet(num, 0); k != NULL && k->type != NODE_NUMBER; k = k->next);
    if(k == NULL || k->type != NODE_NUMBER || !mp_rat_is_integer(k->op.num) || (g = num_FromInt(1)) == NULL)
        return;
    mp_int_gcd(mp_rat_numer_ref(k->op.num), mp_rat_numer_ref(den->op.num), mp_rat_numer_ref(g));
    if(mp_rat_compare_zero(den->op.num) < 0)
        mp_rat_neg(g, g);
    mp_rat_div(k->op.num, g, k->op.num);
    mp_rat_div(den->op.num, g, den->op.num);
    num_Cleanup(g);
    if(k != num && mp_rat_compare_value(k->op.num, 1, 1) == 0 && ast_ChildLength(num) > 1)
        ast_Cleanup(ast_ChildRemove(num, k));
    if(isoptype(num, OP_MULT) && ast_ChildLength(num) == 1)
        replace_node(num, ast_Copy(ast_ChildGet(num, 0)));
    if(mp_rat_compare_value(den->op.num, 1, 1) == 0)
        replace_node(e, ast_Copy(num));
}

/*Fluxion: a^k b^k for two numbers over 0 as one, worked out: sqrt(2)sqrt(6) -> 2sqrt(3). NULL otherwise, sqrt(-2)sqrt(-3) isn't sqrt(6)*/
static pcas_ast_t *number_power(pcas_ast_t *a, pcas_ast_t *b, pcas_ast_t *k) {
    pcas_ast_t *merged;
    mp_rat ab;

    if(a->type != NODE_NUMBER || b->type != NODE_NUMBER
       || mp_rat_compare_zero(a->op.num) <= 0 || mp_rat_compare_zero(b->op.num) <= 0)
        return NULL;

    ab = num_Copy(a->op.num);
    mp_rat_mul(ab, b->op.num, ab);
    merged = ast_MakeBinary(OP_POW, ast_MakeNumber(ab), ast_Copy(k));
    simplify(merged, SIMP_EVAL);
    return merged;
}

/*Fluxion: the number in a term, the 2 of 2sqrt(3). NULL if none*/
static pcas_ast_t *term_number(pcas_ast_t *t) {
    if(t->type == NODE_NUMBER)
        return t;
    if(!isoptype(t, OP_MULT) || ast_ChildLength(t) != 2)
        return NULL;
    if(ast_ChildGet(t, 0)->type == NODE_NUMBER)
        return ast_ChildGet(t, 0);
    if(ast_ChildGet(t, 1)->type == NODE_NUMBER)
        return ast_ChildGet(t, 1);
    return NULL;
}

/*Fluxion: a number, or a number times sqrt(number). *root for the second*/
static bool surd_term(pcas_ast_t *t, bool *root) {
    pcas_ast_t *k = term_number(t), *r = t;

    *root = false;
    if(t->type == NODE_NUMBER)
        return true;
    if(k != NULL)
        r = k == ast_ChildGet(t, 0) ? ast_ChildGet(t, 1) : ast_ChildGet(t, 0);
    *root = isoptype(r, OP_ROOT) && is_ast_int(ast_ChildGet(r, 0), 2) && ast_ChildGet(r, 1)->type == NODE_NUMBER;
    return *root;
}

/*Fluxion: pcas.tests takes 15 at most. compare() isn't one order everywhere, -1, -isqrt(44) and -ypi/2 each
  went before the next and round*/
#define CANONICAL_PASSES_MAX 32

bool simplify_canonical_form(pcas_ast_t *e, unsigned char flags) {
    bool changed = false, intermediate_change;
    unsigned i;
    int passes = 0; /*Fluxion*/

    if(pcas_cancelled)
        return false;

    do {
        intermediate_change = false;

        if((flags & CANONICAL_COMBINE_POWERS) && isoptype(e, OP_MULT)) {
            unsigned j;
            bool inner_intermediate;

            /*Combine a^5b^5 to (ab)^5 */

            do {
                inner_intermediate = false;

                for(i = 0; i < ast_ChildLength(e); i++) {
                    pcas_ast_t *ichild = ast_ChildGet(e, i);

                    if(!isoptype(ichild, OP_POW) && !isoptype(ichild, OP_ROOT))
                        continue;

                    for(j = i + 1; j < ast_ChildLength(e); j++) {
                        pcas_ast_t *jchild = ast_ChildGet(e, j), *merged = NULL;
                        pcas_ast_t *a = ast_ChildGet(ichild, 0), *b = ast_ChildGet(jchild, 0); /*the bases, the indices for roots*/

                        if(optype(jchild) != optype(ichild))
                            continue;

                        /*If the powers are the same*/
                        /*Fluxion: not for sums, (X_1)^2(X+1)^2 is what factor handed back*/
                        /*Fluxion: and sqrt(X)sqrt(Y) only joins with a whole power or one side over 0, both under 0 gives the other sign*/
                        if(isoptype(ichild, OP_POW) && ast_Compare(ast_ChildGet(ichild, 1), ast_ChildGet(jchild, 1))
                           && !isoptype(a, OP_ADD) && !isoptype(b, OP_ADD)
                           && (is_whole_node(ast_ChildGet(ichild, 1))
                               || known_positive(a) || known_positive(b))) {
                            merged = number_power(a, b, ast_ChildGet(ichild, 1));
                            if(merged == NULL)
                                merged = ast_MakeBinary(OP_POW, ast_MakeBinary(OP_MULT, ast_Copy(a), ast_Copy(b)), ast_Copy(ast_ChildGet(ichild, 1)));
                        } else if(isoptype(ichild, OP_ROOT) && ast_Compare(a, b)) {
                            /*Fluxion: number roots only, for the 2sqrt(2) the 2^(3/2) split below hands back a pass late*/
                            pcas_ast_t *k = ast_MakeBinary(OP_DIV, ast_MakeNumber(num_FromInt(1)), ast_Copy(a));

                            merged = number_power(ast_ChildGet(ichild, 1), ast_ChildGet(jchild, 1), k);
                            ast_Cleanup(k);
                        }

                        if(merged != NULL) {
                            ast_ChildAppend(e, merged);

                            ast_Cleanup(ast_ChildRemove(e, ichild));
                            ast_Cleanup(ast_ChildRemove(e, jchild));

                            /*Fluxion: a 2 or 2sqrt(3) coming back flattens in next to the 3 in 3Xcbrt(2)cbrt(4) or 3Xsqrt(2)sqrt(6), eval multiplies them*/
                            simplify(e, isoptype(merged, OP_POW) ? SIMP_COMMUTATIVE : SIMP_COMMUTATIVE | SIMP_EVAL);

                            inner_intermediate = intermediate_change = changed = true;

                            break;
                        }
                    }

                    if(inner_intermediate)
                        break;
                }

            } while(inner_intermediate && !pcas_check());

        }

        if((flags & CANONICAL_POWERS_TO_ROOTS) && isoptype(e, OP_POW)) {
            pcas_ast_t *base, *power;
            mp_small p, q;

            /*Change powers to roots*/

            base = ast_ChildGet(e, 0);
            power = ast_ChildGet(e, 1);

            /*Fluxion: a whole number to a power over 1 comes apart first, 2^(3/2) = 2sqrt(2).
            only here: simplify() puts them back together as like terms.*/
            if(base->type == NODE_NUMBER && mp_rat_is_integer(base->op.num)
               && mp_rat_compare_value(base->op.num, 1, 1) > 0
               && small_fraction(power, &p, &q) && q > 1 && p > q) {
                mp_rat whole = num_FromInt(1), part = num_FromInt(1);

                mp_int_expt(&base->op.num->num, p / q, &whole->num);
                mp_int_expt(&base->op.num->num, p % q, &part->num);
                replace_node(e, ast_MakeBinary(OP_MULT,
                                    ast_MakeNumber(whole),
                                    ast_MakeBinary(OP_POW,
                                        ast_MakeNumber(part),
                                        ast_MakeBinary(OP_DIV, ast_MakeNumber(num_FromInt(1)), ast_MakeNumber(num_FromInt(q))))));
                intermediate_change = changed = true;
            } else if(isoptype(power, OP_DIV) && is_ast_int(ast_ChildGet(power, 0), 1)) {
                replace_node(e, ast_MakeBinary(OP_ROOT,
                                    ast_Copy(ast_ChildGet(power, 1)),
                                    ast_Copy(base)));
                intermediate_change = changed = true;
            }
        }

        if((flags & CANONICAL_RATIONALIZE) && isoptype(e, OP_DIV)) {
            pcas_ast_t *num, *den, *imag;
            bool root0, root1;

            /*Rationalize denominator*/

            num = ast_ChildGet(e, 0);
            den = ast_ChildGet(e, 1);

            /*Fluxion: an i below goes on top as -i, 5pi/(6i) is -5ipi/6. out here, in eval like terms kept *dih*viding by i and lost track of it*/
            imag = NULL;
            if(!has_imaginary_node(num) && den->type == NODE_SYMBOL && den->op.symbol == SYM_IMAG)
                imag = den;
            else if(!has_imaginary_node(num) && isoptype(den, OP_MULT))
                for(imag = ast_ChildGet(den, 0); imag != NULL && !(imag->type == NODE_SYMBOL && imag->op.symbol == SYM_IMAG); imag = imag->next);

            if(imag != NULL) {
                replace_node(num, ast_MakeBinary(OP_MULT, ast_MakeNumber(num_FromInt(-1)),
                                                 ast_MakeBinary(OP_MULT, ast_MakeSymbol(SYM_IMAG), ast_Copy(num))));
                if(imag == den)
                    replace_node(den, ast_MakeNumber(num_FromInt(1)));
                else
                    ast_Cleanup(ast_ChildRemove(den, imag));
                simplify(num, SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_EVAL);
                simplify(den, SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_EVAL);
                cancel_numbers(e);
                intermediate_change = changed = true;
            } else if(isoptype(den, OP_ROOT)) {

                replace_node(num, ast_MakeBinary(OP_MULT,
                                    ast_Copy(num),
                                    root_cofactor(den)
                                ));
                replace_node(den, ast_MakeBinary(OP_MULT,
                                    ast_Copy(den),
                                    root_cofactor(den)
                                ));

                simplify(num, SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_EVAL | SIMP_RATIONAL | SIMP_LIKE_TERMS);
                simplify(den, SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_EVAL | SIMP_RATIONAL | SIMP_LIKE_TERMS);
                cancel_numbers(e); /*Fluxion*/

                intermediate_change = changed = true;
            } else if(isoptype(den, OP_MULT)) {
                pcas_ast_t *child;
                for(child = ast_ChildGet(den, 0); child != NULL; child = child->next) {
                    if(isoptype(child, OP_ROOT)) {
                        replace_node(num, ast_MakeBinary(OP_MULT,
                                            ast_Copy(num),
                                            root_cofactor(child)
                                        ));
                        replace_node(den, ast_MakeBinary(OP_MULT,
                                            ast_Copy(den),
                                            root_cofactor(child)
                                        ));

                        simplify(num, SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_EVAL | SIMP_RATIONAL | SIMP_LIKE_TERMS);
                        simplify(den, SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_EVAL | SIMP_RATIONAL | SIMP_LIKE_TERMS);
                        cancel_numbers(e); /*Fluxion*/

                        intermediate_change = changed = true;
                        break;
                    }
                }
            } else if(isoptype(den, OP_ADD) && ast_ChildLength(den) == 2
                      && surd_term(ast_ChildGet(den, 0), &root0) && surd_term(ast_ChildGet(den, 1), &root1) && (root0 || root1)) {
                /*Fluxion: a+b times a-b is a^2-b^2, a number with square roots of numbers. 1/(sqrt(2)_1) = sqrt(2)+1*/
                pcas_ast_t *conj = ast_Copy(den), *square, *k0, *k1;
                mp_rat g = num_FromInt(1);

                replace_node(ast_ChildGet(conj, 1), ast_MakeBinary(OP_MULT, ast_MakeNumber(num_FromInt(-1)), ast_Copy(ast_ChildGet(conj, 1))));

                /*what both terms share comes off a-b, or 1/(2sqrt(2)+2) is (2sqrt(2)_2)/4*/
                k0 = term_number(ast_ChildGet(den, 0));
                k1 = term_number(ast_ChildGet(den, 1));
                if(k0 != NULL && k1 != NULL && mp_rat_is_integer(k0->op.num) && mp_rat_is_integer(k1->op.num)) {
                    mp_int_gcd(mp_rat_numer_ref(k0->op.num), mp_rat_numer_ref(k1->op.num), mp_rat_numer_ref(g));
                    mp_rat_recip(g, g);
                    conj = ast_MakeBinary(OP_MULT, ast_MakeNumber(g), conj);
                    expand(conj, EXP_ALL);
                    simplify(conj, SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_EVAL | SIMP_RATIONAL | SIMP_LIKE_TERMS);
                } else {
                    num_Cleanup(g);
                }

                square = ast_MakeBinary(OP_MULT, ast_Copy(den), ast_Copy(conj));
                expand(square, EXP_ALL);
                simplify(square, SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_EVAL | SIMP_RATIONAL | SIMP_LIKE_TERMS);

                /*anything else would just come round again next pass*/
                if(square != NULL && square->type == NODE_NUMBER && mp_rat_compare_zero(square->op.num) != 0) {
                    mp_rat_recip(square->op.num, square->op.num);
                    replace_node(e, ast_MakeBinary(OP_MULT, square, ast_MakeBinary(OP_MULT, ast_Copy(num), conj)));
                    simplify(e, SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_EVAL | SIMP_RATIONAL | SIMP_LIKE_TERMS);
                    intermediate_change = changed = true;
                } else {
                    ast_Cleanup(square);
                    ast_Cleanup(conj);
                }
            }
        }

        if(flags & CANONICAL_SORT) {
            /*Order addition and multiplication: C*A*D becomes A*C*D */
            for(i = 0; i < ast_ChildLength(e); i++) {
                pcas_ast_t *child = ast_ChildGet(e, i);

                if(e->type == NODE_OPERATOR && (optype(e) == OP_MULT || optype(e) == OP_ADD)) {
                    unsigned j;

                    for(j = 0; j < i; j++) {
                        int val;
                        pcas_ast_t *child2;

                        child2 = ast_ChildGet(e, j);
                        val = compare(child, child2, optype(e) == OP_ADD);

                        if(val < 0 && compare(child2, child, optype(e) == OP_ADD) > 0) { /*Fluxion: both ways, a pair that each said "me first" swapped forever*/
                            ast_ChildInsert(e, ast_ChildRemoveIndex(e, i), j);
                            intermediate_change = changed = true;
                        }
                    }
                }

            }

        }

        for(i = 0; i < ast_ChildLength(e); i++) {
            pcas_ast_t *child = ast_ChildGet(e, i);

            intermediate_change |= simplify_canonical_form(child, flags);
            changed |= intermediate_change;
        }

        /*Fluxion: the power split above hands back a product, and inside a product
          that nests, 8*(3*sqrt(3)) printed as 83sqrt(3). splice it in, and the
          number goes into the number already there so nothing has to re-collect*/
        if(isoptype(e, OP_MULT)) {
            for(i = 0; i < ast_ChildLength(e); i++) {
                pcas_ast_t *inner = ast_ChildGet(e, i), *first = ast_ChildGet(e, 0);

                if(!isoptype(inner, OP_MULT))
                    continue;
                ast_ChildRemoveIndex(e, i);
                while(ast_ChildLength(inner) > 0) {
                    pcas_ast_t *k = ast_ChildRemoveIndex(inner, 0);

                    if(k->type == NODE_NUMBER && first != NULL && first->type == NODE_NUMBER) {
                        mp_rat_mul(first->op.num, k->op.num, first->op.num);
                        ast_Cleanup(k);
                    } else {
                        ast_ChildInsert(e, k, i++);
                    }
                }
                ast_Cleanup(inner);
                intermediate_change = changed = true;
                break;
            }
        }

    } while(intermediate_change && !pcas_check() && ++passes < CANONICAL_PASSES_MAX); /*Fluxion*/

    return changed;
}

/*
    Combine things like 
    AA to A^(1+1)
    A^2AB to A^(2+1)B
*/ 
bool simplify_like_terms_multiplication(pcas_ast_t *e) {
    pcas_ast_t *child = NULL;
    bool changed = false;

    for(child = ast_ChildGet(e, 0); child != NULL; child = child->next)
            changed |= simplify_like_terms_multiplication(child);

    if(isoptype(e, OP_MULT)) {
        unsigned i, j;

        for(i = 0; i < ast_ChildLength(e); i++) {
            pcas_ast_t *a, *b;

            a = ast_ChildGet(e, i);

            for(j = i + 1; j < ast_ChildLength(e); j++) {
                bool clean_power1 = false, clean_power2 = false;
                pcas_ast_t *base1 = NULL, *base2 = NULL, *power1 = NULL, *power2 = NULL;

                b = ast_ChildGet(e, j);

                if(isoptype(a, OP_POW)) {

                    base1 = ast_ChildGet(a, 0);
                    power1 = ast_ChildGet(a, 1);

                    if(isoptype(b, OP_POW)) {
                        base2 = ast_ChildGet(b, 0);
                        power2 = ast_ChildGet(b, 1);
                    } else {
                        base2 = b;
                        power2 = ast_MakeNumber(num_FromInt(1));
                        clean_power2 = true;
                    }

                } else if(isoptype(b, OP_POW)) {
                    base1 = a;
                    power1 = ast_MakeNumber(num_FromInt(1));
                    base2 = ast_ChildGet(b, 0);
                    power2 = ast_ChildGet(b, 1);

                    clean_power1 = true;
                } else {
                    base1 = a;
                    power1 = ast_MakeNumber(num_FromInt(1));
                    base2 = b;
                    power2 = ast_MakeNumber(num_FromInt(1));

                    clean_power1 = true;
                    clean_power2 = true;
                }

                if(base1 != NULL && ast_Compare(base1, base2)) {
                    pcas_ast_t *append;

                    append = ast_MakeBinary(OP_POW,
                                            ast_Copy(base1),
                                            ast_MakeBinary(OP_ADD,
                                                ast_Copy(power1),
                                                ast_Copy(power2)
                                            )
                                        );

                    ast_ChildAppend(e, append);

                    ast_Cleanup(ast_ChildRemove(e, a));
                    ast_Cleanup(ast_ChildRemove(e, b));

                    if(clean_power1) ast_Cleanup(power1);
                    if(clean_power2) ast_Cleanup(power2);

                    changed = true;

                    i--;

                    break;
                }

                if(clean_power1) ast_Cleanup(power1);
                if(clean_power2) ast_Cleanup(power2);

            }
        }

    }

    return changed;
}

static bool periodic_op(pcas_ast_t *e) {
    return isoptype(e, OP_SIN) || isoptype(e, OP_COS) || isoptype(e, OP_TAN)
        || isoptype(e, OP_SEC) || isoptype(e, OP_CSC) || isoptype(e, OP_COT);
}

/*
    Simplifies sin(9pi/4) to sin(pi/4)

    Works on sin, cos, and tan (Fluxion: and sec, csc, cot)
*/
bool simplify_periodic(pcas_ast_t *e) {
    pcas_ast_t *copy;
    mp_int a, b, c, d;
    bool is_negative, changed = false, half_turn;

    if(!periodic_op(e))
        return false;
    half_turn = isoptype(e, OP_TAN) || isoptype(e, OP_COT); /*period pi, the rest 2pi*/

    copy = ast_MakeBinary(OP_DIV, ast_Copy(ast_ChildGet(e, 0)), ast_MakeSymbol(SYM_PI));
    simplify(copy, SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_RATIONAL | SIMP_EVAL);

    if(pcas_cancelled || copy == NULL) { /*Fluxion: oom, copy or a child of it can be NULL*/
        ast_Cleanup(copy);
        return false;
    }

    if(!isoptype(copy, OP_DIV) && copy->type != NODE_NUMBER) {
        ast_Cleanup(copy);
        return false;
    }

    is_negative = is_negative_for_sure(copy);

    if(copy->type == NODE_NUMBER) {
        a = mp_int_alloc();
        mp_int_init_copy(a, &copy->op.num->num);
        b = mp_int_alloc();
        mp_int_init_value(b, 1);
    } else if(isoptype(copy, OP_DIV)) {
        pcas_ast_t *num, *den;

        num = ast_ChildGet(copy, 0);
        den = ast_ChildGet(copy, 1);

        if(num->type != NODE_NUMBER || den->type != NODE_NUMBER) {
            ast_Cleanup(copy);
            return false;
        }

        a = mp_int_alloc();
        mp_int_init_copy(a, &num->op.num->num);
        b = mp_int_alloc();
        mp_int_init_copy(b, &den->op.num->num);
    } else {
        ast_Cleanup(copy);
        return false;
    }

    mp_int_abs(a, a);
    mp_int_abs(b, b);

    c = mp_int_alloc();
    mp_int_init(c);

    /*c = 2b for sin cos sec csc, c = b for tan and cot*/
    if(half_turn)
        mp_int_copy(b, c);
    else
        mp_int_mul_value(b, 2, c);

    d = mp_int_alloc();
    mp_int_init(d);

    /*d = a/c*/
    mp_int_div(a, c, d, NULL);

    /*if d >= 1 */
    if(mp_int_compare_value(d, 1) >= 0) {
        mp_rat new_num, new_den;

        /*a = a - cd*/
        mp_int_mul(c, d, c);
        mp_int_sub(a, c, a);

        if(is_negative)
            mp_int_neg(a, a);

        new_num = num_FromInt(1);
        new_den = num_FromInt(1);

        mp_int_copy(a, &new_num->num);
        mp_int_copy(b, &new_den->num);

        replace_node(e, ast_MakeUnary(optype(e),
                            ast_MakeBinary(OP_MULT,
                                ast_MakeSymbol(SYM_PI),
                                ast_MakeBinary(OP_DIV,
                                    ast_MakeNumber(new_num),
                                    ast_MakeNumber(new_den)
                                )
                            )));

        changed = true;
    }

    mp_int_free(a);
    mp_int_free(b);
    mp_int_free(c);
    mp_int_free(d);

    ast_Cleanup(copy);

    return changed;
}

/*Fluxion: asin, acos or atan anywhere in e*/
static bool has_inverse_trig(pcas_ast_t *e) {
    pcas_ast_t *c;

    if(isoptype(e, OP_SIN_INV) || isoptype(e, OP_COS_INV) || isoptype(e, OP_TAN_INV))
        return true;
    if(e->type == NODE_OPERATOR)
        for(c = ast_ChildGet(e, 0); c != NULL; c = c->next)
            if(has_inverse_trig(c))
                return true;
    return false;
}

/*Fluxion: asec, acsc and acot of a plain number through acos(1/c), asin(1/c) and pi/2 - atan(c), which the
  inverse table knows. kept only when that comes out exact: asec(2) is pi/3, asec(3) stays asec(3)*/
static bool reciprocal_inverses(pcas_ast_t *e) {
    pcas_ast_t *c, *n, *g;
    bool changed = false;

    if(e->type != NODE_OPERATOR)
        return false;
    for(c = ast_ChildGet(e, 0); c != NULL; c = c->next)
        changed |= reciprocal_inverses(c);
    if((!isoptype(e, OP_SEC_INV) && !isoptype(e, OP_CSC_INV) && !isoptype(e, OP_COT_INV))
       || (n = approx_number(ast_ChildGet(e, 0))) == NULL) /*letters in it*/
        return changed;
    ast_Cleanup(n);

    c = ast_Copy(ast_ChildGet(e, 0));
    if(isoptype(e, OP_COT_INV))
        g = ast_MakeBinary(OP_ADD, ast_MakeBinary(OP_DIV, ast_MakeSymbol(SYM_PI), ast_MakeInt(2)),
            ast_MakeBinary(OP_MULT, ast_MakeInt(-1), ast_MakeUnary(OP_TAN_INV, c)));
    else
        g = ast_MakeUnary(isoptype(e, OP_SEC_INV) ? OP_COS_INV : OP_SIN_INV, ast_MakeBinary(OP_DIV, ast_MakeInt(1), c));
    simplify(g, SIMP_ALL);
    if(g == NULL || pcas_cancelled || has_inverse_trig(g)) {
        ast_Cleanup(g);
        return changed;
    }
    replace_node(e, g);
    return true;
}

/*Fluxion: pi or a number all the way down, the only arguments simplify_periodic can reduce*/
static bool pi_and_numbers(pcas_ast_t *e) {
    pcas_ast_t *c;

    if(e == NULL) /*Fluxion: oom, a child that never got appended*/
        return false;
    if(e->type == NODE_SYMBOL)
        return e->op.symbol == SYM_PI;
    if(e->type == NODE_OPERATOR)
        for(c = ast_ChildGet(e, 0); c != NULL; c = c->next)
            if(!pi_and_numbers(c))
                return false;
    return true;
}

/*Fluxion: simplify_periodic does one node, this runs it on all of them (cos(5pi) inside a sum)*/
static bool periodic_tree(pcas_ast_t *e) {
    pcas_ast_t *c;
    bool changed = false;

    if(e == NULL || e->type != NODE_OPERATOR)
        return false;
    for(c = ast_ChildGet(e, 0); c != NULL; c = c->next)
        changed |= periodic_tree(c);
    if(periodic_op(e) && pi_and_numbers(ast_ChildGet(e, 0)))
        while(!pcas_check() && simplify_periodic(e))
            changed = true;
    return changed;
}

bool simplify_identities(pcas_ast_t *e, unsigned short flags) {
    bool changed = false;

    if(pcas_cancelled) /*Fluxion: stop here, the answer's being thrown away*/
        return false;

    if(flags & SIMP_ID_GENERAL)
        changed |= id_ExecuteTable(e, id_general, ID_NUM_GENERAL, true);

    if(flags & SIMP_ID_COMPLEX)
        while(!pcas_check() && id_ExecuteTable(e, id_complex, ID_NUM_COMPLEX, true))
            changed = true;

    if(flags & SIMP_ID_TRIG) {
        changed |= periodic_tree(e);
        changed |= id_ExecuteTable(e, id_trig_identities, ID_NUM_TRIG_IDENTITIES, true);
    }

    if(flags & SIMP_ID_TRIG_CONSTANTS)
        changed |= id_ExecuteTable(e, id_trig_constants, ID_NUM_TRIG_CONSTANTS, true);

    if(flags & SIMP_ID_TRIG_INV_CONSTANTS) {
        changed |= id_ExecuteTable(e, id_trig_inv_constants, ID_NUM_TRIG_INV_CONSTANTS, true);
        changed |= reciprocal_inverses(e); /*Fluxion*/
    }

    if(flags & SIMP_ID_HYPERBOLIC)
        changed |= id_ExecuteTable(e, id_hyperbolic, ID_NUM_HYPERBOLIC, true);

    return changed;
}

/*
    Executes a collection of simplification functions.

    O(infinity - 1)

    Returns true if ast was changed
*/
/*Fluxion: (a+bi)^n with n whole gets multiplied out before the identities see it. they turn it
into e^(n ln(a+bi)), which came back as 65cos(2atan(4/7))+65isin(2atan(4/7)) for (7+4i)^2, and the
app ran out of memory drawing that. 33+56i*/
static bool expand_complex_powers(pcas_ast_t *e) {
    pcas_ast_t *child, *base, *power;
    bool changed = false;

    if(e == NULL || e->type != NODE_OPERATOR)
        return false;
    for(child = ast_ChildGet(e, 0); child != NULL; child = child->next)
        changed |= expand_complex_powers(child);

    if(!isoptype(e, OP_POW))
        return changed;
    base = ast_ChildGet(e, 0);
    power = ast_ChildGet(e, 1);
    if(isoptype(base, OP_ADD) && has_imaginary_node(base) && power->type == NODE_NUMBER && mp_rat_is_integer(power->op.num)
        && mp_rat_compare_value(power->op.num, 2, 1) >= 0 && mp_rat_compare_value(power->op.num, EXPAND_MAX_POWER, 1) <= 0) {
        expand(e, EXP_ALL);
        changed = true;
    }
    return changed;
}

/*Fluxion: an odd root of something negative stays real, like the nspire in real mode. (-8)^(1/3)
is -2, the (-A)^(X/Y) identity made it 1+isqrt(3), and solve(x^3+1=y,x) came back complex too*/
static bool real_odd_roots(pcas_ast_t *e) {
    pcas_ast_t *child, *base, *power, *pos;
    mp_small p, q;
    bool changed = false;

    if(pcas_cancelled || e == NULL || e->type != NODE_OPERATOR) /*oom can leave a power with no exponent*/
        return false;
    for(child = ast_ChildGet(e, 0); child != NULL; child = child->next)
        changed |= real_odd_roots(child);

    if(!isoptype(e, OP_POW))
        return changed;
    base = ast_ChildGet(e, 0);
    power = ast_ChildGet(e, 1);
    if(!small_fraction(power, &p, &q) || q < 3 || q % 2 == 0 || !is_negative_for_sure(base))
        return changed;

    pos = ast_Copy(base);
    absolute_val(pos);
    pos = ast_MakeBinary(OP_POW, pos, ast_Copy(power));
    if(p % 2 != 0)
        pos = ast_MakeBinary(OP_MULT, ast_MakeNumber(num_FromInt(-1)), pos);
    replace_node(e, pos);
    return true;
}

bool simplify(pcas_ast_t *e, unsigned short flags) {
    bool did_change = false, intermediate_change, expand_changed;

    /*Fluxion: every simplify inside a cancelled one returns at once*/
    if(pcas_cancelled)
        return false;

    do {
        intermediate_change = false;
        expand_changed = false;

        if(flags & SIMP_NORMALIZE)
            while(!pcas_check() && simplify_normalize(e))    intermediate_change = did_change = true;
        if(flags & SIMP_COMMUTATIVE)
            while(!pcas_check() && simplify_commutative(e))  intermediate_change = did_change = true;
        if(flags & SIMP_RATIONAL)
            while(!pcas_check() && simplify_rational(e))     intermediate_change = did_change = true;
        if(flags & SIMP_EVAL)
            while(!pcas_check() && eval(e, EVAL_EASY | EVAL_POWERS_FIT))       intermediate_change = did_change = true; /*Fluxion: FIT, (3/100)^2 left 100^2 on the bottom*/
        
        if(flags & SIMP_DERIV) {
            intermediate_change |= eval_derivative_nodes(e);
            intermediate_change |= eval_limit_nodes(e); /*Fluxion*/
            intermediate_change |= eval_integral_nodes(e); /*Fluxion*/
            did_change |= intermediate_change;
        }

        /*Simplify identities. First factor the expression and simplify identities.
        Then expand the expression and simplify identities that we missed. Only factor and expand if 
        at least one id flag is set. The expression remains in the expanded state at the end of simplify().*/

        if(flags & SIMP_ID_ALL) {
            bool pre = real_odd_roots(e); /*Fluxion: both before the identities get to it*/

            if(expand_complex_powers(e) || pre) {
                simplify(e, SIMP_COMMUTATIVE | SIMP_EVAL | SIMP_LIKE_TERMS);
                intermediate_change = did_change = true;
            }
            factor(e, FAC_SIMPLE_ADDITION_EVALUATEABLE | FAC_SIMPLE_ADDITION_NONEVALUATEABLE);
            intermediate_change |= real_odd_roots(e); /*Fluxion: factor just pulled the -1 back out of y-1*/

            intermediate_change |= simplify_identities(e, flags);
            did_change |= intermediate_change;
        }

        if(flags & SIMP_ID_ALL) {
            expand_changed = expand(e, EXP_DISTRIB_NUMBERS | EXP_DISTRIB_MULTIPLICATION | EXP_DISTRIB_DIVISION | EXP_DISTRIB_POWERS);

            if(expand_changed) {

                /*Expand leaves constants unevaluated and commutative operators with 1 or 0 children.*/
                simplify(e, SIMP_COMMUTATIVE | SIMP_EVAL | SIMP_LIKE_TERMS);

                intermediate_change |= simplify_identities(e, flags);
                did_change |= intermediate_change;

                /*Fix the expansion of division here*/
                simplify(e, SIMP_COMMUTATIVE | SIMP_RATIONAL | SIMP_EVAL | SIMP_LIKE_TERMS);
            }
        }

        /*Simplify like terms*/
        if(flags & SIMP_LIKE_TERMS) {
            /*First fix 2A+B _ (A+B) to 2A+B_A_B */
            while(!pcas_check() && expand(e, EXP_DISTRIB_NUMBERS | EXP_DISTRIB_POWERS)) intermediate_change = did_change = true;
            /*Then fix 2A+B_A_B to A(2+-1)+B(1+-1)*/
            while(!pcas_check() && factor(e, FAC_SIMPLE_ADDITION_EVALUATEABLE))               intermediate_change = did_change = true;
            /*This would fix AA to A^2 if it exists*/
            while(!pcas_check() && simplify_like_terms_multiplication(e))                     intermediate_change = did_change = true;
        }

    } while(intermediate_change && !pcas_check());

    return did_change;
}