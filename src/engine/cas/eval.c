#include "cas.h"

static bool eval_commutative(pcas_ast_t *e, unsigned short flags) {
    /*How many numbers were accumulated. If <= 1, nothing changed*/
    unsigned num_changed = false;
    unsigned i;
    mp_rat accumulator;

    if(pcas_cancelled) /*Fluxion: stop here, the answer's being thrown away*/
        return false;

    if(!(flags & EVAL_COMMUTATIVE))
        return false;

    /*Fluxion: a sum whose only constant is one a/b is left alone. folding it made
    it the number a/b and returned false, normalize made that a/b again and returned
    true, forever and ever: X/2_1/2 hung simplify without SIMP_LIKE_TERMS*/
    /*:P*/
    if(optype(e) == OP_ADD) {
        pcas_ast_t *child, *only = NULL;
        unsigned constants = 0;

        for(child = opbase(e); child != NULL; child = child->next) {
            if(child->type == NODE_NUMBER
               || (isoptype(child, OP_DIV) && ast_ChildGet(child, 0)->type == NODE_NUMBER
                   && ast_ChildGet(child, 1)->type == NODE_NUMBER)) {
                constants++;
                only = child;
            }
        }
        if(constants == 1 && only->type == NODE_OPERATOR)
            return false;
    }

    accumulator = num_FromInt(optype(e) == OP_MULT ? 1 : 0);

    if(accumulator == NULL) /*Fluxion: oom*/
        return false;

    for(i = 0; i < ast_ChildLength(e); i++) {
        pcas_ast_t *child = ast_ChildGet(e, i);

        if(child->type == NODE_NUMBER) {
            if(optype(e) == OP_MULT)
                mp_rat_mul(accumulator, child->op.num, accumulator);
            else
                mp_rat_add(accumulator, child->op.num, accumulator);

            ast_Cleanup(ast_ChildRemoveIndex(e, i));
            i--;
            num_changed++;
        } else if(isoptype(e, OP_ADD) && isoptype(child, OP_DIV)) {

            pcas_ast_t *c_num, *c_den;

            c_num = ast_ChildGet(child, 0);
            c_den = ast_ChildGet(child, 1);

            if(c_num->type == NODE_NUMBER && c_den->type == NODE_NUMBER) {
                mp_int a, b, c, d;
                mp_int first, second, num, den;

                /*a/b + c/d = (ad + bc)/(bd)*/

                a = &accumulator->num;
                b = &accumulator->den;
                c = &c_num->op.num->num;
                d = &c_den->op.num->num;

                first = mp_int_alloc();
                mp_int_init(first);

                mp_int_mul(a, d, first);

                second = mp_int_alloc();
                mp_int_init(second);

                mp_int_mul(b, c, second);

                num = mp_int_alloc();
                mp_int_add(first, second, num);

                den = mp_int_alloc();
                mp_int_init(den);

                mp_int_mul(b, d, den);

                mp_int_copy(num, a);
                mp_int_copy(den, b);

                mp_int_free(first);
                mp_int_free(second);
                mp_int_free(num);
                mp_int_free(den);

                ast_Cleanup(ast_ChildRemoveIndex(e, i));

                i--;
                num_changed++;
            }
        }
    }

    /*If all children nodes were numbers or if multiplied by -*/
    if(ast_ChildLength(e) == 0
        || (optype(e) == OP_MULT && mp_rat_compare_zero(accumulator) == 0)) {
        replace_node(e, ast_MakeNumber(accumulator));
        return true;
    }
    /*Do not append accumulator if multiplying by 1 or adding 0*/
    else if(!((optype(e) == OP_ADD && mp_rat_compare_zero(accumulator) == 0)
        || (optype(e) == OP_MULT && mp_rat_compare_value(accumulator, 1, 1) == 0))) {
        ast_ChildInsert(e, ast_MakeNumber(accumulator), 0);
    }
    else
        num_Cleanup(accumulator);

    return num_changed > 1;
}

static bool eval_div(pcas_ast_t *e, unsigned short flags);

