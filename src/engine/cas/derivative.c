#include "cas.h"

/*Sum rule, constant rule, product rule, and power rule are hardcoded for speed or because of limitations in identity searching*/
pcas_id_t id_derivative[ID_NUM_DERIV] = {
    {"deriv(X,X,T", "1"},

    {"deriv(A/B,X,T", "(deriv(A,X,T)B_deriv(B,X,T)A)/B^2"},

    {"deriv(e^A,X,T", "deriv(A,X,T)e^A"},
    {"deriv(B^A,X,T", "deriv(e^(Aln(B,X,T"},

    {"deriv(ln(A,X,T", "deriv(A,X,T)/A"},
    {"deriv(logb(A,B),X,T", "deriv(ln(A)/ln(B,X,T"},

    {"deriv(sin(A,X,T", "cos(A)deriv(A,X,T"},
    {"deriv(cos(A,X,T", "-sin(A)deriv(A,X,T"},
    {"deriv(tan(A,X,T", "deriv(A,X,T)/cos(A)^2"},
    {"deriv(cot(A,X,T", "-deriv(A,X,T)csc(A)^2"},
    {"deriv(sec(A,X,T", "deriv(A,X,T)sec(A)tan(A"},
    {"deriv(csc(A,X,T", "-deriv(A,X,T)csc(A)cot(A"},

    {"deriv(asin(A,X,T", "deriv(A,X,T)/sqrt(1_A^2"},
    {"deriv(acos(A,X,T", "-deriv(A,X,T)/sqrt(1_A^2"},
    {"deriv(atan(A,X,T", "deriv(A,X,T)/(1+A^2"},
    {"deriv(acot(A,X,T", "-deriv(A,X,T)/(1+A^2"}, /*Fluxion*/
    {"deriv(asec(A,X,T", "deriv(A,X,T)/(abs(A)sqrt(A^2_1"}, /*Fluxion*/
    {"deriv(acsc(A,X,T", "-deriv(A,X,T)/(abs(A)sqrt(A^2_1"}, /*Fluxion*/

    {"deriv(sinh(A,X,T", "deriv(A,X,T)cosh(A"},
    {"deriv(cosh(A,X,T", "deriv(A,X,T)sinh(A"},
    {"deriv(tanh(A,X,T", "deriv(A,X,T)/cosh(A)^2"},

    {"deriv(asinh(A,X,T", "deriv(A,X,T)/sqrt(A^2+1"},
    {"deriv(acosh(A,X,T", "deriv(A,X,T)/sqrt(A^2_1"},
    {"deriv(atanh(A,X,T", "deriv(A,X,T)/(1_A^2"},

    {"deriv(abs(A,X,T", "Aderiv(A,X,T)/abs(A"}, /*Fluxion: ln|x| from the integrals*/
};

/*Identities we have to check manually because we have to check for constants*/
pcas_id_t id_deriv_power_rule = {"deriv(A^B,X,T", "deriv(A,X,T)BA^(B_1"};
pcas_id_t id_deriv_constant_rule = {"deriv(CX,X,T", "C"};
pcas_id_t id_deriv_product_rule = {"deriv(AB,X,T", "Aderiv(B,X,T)+Bderiv(A,X,T"};

bool is_constant(pcas_ast_t *e, pcas_ast_t *respect_to) {

    if(pcas_cancelled) /*Fluxion: stop here, the answer's being thrown away*/
        return false;

    if(e == NULL) /*Fluxion: oom. true so every caller takes its cheap branch*/
        return true;

    if(ast_Compare(e, respect_to))
        return false;

    if(e->type == NODE_OPERATOR) {
        pcas_ast_t *child;
        for(child = ast_ChildGet(e, 0); child != NULL; child = child->next) {
            if(!is_constant(child, respect_to))
                return false;
        }
    }

    return true;
}

/*Fluxion: deriv(f,x,at) as a new node*/
static pcas_ast_t *deriv_of(pcas_ast_t *f, pcas_ast_t *x, pcas_ast_t *at) {
    pcas_ast_t *d = ast_MakeOperator(OP_DERIV);

    if(d == NULL) /*oom, the copies wolud leak*/
        return NULL;
    ast_ChildAppend(d, ast_Copy(f));
    ast_ChildAppend(d, ast_Copy(x));
    ast_ChildAppend(d, ast_Copy(at));
    return d;
}

/*Fluxion: f(bound) bound' for an integral of f over t*/
static pcas_ast_t *limit_term(pcas_ast_t *f, pcas_ast_t *t, pcas_ast_t *bound, pcas_ast_t *x, pcas_ast_t *at) {
    pcas_ast_t *f_at = ast_Copy(f);

    if(f_at != NULL)
        substitute(f_at, t, bound);
    return ast_MakeBinary(OP_MULT, f_at, deriv_of(bound, x, at));
}

