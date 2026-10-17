#include "polyfn.h"

#include "app.h"
#include "settings.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "engine/ast.h"
#include "engine/parser.h"
#include "engine/cas/cas.h"
#include "render/draw.h"

/*
    fromroots( and inverse(, run by the app like geom's. same deal: the line gets
    cut at the top level commas and each piece parsed on its own. roots( isn't
    here, it's just solve( with a different name so app.c sends it to run_solve
*/

#define ARGS_MAX 12

#define SIMP_BASIC (SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_RATIONAL)

/* same as geom's. more than ARGS_MAX parses nothing and says 0, -1 for a parse
   error with err saying which */
static int read_args(char *src, pcas_ast_t **args, pcas_error_t *err) {
    char *piece[ARGS_MAX], *s;
    int depth = 0, pieces = 1, i;

    piece[0] = src;
    for(s = src; *s != '\0'; s++) {
        if(*s == '(')
            depth++;
        else if(*s == ')')
            depth--;
        else if(depth == 0 && *s == ',') {
            if(pieces == ARGS_MAX)
                return 0;
            *s = '\0';
            piece[pieces++] = s + 1;
        }
    }

    for(i = 0; i < pieces; i++) {
        args[i] = parse((const uint8_t *)piece[i], strlen(piece[i]), str_table, err);
        if(*err != E_SUCCESS || args[i] == NULL) {
            if(*err == E_SUCCESS)
                *err = E_GENERIC;
            while(i >= 0)
                ast_Cleanup(args[i--]);
            return -1;
        }
    }
    return pieces;
}

/* a letter on its own (or theta), so it can be the variable */
static bool is_var(const pcas_ast_t *e) {
    return e->type == NODE_SYMBOL
           && ((e->op.symbol >= SYM_A && e->op.symbol <= SYM_Z) || e->op.symbol == SYM_THETA);
}

/* var - r. no subtract node, it's a plus the negative */
static pcas_ast_t *factor_of(Symbol var, pcas_ast_t *r) {
    return ast_MakeBinary(OP_ADD, ast_MakeSymbol((char)var),
                          ast_MakeBinary(OP_MULT, ast_MakeNumber(num_FromInt(-1)), r));
}

/* (x - r1)(x - r2)... multiplied out the way expand( does it */
static box_t *from_roots(pcas_ast_t **a, int n) {
    pcas_ast_t *e;
    Symbol var = SYM_X;
    box_t *out;
    int i;

    /* a letter at the end is the variable, unless it's the only thing there */
    if(n > 1 && is_var(a[n - 1]))
        var = a[--n]->op.symbol;

    e = factor_of(var, ast_Copy(a[0]));
    for(i = 1; i < n; i++)
        e = ast_MakeBinary(OP_MULT, e, factor_of(var, ast_Copy(a[i])));

    simplify(e, SIMP_BASIC);
    expand(e, EXP_ALL);
    simplify(e, SIMP_BASIC | SIMP_EVAL | SIMP_LIKE_TERMS);
    /* not value_box, that puts it over one denominator and (2x^2-7x+3)/2 isn't
       what expand( would say. rationalize is the only canon bit the app flips */
    simplify_canonical_form(e, app_rationalize() ? CANONICAL_ALL : CANONICAL_ALL & ~CANONICAL_RATIONALIZE);
    out = pcas_cancelled ? NULL : layout_build(e, &draw_font);
    ast_Cleanup(e);
    return out;
}

/* the answers side by side with commas, like solve's but without the x = in
   front, they're functions of x and x = sqrt(x) would be nonsense */
static box_t *values_box(pcas_ast_t **v, int n) {
    box_t *row = layout_box(BOX_ROW, 0);
    int i;

    for(i = 0; i < n; i++) {
        if(i > 0) {
            layout_add(row, layout_text(",", 0));
            layout_add(row, layout_space(2 * SPACE_BINARY));
        }
        layout_add(row, value_box(v[i]));
    }
    layout_finish(row, &draw_font);
    return row;
}

/* solve y = f(x) for x, then call y x again. y is just the usual letter, if f
   already has one some other letter stands in. not n, that's the trig n */
static box_t *inverse_of(pcas_ast_t *f, Symbol var, const char **message) {
    pcas_ast_t *roots[SOLVE_MAX] = { NULL }, *e, *t, *x;
    Symbol tmp = SYM_Y;
    box_t *out = NULL;
    int n, i;

    if(var == SYM_Y || has_symbol(f, SYM_Y))
        for(tmp = SYM_A; tmp <= SYM_Z; tmp++)
            if(tmp != var && tmp != SYM_N && !has_symbol(f, tmp))
                break;
    if(tmp > SYM_Z) {
        *message = "no letter left over to use";
        return NULL;
    }

    e = ast_Copy(f);
    if(setting_angle == ANGLE_DEGREES)
        e = degrees_in(e);
    e = ast_MakeBinary(OP_ADD, e, ast_MakeBinary(OP_MULT, ast_MakeNumber(num_FromInt(-1)),
                                                 ast_MakeSymbol((char)tmp)));
    n = solve_ex(e, var, roots, false);
    ast_Cleanup(e);

    if(pcas_cancelled) {
        for(i = 0; i < n; i++)
            ast_Cleanup(roots[i]);
        return NULL;
    }
    /* SOLVE_ALL too, f didn't have x in it so there's nothing to undo */
    if(n <= 0) {
        *message = "no inverse found";
        return NULL;
    }

    t = ast_MakeSymbol((char)tmp);
    x = ast_MakeSymbol((char)var);
    for(i = 0; i < n; i++) {
        substitute(roots[i], t, x);
        simplify(roots[i], SIMP_ALL);
        if(setting_angle == ANGLE_DEGREES)
            degrees_out(roots[i]);
    }
    ast_Cleanup(t);
    ast_Cleanup(x);

    if(!pcas_cancelled)
        out = values_box(roots, n);
    for(i = 0; i < n; i++)
        ast_Cleanup(roots[i]);
    return out;
}

bool polyfn_run(const char *cmd, char *src, box_t *in) {
    pcas_ast_t *a[ARGS_MAX] = { NULL };
    pcas_error_t err = E_SUCCESS;
    const char *message = NULL, *usage;
    box_t *out = NULL;
    clock_t start;
    bool inv;
    int n, i;

    if(cmd == NULL)
        return false;
    if(strcmp(cmd, "fromroots") == 0)
        inv = false;
    else if(strcmp(cmd, "inverse") == 0)
        inv = true;
    else
        return false;
    usage = inv ? "inverse(f, x)" : "fromroots(root, root, ..., x)";

    n = read_args(src, a, &err);
    if(n < 1 || (inv && (n > 2 || (n == 2 && !is_var(a[1]))))) {
        for(i = 0; i < n; i++)
            ast_Cleanup(a[i]);
        push(in, NULL, n < 0 ? error_text[err] : usage, 0);
        return true;
    }

    start = clock();
    if(inv)
        out = inverse_of(a[0], n == 2 ? a[1]->op.symbol : SYM_X, &message);
    else
        out = from_roots(a, n);

    if(pcas_cancelled && out == NULL)
        message = pcas_oom ? "out of memory" : "cancelled";
    push(in, out, message, (unsigned long)((clock() - start) * 1000 / CLOCKS_PER_SEC));

    for(i = 0; i < n; i++)
        ast_Cleanup(a[i]);
    return true;
}