static bool eval_div_mult(pcas_ast_t *num, pcas_ast_t *den, unsigned short flags) {

    pcas_ast_t *temp_num, *temp_den, *temp_div;

    bool changed = false;
    bool div_changed = false;

    if(pcas_cancelled) /*Fluxion: stop here, the answer's being thrown away*/
        return false;

    if(isoptype(num, OP_MULT)) {
        unsigned i = 0;

        for(i = 0; i < ast_ChildLength(num); i++) {

            temp_num = ast_ChildGet(num, i);

            if(isoptype(den, OP_MULT)) {
                unsigned j;

                for(j = 0; j < ast_ChildLength(den); j++) {
                    temp_den = ast_ChildGet(den, j);

                    temp_div = ast_MakeBinary(OP_DIV, ast_Copy(temp_num), ast_Copy(temp_den));

                    div_changed = eval_div(temp_div, flags);

                    if(div_changed) {
                        ast_Cleanup(ast_ChildRemoveIndex(num, i));
                        ast_Cleanup(ast_ChildRemoveIndex(den, j));

                        ast_ChildAppend(num, temp_div);

                        changed = true;

                        j--;
                        break;
                    } else {
                        ast_Cleanup(temp_div);
                    }

                }
            } else {
                temp_den = den;

                temp_div = ast_MakeBinary(OP_DIV, ast_Copy(temp_num), ast_Copy(temp_den));

                div_changed = eval_div(temp_div, flags);

                if(div_changed) {
                    ast_Cleanup(ast_ChildRemoveIndex(num, i));
                    replace_node(den, ast_MakeNumber(num_FromInt(1)));

                    ast_ChildAppend(num, temp_div);

                    changed = true;
                } else {
                    ast_Cleanup(temp_div);
                }

            }
        }

    } else if(isoptype(den, OP_MULT)) {
        unsigned j;

        temp_num = num;

        for(j = 0; j < ast_ChildLength(den); j++) {
            temp_den = ast_ChildGet(den, j);

            temp_div = ast_MakeBinary(OP_DIV, ast_Copy(temp_num), ast_Copy(temp_den));

            div_changed = eval_div(temp_div, flags);

            if(div_changed) {

                replace_node(num, temp_div);
                ast_Cleanup(ast_ChildRemoveIndex(den, j));

                changed = true;

                j--;
                break;
            } else {
                ast_Cleanup(temp_div);
            }
        }

    }

    return changed;
}