bool eval_derivative_nodes(pcas_ast_t *e) {
    pcas_ast_t *expr, *respect_to, *at;
    pcas_ast_t *child;

    bool changed = false;

    if(pcas_cancelled) /*Fluxion: stop here, the answer's being thrown away*/
        return false;

    if(e->type != NODE_OPERATOR)
        return false;

    for(child = ast_ChildGet(e, 0); child != NULL; child = child->next)
        changed |= eval_derivative_nodes(child);

    if(!isoptype(e, OP_DERIV))
        return changed;

    expr = ast_ChildGet(e, 0);
    /*Have to copy these because node might change away from deriv node*/
    respect_to = ast_Copy(ast_ChildGet(e, 1));
    at = ast_Copy(ast_ChildGet(e, 2));

    /*Hardcode constant rule*/
    if(is_constant(expr, respect_to)) {
        replace_node(e, ast_MakeNumber(num_FromInt(0)));
        ast_Cleanup(respect_to);
        ast_Cleanup(at);
        return true;
    }
    /*Hardcode multiplication rules*/
    else if(isoptype(expr, OP_MULT)) {
        pcas_ast_t *copy = ast_Copy(e);
        changed |= id_Execute(copy, &id_deriv_constant_rule, false);

        /*Multiplication of a constant*/
        if(is_constant(copy, respect_to)) {
            replace_node(e, copy);
            changed = true;
        }
        /*Apply product rule*/
        else {
            ast_Cleanup(copy);
            changed |= id_Execute(e, &id_deriv_product_rule, false);
        }
    }
    /*Hardcode sum rule*/
    else if(isoptype(expr, OP_ADD)) {
        pcas_ast_t *n = ast_MakeOperator(OP_ADD);

        for(child = ast_ChildGet(expr, 0); child != NULL; child = child->next) {
            ast_ChildAppend(n, deriv_of(child, respect_to, at));
        }

        replace_node(e, n);
        changed = true;
    }
    /*Hardcode power rule because we have to check if the power is a constant*/
    else if(isoptype(expr, OP_POW) && is_constant(ast_ChildGet(expr, 1), respect_to)) {
        changed |= id_Execute(e, &id_deriv_power_rule, false);
    }
    /*Fluxion: f(upper)upper' - f(lower)lower' for an integral with x in its limits and not in f*/
    else if(isoptype(expr, OP_INTEGRAL) && ast_ChildLength(expr) == 4 && ast_ChildGet(expr, 1)->type == NODE_SYMBOL
            && !ast_Compare(ast_ChildGet(expr, 1), respect_to) && is_constant(ast_ChildGet(expr, 0), respect_to)) {
        pcas_ast_t *t = ast_ChildGet(expr, 1), *f = ast_ChildGet(expr, 0);
        pcas_ast_t *n = ast_MakeBinary(OP_ADD, limit_term(f, t, ast_ChildGet(expr, 3), respect_to, at),
                                       ast_MakeBinary(OP_MULT, ast_MakeInt(-1),
                                                      limit_term(f, t, ast_ChildGet(expr, 2), respect_to, at)));

        replace_node(e, n);
        changed = true;
    } else {
        /*While is necessary because of power rules.*/
        while(!pcas_check() && id_ExecuteTable(e, id_derivative, ID_NUM_DERIV, false))
            changed = true;
    }

    for(child = ast_ChildGet(e, 0); child != NULL; child = child->next)
        changed |= eval_derivative_nodes(child);

    if(changed) {
        if(!ast_Compare(respect_to, at))
            substitute(e, respect_to, at);
    }

    ast_Cleanup(respect_to);
    ast_Cleanup(at);

    return changed;
}

void derivative(pcas_ast_t *e, pcas_ast_t *respect_to, pcas_ast_t *eval_at) {
    pcas_ast_t *deriv_node = ast_MakeOperator(OP_DERIV);

    if(pcas_cancelled) { /*Fluxion: stop here, the answer's being thrown away*/
        ast_Cleanup(deriv_node);
        return;
    }

    ast_ChildAppend(deriv_node, ast_Copy(e));           /*value to take the derivative of*/
    ast_ChildAppend(deriv_node, ast_Copy(respect_to));  /*variable in respect to*/
    ast_ChildAppend(deriv_node, ast_Copy(eval_at));     /*evaluate at*/

    while(!pcas_check() && eval_derivative_nodes(deriv_node));

    replace_node(e, deriv_node);
}

/*Fluxion*/
pcas_ast_t *slope_of(pcas_ast_t *e, pcas_ast_t *var) {
    pcas_ast_t *d = ast_Copy(e);

    if(d == NULL)
        return NULL;
    derivative(d, var, var);
    simplify(d, SIMP_ALL);
    if(pcas_cancelled) {
        ast_Cleanup(d);
        return NULL;
    }
    return d;
}