static bool eval_div(pcas_ast_t *e, unsigned short flags) {
    pcas_ast_t *num, *den;

    bool changed = false;

    if(pcas_cancelled) /*Fluxion: stop here, the answer's being thrown away*/
        return false;

    if(e == NULL) /*Fluxion: oom*/
        return false;

    num = ast_ChildGet(e, 0);
    den = ast_ChildGet(e, 1);

    if(num == NULL || den == NULL) /*Fluxion: oom*/
        return false;

    if(flags & EVAL_BASIC_IDENTITIES) {

        if(is_negative_for_sure(num) && is_negative_for_sure(den)) {
            pcas_ast_t *new_div;

            absolute_val(num);
            absolute_val(den);

            new_div = ast_MakeBinary(OP_DIV, ast_Copy(num), ast_Copy(den));

            eval_div(new_div, flags);
            replace_node(e, new_div);

            return true;
        }

        /*If numerator and denominator are equal, replace node with "1"*/
        if(ast_Compare(num, den)) {
            replace_node(e, ast_MakeNumber(num_FromInt(1)));
            return true;
        }

        if(num->type == NODE_NUMBER && mp_rat_compare_zero(num->op.num) == 0) {
            replace_node(e, ast_MakeNumber(num_FromInt(0)));
            return true;
        }

        if(den->type == NODE_NUMBER && mp_rat_compare_value(den->op.num, 1, 1) == 0) {
            replace_node(e, num);
            return true;
        }
    }

    if(!(flags & EVAL_DIVISION))
        return false;

    if(isoptype(num, OP_POW)) {
        pcas_ast_t *base1, *power1;

        base1 = ast_ChildGet(num, 0);
        power1 = ast_ChildGet(num, 1);

        if(isoptype(den, OP_POW)) {
            pcas_ast_t *base2, *power2;

            base2 = ast_ChildGet(den, 0);
            power2 = ast_ChildGet(den, 1);

            if(ast_Compare(base1, base2)) {
                /*Subtract powers*/
                replace_node(power1, ast_MakeBinary(OP_ADD,
                                                    ast_Copy(power1),
                                                    ast_MakeBinary(OP_MULT,
                                                                    ast_MakeNumber(num_FromInt(-1)),
                                                                    ast_Copy(power2))));
                replace_node(den, ast_MakeNumber(num_FromInt(1)));
                return true;
            }

        } else if(ast_Compare(base1, den)) {
            /*Subtract powers*/
            replace_node(power1, ast_MakeBinary(OP_ADD,
                                                ast_MakeNumber(num_FromInt(-1)),
                                                ast_Copy(power1)));
            replace_node(den, ast_MakeNumber(num_FromInt(1)));

            return true;
        }
    } else if(isoptype(den, OP_POW) && ast_Compare(num, ast_ChildGet(den, 0))) {
        /*Subtract powers*/
        pcas_ast_t *power2 = ast_ChildGet(den, 1);

        replace_node(power2, ast_MakeBinary(OP_ADD,
                                                ast_MakeNumber(num_FromInt(-1)),
                                                ast_Copy(power2)));
        replace_node(num, ast_MakeNumber(num_FromInt(1)));

        return true;
    }

    if(eval_div_mult(num, den, flags))
        return true;

    if(num->type != NODE_NUMBER || den->type != NODE_NUMBER)
        return false;

    /*Simplify fractions using GCD*/
    if(mp_rat_is_integer(num->op.num) && mp_rat_is_integer(den->op.num)) {
        mpz_t gcd;
        mp_int a = &num->op.num->num, b = &den->op.num->num;

        /*TODO: check for division by zero?*/

        mp_int_init(&gcd);
        mp_int_gcd(a, b, &gcd);

        if(mp_int_compare_value(&gcd, 1) != 0) {
            mp_int_div(a, &gcd, a, NULL);
            mp_int_div(b, &gcd, b, NULL);
            changed = true;
        }

        mp_int_clear(&gcd);
    }

    return changed; 
}

#define power_in_small_range(a, b) (mp_rat_compare_value((a)->op.num, -1, 1) == 0 || (mp_rat_compare_value((a)->op.num, 10, 1) <= 0 && mp_rat_compare_value((b)->op.num, 10, 1) <= 0))

/*Fluxion: trial division for root factors stops here, so a k-th power of a bigger
prime stays inside the root (still right, just not as simple)*/
#define ROOT_MAX_FACTOR 1000

/*Fluxion: n^(1/k) with n = s^k * r, s > 1, becomes s * r^(1/k): sqrt(3780) = 6sqrt(105).
exponent is the 1/k node. returns false if there's no k-th power to take out.*/
/*rawr~~*/
static bool eval_root_factors(pcas_ast_t *e, mp_int n, mp_small k, pcas_ast_t *exponent) {
    mp_int r = mp_int_alloc(), s = mp_int_alloc(), q = mp_int_alloc();
    mp_small p, rem;
    bool changed = false;
    int count, i;

    if(!pcas_cancelled && r != NULL && s != NULL && q != NULL) { /*Fluxion: skip it all once cancelled, the answer's being thrown away*/
        mp_int_copy(n, r);
        mp_int_set_value(s, 1);

        for(p = 2; p <= ROOT_MAX_FACTOR && mp_int_compare_value(r, 1) > 0; p += p == 2 ? 1 : 2) {
            for(count = 0; mp_int_div_value(r, p, q, &rem) == MP_OK && rem == 0; count++)
                mp_int_copy(q, r);

            /*p^count: whole k-th powers out, the rest back in*/
            for(i = 0; i < count / k; i++)
                mp_int_mul_value(s, p, s);
            for(i = 0; i < count % k; i++)
                mp_int_mul_value(r, p, r);
        }

        if(mp_int_compare_value(s, 1) > 0) {
            mp_rat outside = num_FromInt(1), inside = num_FromInt(1);

            mp_int_copy(s, &outside->num);
            mp_int_copy(r, &inside->num);
            replace_node(e, ast_MakeBinary(OP_MULT,
                                ast_MakeNumber(outside),
                                ast_MakeBinary(OP_POW, ast_MakeNumber(inside), ast_Copy(exponent))
                            ));
            changed = true;
        }
    }

    if(r != NULL) mp_int_free(r);
    if(s != NULL) mp_int_free(s);
    if(q != NULL) mp_int_free(q);
    return changed;
}

static bool eval_pow(pcas_ast_t *e, unsigned short flags) {
    /*a^b*/
    pcas_ast_t *a, *b;
    bool changed = false;

    if(pcas_cancelled) /*Fluxion: stop here, the answer's being thrown away*/
        return false;

    a = ast_ChildGet(e, 0);
    b = ast_ChildGet(e, 1);

    if (!a || !b) {
        return false;
    }

    if(flags & EVAL_BASIC_IDENTITIES) {
        /*If a == 0*/
        if(is_ast_int(a, 0)) {
            /*If b != 0, e = 0*/
            if(!is_ast_int(b, 0)) {
                replace_node(e, ast_MakeNumber(num_FromInt(0)));
                return true;
            }
        }

        /*b == 0*/
        else if(is_ast_int(b, 0)) {
            /*If a != 0, e = 1*/
            if(!is_ast_int(a, 0)) {
                replace_node(e, ast_MakeNumber(num_FromInt(1)));
                return true;
            }
        }

        /*a == 1*/
        else if(is_ast_int(a, 1)) {
            replace_node(e, a);
            return true;
        }

        /*b == 1*/
        else if(is_ast_int(b, 1)) {
            replace_node(e, a);
            return true;
        }

        /*(A^B)^C = A^(BC)*/
        if(isoptype(a, OP_POW)) {
            replace_node(e, ast_MakeBinary(OP_POW,
                                            ast_Copy(ast_ChildGet(a, 0)),
                                            ast_MakeBinary(OP_MULT,
                                                ast_Copy(ast_ChildGet(a, 1)),
                                                ast_Copy(b)
                                                )
                                            ));
            return true;
        }

        /*A^(-B) = 1/(A^B) */
        if(is_negative_for_sure(b)) {
            absolute_val(b);
            replace_node(e, ast_MakeBinary(OP_DIV,
                                ast_MakeNumber(num_FromInt(1)),
                                ast_Copy(e)
                            ));
            return true;
        }
    }

    /*Evaluate a^b if a and b are integers*/
    if(a->type == NODE_NUMBER && b->type == NODE_NUMBER) {

        if(mp_rat_is_integer(a->op.num) && mp_rat_is_integer(b->op.num)
            && mp_rat_compare_zero(b->op.num) > 0) {

            if(flags & EVAL_POWERS_FULL ||
                (flags & EVAL_POWERS_SMALL && power_in_small_range(a, b))) {
                mp_rat result;

                result = num_FromInt(1);

                mp_int_expt_full(&a->op.num->num, &b->op.num->num, &result->num);

                replace_node(e, ast_MakeNumber(result));

                return true;
            }

        }

    }
    /*Evaluate i^b if b is an integer*/
    else if((flags & EVAL_POWERS_SMALL || flags & EVAL_POWERS_FULL) && a->type == NODE_SYMBOL && a->op.symbol == SYM_IMAG && b->type == NODE_NUMBER) {
        if(mp_rat_is_integer(b->op.num) && mp_rat_compare_zero(b->op.num) > 0) {
            mp_small remainder;

            mp_int_mod_value(&b->op.num->num, 4, &remainder);

            if(remainder == 0) {
                replace_node(e, ast_MakeNumber(num_FromInt(1)));
            } else if(remainder == 1) {
                replace_node(e, ast_MakeSymbol(SYM_IMAG));
            } else if(remainder == 2) {
                replace_node(e, ast_MakeNumber(num_FromInt(-1)));                
            } else if(remainder == 3) {
                replace_node(e, ast_MakeBinary(OP_MULT, ast_MakeNumber(num_FromInt(-1)), ast_MakeSymbol(SYM_IMAG)));
            }

            return true;
        }
    }

    /*Do roots*/
    if((flags & EVAL_POWERS_SMALL || flags & EVAL_POWERS_FULL) && a->type == NODE_NUMBER && b->type == NODE_OPERATOR && optype(b) == OP_DIV && is_ast_int(ast_ChildGet(b, 0), 1) && ast_ChildGet(b, 1)->type == NODE_NUMBER) {
        /*a root of b*/
        pcas_ast_t *temp = b;
        b = a;
        a = ast_ChildGet(temp, 1);

        if(mp_rat_is_integer(a->op.num) && mp_rat_is_integer(b->op.num) && mp_rat_compare_zero(b->op.num) > 0) {
            mp_int answer, check;
            mp_small small;
            /*Set answer = small root of a*/
            answer = mp_int_alloc();
            mp_int_init(answer);

            mp_int_to_int(&a->op.num->num, &small);

            mp_int_root(&b->op.num->num, small, answer);

            /*Check if answer ^ small == b*/
            check = mp_int_alloc();
            mp_int_init(check);

            mp_int_expt(answer, small, check);

            if(mp_int_compare(check, &b->op.num->num) == 0) {
                /*Then this was successful*/
                mp_rat ret = num_FromInt(1);
                mp_int_copy(answer, &ret->num);

                replace_node(e, ast_MakeNumber(ret));

                changed = true;
            } else {
                /*Fluxion: not a perfect power, but maybe part of it is (trial division only to 1000, too slow otherwise)*/
                changed = eval_root_factors(e, &b->op.num->num, small, temp);
            }

            mp_int_free(check);
            mp_int_free(answer);

        }
    }
    /*Fluxion: p/q over 1 goes through the root too, 27^(2/3) = 3^2 and 12^(3/2) = 2^3*3^(3/2).
    lowest terms only. the r^(p/q) left over has no q-th powers in it so it stops there*/
    else if((flags & EVAL_POWERS_SMALL || flags & EVAL_POWERS_FULL) && a->type == NODE_NUMBER && isoptype(b, OP_DIV)
            && ast_ChildGet(b, 0)->type == NODE_NUMBER && ast_ChildGet(b, 1)->type == NODE_NUMBER) {
        pcas_ast_t *top = ast_ChildGet(b, 0), *bottom = ast_ChildGet(b, 1), *tmp;
        mp_small p, q, x, y, t;
        mp_int answer, check;

        if(!mp_rat_is_integer(a->op.num) || mp_rat_compare_zero(a->op.num) <= 0
           || !mp_rat_is_integer(top->op.num) || !mp_rat_is_integer(bottom->op.num)
           || mp_int_to_int(&top->op.num->num, &p) != MP_OK || mp_int_to_int(&bottom->op.num->num, &q) != MP_OK
           || p <= 1 || q <= 1)
            return false;

        for(x = p, y = q; y != 0; x = t) {
            t = y;
            y = x % y;
        }
        if(x != 1)
            return false;

        answer = mp_int_alloc();
        check = mp_int_alloc();

        mp_int_root(&a->op.num->num, q, answer);
        mp_int_expt(answer, q, check);

        if(mp_int_compare(check, &a->op.num->num) == 0) {
            /*Fluxion: root^p as a power, the integer case above decides if it's small enough to work out*/
            mp_rat ret = num_FromInt(1);

            mp_int_copy(answer, &ret->num);
            replace_node(e, ast_MakeBinary(OP_POW, ast_MakeNumber(ret), ast_MakeNumber(num_FromInt(p))));
            changed = true;
        } else {
            tmp = ast_MakeBinary(OP_POW, ast_Copy(a),
                        ast_MakeBinary(OP_DIV, ast_MakeNumber(num_FromInt(1)), ast_MakeNumber(num_FromInt(q))));

            /*Fluxion: tmp is s * r^(1/q) now, raise both halves to p*/
            if(tmp != NULL && eval_root_factors(tmp, &a->op.num->num, q, ast_ChildGet(tmp, 1))) {
                pcas_ast_t *r = ast_ChildGet(ast_ChildGet(tmp, 1), 0), *rest;

                /*Fluxion: r^(p/q) with p > q comes apart here and not in canonical form,
                that one leaves 8*(3*sqrt(3)) nested and it prints as 83sqrt(3)*/
                if(p > q)
                    rest = ast_MakeBinary(OP_MULT,
                                ast_MakeBinary(OP_POW, ast_Copy(r), ast_MakeNumber(num_FromInt(p / q))),
                                ast_MakeBinary(OP_POW, ast_Copy(r),
                                    ast_MakeBinary(OP_DIV, ast_MakeNumber(num_FromInt(p % q)), ast_MakeNumber(num_FromInt(q)))));
                else
                    rest = ast_MakeBinary(OP_POW, ast_Copy(r), ast_Copy(b));

                replace_node(e, ast_MakeBinary(OP_MULT,
                                    ast_MakeBinary(OP_POW, ast_Copy(ast_ChildGet(tmp, 0)), ast_MakeNumber(num_FromInt(p))),
                                    rest
                                ));
                changed = true;
            }

            ast_Cleanup(tmp);
        }

        mp_int_free(check);
        mp_int_free(answer);
    }

    return changed;
}

static bool eval_int(pcas_ast_t *e, unsigned short flags) {
    bool changed = false;
    mp_rat res;
    pcas_ast_t *a;
    mp_int remainder;

    if(!(flags & EVAL_INT))
        return false;

    a = ast_ChildGet(e, 0);

    remainder = mp_int_alloc();
    mp_int_init(remainder);

    if(a->type == NODE_NUMBER) {

        res = num_FromInt(1);

        mp_int_div(&a->op.num->num, &a->op.num->den, &res->num, remainder);

        if(mp_int_compare_zero(remainder) != 0 && mp_int_compare_zero(&res->num) < 0)
            mp_int_sub_value(&res->num, 1, &res->num);

        replace_node(e, ast_MakeNumber(res));

        changed = true;
    } else if(isoptype(a, OP_DIV)) {
        pcas_ast_t *num, *den;

        num = ast_ChildGet(a, 0);
        den = ast_ChildGet(a, 1);

        if(num->type == NODE_NUMBER && den->type == NODE_NUMBER) {
            res = num_FromInt(1);

            /*That is a lot of num lol*/
            mp_int_div(&num->op.num->num, &den->op.num->num, &res->num, remainder);

            if(mp_int_compare_zero(remainder) != 0 && mp_int_compare_zero(&res->num) < 0)
                mp_int_sub_value(&res->num, 1, &res->num);

            replace_node(e, ast_MakeNumber(res));

            changed = true;
        }
    }

    mp_int_free(remainder);

    return changed;
}

static bool eval_abs(pcas_ast_t *e, unsigned short flags) {
    bool changed = false;
    pcas_ast_t *a = ast_ChildGet(e, 0);

    if(!(flags & EVAL_ABS))
        return false;

    changed = absolute_val(a) || a->type == NODE_NUMBER;

    if(changed)
        replace_node(e, a);

    return changed;
}

static bool eval_log(pcas_ast_t *e, unsigned short flags) {
    pcas_ast_t *base, *val;

    if(pcas_cancelled) /*Fluxion: stop here, the answer's being thrown away*/
        return false;

    base = ast_ChildGet(e, 0);
    val = ast_ChildGet(e, 1);

    if(base == NULL || val == NULL) /*Fluxion: oom*/
        return false;

    if(flags & EVAL_BASIC_IDENTITIES) {
        /*log(1) = 0*/
        if(is_ast_int(val, 1)) {
            replace_node(e, ast_MakeNumber(num_FromInt(0)));
            return true;
        }

        /*This is hardcoded here because we want this to happen before
        powers are evaluated*/
        /*log(A^B)=Blog(A)*/

        if(isoptype(val, OP_POW)) {
            pcas_ast_t *power_base, *power_exponent;

            power_base = ast_ChildGet(val, 0);
            power_exponent = ast_ChildGet(val, 1);

            replace_node(e, ast_MakeBinary(OP_MULT,
                                ast_Copy(power_exponent),
                                ast_MakeBinary(OP_LOG,
                                    ast_Copy(base),
                                    ast_Copy(power_base)
                                )
                            ));
            return true;

        }

        /*Fluxion: log(1000) = 3, logb(8,2) = 3, exact when it works out. walks
        base^k up to val so it's tiny k only, which is all anyone types*/
        if(base->type == NODE_NUMBER && val->type == NODE_NUMBER
           && mp_rat_is_integer(base->op.num) && mp_rat_is_integer(val->op.num)
           && mp_rat_compare_value(base->op.num, 1, 1) > 0 && mp_rat_compare_value(val->op.num, 1, 1) > 0) {
            mp_int pow = mp_int_alloc();
            int k;

            mp_int_copy(mp_rat_numer_ref(base->op.num), pow);
            for(k = 1; k < 64 && mp_int_compare(pow, mp_rat_numer_ref(val->op.num)) < 0; k++)
                mp_int_mul(pow, mp_rat_numer_ref(base->op.num), pow);
            if(mp_int_compare(pow, mp_rat_numer_ref(val->op.num)) == 0) {
                mp_int_free(pow);
                replace_node(e, ast_MakeNumber(num_FromInt(k)));
                return true;
            }
            mp_int_free(pow);
        }
    }

    /*Todo evaluate log constants*/

    return false;
}

#define factorial_in_small_range(a) (mp_rat_compare_value((a)->op.num, 10, 1) <= 0)

static bool eval_factorial(pcas_ast_t *e, unsigned short flags) {

    pcas_ast_t *a = ast_ChildGet(e, 0);

    if(pcas_cancelled) /*Fluxion: stop here, the answer's being thrown away*/
        return false;

    if(a->type == NODE_NUMBER) {

        if(flags & EVAL_BASIC_IDENTITIES) {
            /*0! = 1*/
            if(mp_rat_compare_zero(a->op.num) == 0) {
                replace_node(e, ast_MakeNumber(num_FromInt(1)));
                return true;
            }
        }

        if(mp_rat_is_integer(a->op.num) && mp_rat_compare_zero(a->op.num) > 0) {
            mp_rat accumulator;
            mp_int i;

            if(flags & EVAL_FACTORIAL_FULL
            || (flags & EVAL_FACTORIAL_SMALL && factorial_in_small_range(a))) {
                accumulator = num_FromInt(1);
                i = mp_int_alloc();
                mp_int_init_copy(i, &a->op.num->num);

                while(mp_int_compare_zero(i) != 0 && !pcas_check()) {
                    mp_rat_mul_int(accumulator, i, accumulator);
                    mp_int_sub_value(i, 1, i);
                }

                replace_node(e, ast_MakeNumber(accumulator));

                mp_int_free(i);

                return true;
            }

        }

    }

    return false;
}

/*Simplifies expressions like 5 + 5 to 10*/
bool eval(pcas_ast_t *e, unsigned short flags) {

    bool changed = false;
    pcas_ast_t *current;

    if(pcas_cancelled) /*Fluxion: stop here, the answer's being thrown away*/
        return false;

    /*Get a head start to evaluate the identity ln(A^B)=Bln(A)
    before child power node is evaluated*/
    if(isoptype(e, OP_LOG))
        changed |= eval_log(e, flags);

    if(e->type != NODE_OPERATOR)
        return false;

    for(current = e->op.operator.base; current != NULL; current = current->next) {
        changed |= eval(current, flags);
    }

    if(is_op_commutative(optype(e))) {
        changed |= eval_commutative(e, flags);
    } else {

        /*Simplify pow, root, log, factorial*/
        switch(optype(e)) {
            case OP_DIV: changed |= eval_div(e, flags); break;
            case OP_POW: changed |= eval_pow(e, flags); break;
            case OP_INT: changed |= eval_int(e, flags); break;
            case OP_ABS: changed |= eval_abs(e, flags); break;
            case OP_FACTORIAL: changed |= eval_factorial(e, flags); break;
            default:
                break;
        }
    }

    return changed;
}

bool substitute(pcas_ast_t *e, pcas_ast_t *from, pcas_ast_t *to) {

    if(ast_Compare(e, from)) {
        replace_node(e, ast_Copy(to));
        return true;
    }

    if(e->type == NODE_OPERATOR) {
        pcas_ast_t *child;
        bool changed = false;

        for(child = ast_ChildGet(e, 0); child != NULL; child = child->next)
            changed |= substitute(child, from, to);

        return changed;
    }

    return false;
}