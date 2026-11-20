#ifdef COMPILE_PC

#include "tests.h"

#include <string.h>
#include <stdlib.h>

#include "../parser.h"
#include "../ast.h"
#include "../cas/cas.h"
#include "../cas/steps.h"

#include "../dbg.h"

/*Trim null terminated string*/
static char *trim(char *str) {
    unsigned i, trimmed_len = 0, trim_index = 0;
    char *trimmed;

    for(i = 0; i < strlen(str) + 1; i++) {
        if(str[i] != ' ' && str[i] != '\t' && str[i] != '\n')
            trimmed_len++;
    }

    trimmed = malloc(trimmed_len * sizeof(char));

    for(i = 0; i < strlen(str) + 1; i++) {
        if(str[i] != ' ' && str[i] != '\t' && str[i] != '\n')
            trimmed[trim_index++] = str[i];
    }

    return trimmed;
}

TestType resolve_type(char *type) {

    if(!strcmp(type, "simplify"))   return TEST_SIMPLIFY;
    if(!strcmp(type, "gcd"))        return TEST_GCD;
    if(!strcmp(type, "factor"))     return TEST_FACTOR;
    if(!strcmp(type, "factortext")) return TEST_FACTOR_TEXT;
    if(!strcmp(type, "simplifytext")) return TEST_SIMPLIFY_TEXT;
    if(!strcmp(type, "steps"))      return TEST_STEPS;
    if(!strcmp(type, "expand"))     return TEST_EXPAND;
    if(!strcmp(type, "deriv"))      return TEST_DERIV;
    if(!strcmp(type, "solve"))      return TEST_SOLVE;
    if(!strcmp(type, "cancel"))     return TEST_CANCEL;
    if(!strcmp(type, "comdenom"))   return TEST_COMDENOM;
    if(!strcmp(type, "comdenomf"))  return TEST_COMDENOMF;
    if(!strcmp(type, "decimal"))    return TEST_DECIMAL;
    if(!strcmp(type, "asym"))       return TEST_ASYM;
    if(!strcmp(type, "domain"))     return TEST_DOMAIN;
    if(!strcmp(type, "system"))     return TEST_SYSTEM;
    if(!strcmp(type, "turn"))       return TEST_TURN;
    if(!strcmp(type, "csolve"))     return TEST_CSOLVE;

    return TEST_INVALID;
}

test_t *test_Parse(char *line) {
    char *pt, *trimmed;
    unsigned i;
    test_t *t = malloc(sizeof(test_t));

    pt = strtok(line, ";");
    for(i = 0; i < 3; i++) {

        if(pt == NULL) {
            free(t);
            return NULL;
        }

        trimmed = trim(pt);

        if(i == 0)      t->type = resolve_type(trimmed);
        else if(i == 1) strncpy(t->arg1, trimmed, MAX_PAR);
        else if(i == 2) strncpy(t->arg2, trimmed, MAX_PAR);

        free(trimmed);

        pt = strtok(NULL, ";");
    }

    if(pt != NULL) {
        trimmed = trim(pt);
        strncpy(t->arg3, trimmed, MAX_PAR);
        free(trimmed);
    } else {
        t->arg3[0] = '\0';
    }

    return t;
}

test_t **test_Load(char *file, unsigned *len) {
    FILE *f;
    unsigned i = 0, cur_line = 1;
    test_t **arr;
    char line[MAX_LINE];

    arr = malloc(sizeof(test_t**) * MAX_TESTS);

    f = fopen(file, "r");

    if(f == NULL) {
        free(arr);
        return NULL;
    }

    while(fgets(line, sizeof(line), f) != NULL) {
        test_t *t;

        /*Fluxion: say so, otherwise the rest of the file looks like it passed. wasted ages on this*/
        if(i >= MAX_TESTS) {
            printf("Only the first %u tests in %s are run (MAX_TESTS), the rest are skipped.\n", MAX_TESTS, file);
            break;
        }

        t = test_Parse(line);

        if(t != NULL && t->type != TEST_INVALID) {
            arr[i] = t;

            t->line = cur_line;

            i++;
        } else if(t != NULL) {
            free(t);
        }

        cur_line++;

    }

    fclose(f);

    *len = i;

    return arr;
}

bool check(test_t *t, pcas_ast_t *actual, pcas_ast_t *expected) {
    char *output_expected, *output_actual;
    unsigned expected_len, actual_len;

    pcas_error_t expted_err, actual_err;

    if(!ast_Compare(expected, actual)) {

        output_expected = (char*)export_to_binary(expected, &expected_len, str_table, &expted_err);
        output_actual = (char*)export_to_binary(actual, &actual_len, str_table, &actual_err);

        if(expted_err != E_SUCCESS || actual_err != E_SUCCESS) {
            printf("Test failed on line %u. Error when exporting ast to text.\n", t->line);
        } else {
            printf("Test failed on line %u. Expected %.*s but got %.*s\n", t->line, expected_len, output_expected, actual_len, output_actual);

            free(output_expected);
            free(output_actual);
        }

        printf("Expected:\n");
        dbg_print_tree(expected, 4);
        printf("Actual:\n");
        dbg_print_tree(actual, 4);

        return false;

    }

    return true;
}

/*Fluxion: solve; equation; var; expected. the equation is A=B, or A for A=0.
expected is the roots smallest first (A,B,...), or true / false / unknown.
one_turn is turn; the same, then solve_in_range() to 2pi like the one turn mode row.
complex is csolve;, solve_ex() with the complex roots like the app*/
static bool solve_roots(test_t *t, bool one_turn, bool complex) {
    pcas_ast_t *roots[SOLVE_MAX] = {0}, *e, *rhs = NULL, *hi;
    char *eq = strchr(t->arg1, '='), *s, *start;
    pcas_error_t err;
    int n, i = 0, depth = 0;
    bool passed = true;

    if(eq != NULL)
        *eq = '\0';
    e = parse((uint8_t*)t->arg1, strlen(t->arg1), str_table, &err);
    if(err == E_SUCCESS && eq != NULL)
        rhs = parse((uint8_t*)eq + 1, strlen(eq + 1), str_table, &err);
    if(err != E_SUCCESS || e == NULL || (eq != NULL && rhs == NULL)) {
        printf("Test failed on line %u. Unable to parse the equation\n", t->line);
        return false;
    }
    if(rhs != NULL)
        e = ast_MakeBinary(OP_ADD, e, ast_MakeBinary(OP_MULT, ast_MakeNumber(num_FromInt(-1)), rhs));

    if(e == NULL) { /*Fluxion: oom*/
        printf("Test failed on line %u. Out of memory building the equation\n", t->line);
        return false;
    }

    n = solve_ex(e, (Symbol)t->arg2[0], roots, complex);

    if(one_turn && n > 0) {
        hi = parse((uint8_t*)"2pi", 3, str_table, &err);
        if(err != E_SUCCESS || hi == NULL) {
            printf("Test failed on line %u. Unable to parse 2pi\n", t->line);
            for(i = 0; i < n; i++)
                ast_Cleanup(roots[i]);
            ast_Cleanup(e);
            return false;
        }
        n = solve_in_range(roots, n, hi);
        ast_Cleanup(hi);
    }

    if(!strcmp(t->arg3, "true"))
        passed = n == SOLVE_ALL;
    else if(!strcmp(t->arg3, "false"))
        passed = n == 0;
    else if(!strcmp(t->arg3, "unknown"))
        passed = n == SOLVE_UNKNOWN;
    else {
        /*each expected root in turn, split at the commas outside brackets*/
        for(start = s = t->arg3; passed; s++) {
            if(*s == '(') {
                depth++;
            } else if(*s == ')') {
                depth--;
            } else if((*s == ',' && depth == 0) || *s == '\0') {
                bool last = *s == '\0';
                pcas_ast_t *expected;

                *s = '\0';
                expected = parse((uint8_t*)start, strlen(start), str_table, &err);
                if(err != E_SUCCESS || expected == NULL) {
                    printf("Test failed on line %u. Unable to parse root %s\n", t->line, start);
                    passed = false;
                } else {
                    simplify(expected, SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_RATIONAL | SIMP_EVAL);
                    passed = i < n && check(t, roots[i], expected);
                }
                if(expected != NULL)
                    ast_Cleanup(expected);
                i++;

                if(last)
                    break;
                start = s + 1;
            }
        }
        passed = passed && i == n;
    }

    if(!passed) {
        printf("Test failed on line %u. %s gave %d root(s)", t->line, one_turn ? "solve_in_range()" : complex ? "solve_ex()" : "solve()", n);
        for(i = 0; i < n; i++) {
            unsigned len;
            char *out = (char*)export_to_binary(roots[i], &len, str_table, &err);

            if(out != NULL) {
                printf("%s %.*s", i == 0 ? ":" : ",", len, out);
                free(out);
            }
        }
        printf("\n");
    }

    for(i = 0; i < n; i++)
        ast_Cleanup(roots[i]);
    ast_Cleanup(e);
    return passed;
}

static bool test_solve(test_t *t) {
    return solve_roots(t, false, false);
}

/*Fluxion: csolve; equation; var; same as test_solve but quadratics can come out complex*/
static bool test_csolve(test_t *t) {
    return solve_roots(t, false, true);
}

/*Fluxion: turn; equation; var; the roots from 0 up to 2pi, smallest first*/
static bool test_turn(test_t *t) {
    return solve_roots(t, true, false);
}

/*Fluxion: a - b simplifies to 0 (2sqrt(2) is 2^(3/2)), or both match once rationalized. sqrt(15)/sqrt(2) and sqrt(30)/2 never cancel as a difference*/
static bool same_value(pcas_ast_t *a, pcas_ast_t *b) {
    pcas_ast_t *d = ast_MakeBinary(OP_ADD, ast_Copy(a), ast_MakeBinary(OP_MULT, ast_MakeNumber(num_FromInt(-1)), ast_Copy(b)));
    pcas_ast_t *x, *y;
    bool same;

    simplify(d, SIMP_ALL);
    same = is_ast_int(d, 0);
    ast_Cleanup(d);
    if(same)
        return true;
    x = ast_Copy(a);
    y = ast_Copy(b);
    simplify(x, SIMP_ALL);
    simplify(y, SIMP_ALL);
    simplify_canonical_form(x, CANONICAL_ALL);
    simplify_canonical_form(y, CANONICAL_ALL);
    same = ast_Compare(x, y);
    ast_Cleanup(x);
    ast_Cleanup(y);
    return same;
}

/*Fluxion: e exproted onto the end of buf, ? if it won't export*/
static void append_tree(char *buf, unsigned size, pcas_ast_t *e) {
    pcas_error_t err;
    unsigned len;
    char *out = (char*)export_to_binary(e, &len, str_table, &err);

    if(out == NULL || err != E_SUCCESS) {
        strncat(buf, "?", size - strlen(buf) - 1);
    } else {
        if(len > size - strlen(buf) - 1)
            len = size - strlen(buf) - 1;
        strncat(buf, out, len); /*out isn't null terminated*/
    }
    free(out);
}

/*Fluxion: the steps as one line, the way the test file writes them*/
static void steps_text(const steps_t *s, char *got, unsigned size) {
    int i, j, k;

    for(i = 0; i < s->n; i++) {
        for(j = 0; j < s->line[i].eqs; j++) {
            const step_eq_t *q = &s->line[i].eq[j];

            if(j > 0)
                strncat(got, s->line[i].list ? "," : "or", size - strlen(got) - 1);
            else if(i > 0)
                strncat(got, "|", size - strlen(got) - 1);
            append_tree(got, size, q->left);
            if(q->right == NULL && q->pm == NULL) /*just an expression, *dih*rivative steps*/
                continue;
            strncat(got, q->den != NULL ? "=(" : "=", size - strlen(got) - 1);
            if(q->right != NULL)
                append_tree(got, size, q->right);
            if(q->pm != NULL) {
                strncat(got, "+-", size - strlen(got) - 1);
                append_tree(got, size, q->pm);
            }
            if(q->den != NULL) {
                strncat(got, ")/", size - strlen(got) - 1);
                append_tree(got, size, q->den);
            }
        }
        if(s->line[i].move == 'n') {
            for(k = 0; k < STEP_RULES_MAX && s->line[i].rules[k] != NULL; k++) {
                const char *c;

                strncat(got, k == 0 ? "|" : ",", size - strlen(got) - 1);
                for(c = s->line[i].rules[k]; *c != '\0'; c++) /*no spaces, the test file's are trimmed off*/
                    if(*c != ' ' && strlen(got) < size - 1)
                        strncat(got, c, 1);
            }
        } else if(s->line[i].move != 0) {
            char move[3] = "|?";

            move[1] = s->line[i].move;
            strncat(got, move, size - strlen(got) - 1);
            append_tree(got, size, s->line[i].by);
        }
    }
}

/*Fluxion: steps; deriv(f,X,X) or limit(f,X,a); X; the lines | the rules | lines, or none. the last line has to be what simplify makes of it*/
static bool test_calc_steps(test_t *t) {
    pcas_error_t err;
    pcas_ast_t *e = parse((uint8_t*)t->arg1, strlen(t->arg1), str_table, &err);
    char got[MAX_PAR * 2] = "";
    steps_t s;
    bool passed;

    if(err != E_SUCCESS || e == NULL) {
        printf("Test failed on line %u. Unable to parse %s\n", t->line, t->arg1);
        return false;
    }
    if(!(isoptype(e, OP_LIMIT) ? steps_limit(e, &s) : steps_derivative(e, &s)))
        strcpy(got, "none");
    else
        steps_text(&s, got, sizeof(got));
    passed = strcmp(got, t->arg3) == 0;
    if(!passed)
        printf("Test failed on line %u. Expected %s but got %s\n", t->line, t->arg3, got);

    if(passed && s.n > 0 && !same_value(e, s.line[s.n - 1].eq[0].left)) {
        printf("Test failed on line %u. The steps end somewhere else than simplify\n", t->line);
        passed = false;
    }
    if(s.n > 0)
        steps_free(&s);
    ast_Cleanup(e);
    return passed;
}

/*Fluxion: steps; equation; var and a method (X, Xformula, ...); the lines joinned with |, the move between
two lines as its sign and what it uses (_5, /3), or none. a last line of var = numbers has to be what solve says*/
static bool test_steps(test_t *t) {
    static const char *const methods[METHOD_COUNT] = { "", "root", "factor", "square", "formula" };
    pcas_ast_t *left, *right, *e, *roots[SOLVE_MAX] = {0};
    char *eq = strchr(t->arg1, '='), got[MAX_PAR * 2] = "";
    pcas_error_t err;
    steps_t s;
    step_t *last;
    int m, i, j, n;
    bool passed;

    if(eq == NULL)
        return test_calc_steps(t);
    for(m = 0; m < METHOD_COUNT && strcmp(t->arg2 + 1, methods[m]) != 0; m++);
    if(m == METHOD_COUNT) {
        printf("Test failed on line %u. steps; equation; var and method; lines\n", t->line);
        return false;
    }
    *eq = '\0';
    left = parse((uint8_t*)t->arg1, strlen(t->arg1), str_table, &err);
    right = err == E_SUCCESS ? parse((uint8_t*)eq + 1, strlen(eq + 1), str_table, &err) : NULL;
    if(err != E_SUCCESS || left == NULL || right == NULL) {
        printf("Test failed on line %u. Unable to parse the equation\n", t->line);
        ast_Cleanup(left);
        ast_Cleanup(right);
        return false;
    }

    if(!steps_solve(left, right, (Symbol)t->arg2[0], (Method)m, &s))
        strcpy(got, "none");
    else
        steps_text(&s, got, sizeof(got));
    passed = strcmp(got, t->arg3) == 0;
    if(!passed)
        printf("Test failed on line %u. Expected %s but got %s\n", t->line, t->arg3, got);

    /*the last line against solve, var = a number each. complex like the app's solve*/
    last = s.n > 0 ? &s.line[s.n - 1] : NULL;
    if(passed && last != NULL && last->eq[0].left->type == NODE_SYMBOL && last->eq[0].pm == NULL) {
        e = ast_MakeBinary(OP_ADD, ast_Copy(left), ast_MakeBinary(OP_MULT, ast_MakeNumber(num_FromInt(-1)), ast_Copy(right)));
        n = solve_ex(e, (Symbol)t->arg2[0], roots, true);
        for(j = 0; j < last->eqs; j++) {
            for(i = 0; i < n && !same_value(roots[i], last->eq[j].right); i++);
            if(n < 0 || i == n) {
                printf("Test failed on line %u. The steps end at a root solve doesn't have\n", t->line);
                passed = false;
                break;
            }
        }
        if(passed && n != last->eqs) {
            printf("Test failed on line %u. solve has %d roots, the steps %d\n", t->line, n, last->eqs);
            passed = false;
        }
        for(i = 0; i < n; i++)
            ast_Cleanup(roots[i]);
        ast_Cleanup(e);
    }

    if(s.n > 0)
        steps_free(&s);
    ast_Cleanup(left);
    ast_Cleanup(right);
    return passed;
}

/*Fluxion: the values exported like test_solve prints its roots, joined with commas, - for none*/
static void asym_list(char *buf, unsigned size, pcas_ast_t **vals, int n) {
    pcas_error_t err;
    unsigned len;
    char *out;
    int i;

    if(n == 0)
        strncat(buf, " -", size - strlen(buf) - 1);
    for(i = 0; i < n; i++) {
        strncat(buf, i == 0 ? " " : ",", size - strlen(buf) - 1);
        out = (char*)export_to_binary(vals[i], &len, str_table, &err);
        if(out == NULL || err != E_SUCCESS) { /*never matches, so it can't pass by accident*/
            strncat(buf, "?", size - strlen(buf) - 1);
        } else {
            if(len > size - strlen(buf) - 1)
                len = size - strlen(buf) - 1;
            strncat(buf, out, len); /*out isn't null terminated*/
        }
        free(out);
    }
}

/*Fluxion: asym; expression; var; holes <list> | vert <list> | horiz <y> | slant <y>. holes and vert
are x values smallest first, horiz and slant are - when there's none. unknown when asym() gives up*/
static bool test_asym(test_t *t) {
    pcas_error_t err;
    pcas_ast_t *e = parse((uint8_t*)t->arg1, strlen(t->arg1), str_table, &err);
    asym_t a;
    char got[MAX_LINE] = "";
    char *trimmed;
    bool passed;

    if(err != E_SUCCESS || e == NULL) {
        printf("Test failed on line %u. Unable to parse %s\n", t->line, t->arg1);
        return false;
    }

    if(asym(e, (Symbol)t->arg2[0], &a)) {
        strcpy(got, "holes");
        asym_list(got, sizeof(got), a.holes, a.nholes);
        strncat(got, " | vert", sizeof(got) - strlen(got) - 1);
        asym_list(got, sizeof(got), a.vert, a.nvert);
        strncat(got, " | horiz", sizeof(got) - strlen(got) - 1);
        asym_list(got, sizeof(got), &a.horiz, a.horiz != NULL);
        strncat(got, " | slant", sizeof(got) - strlen(got) - 1);
        asym_list(got, sizeof(got), &a.slant, a.slant != NULL);
        asym_free(&a);
    } else {
        strcpy(got, "unknown");
    }

    /*the expected one lost its spaces in test_Parse*/
    trimmed = trim(got);
    passed = strcmp(trimmed, t->arg3) == 0;
    free(trimmed);
    if(!passed)
        printf("Test failed on line %u. Expected %s but got %s\n", t->line, t->arg3, got);

    ast_Cleanup(e);
    return passed;
}

/*Fluxion: domain; expression; var; lower <x> | upper <x> | holes <list>. o after a bound
for open (lower 0o is x > 0), - for none, | partial on the end when domain() says there may
be more out. empty when there's no x at all, unknown when it gives up*/
static bool test_domain(test_t *t) {
    pcas_error_t err;
    pcas_ast_t *e = parse((uint8_t*)t->arg1, strlen(t->arg1), str_table, &err);
    domain_t d;
    char got[MAX_LINE] = "";
    char *trimmed;
    bool passed;

    if(err != E_SUCCESS || e == NULL) {
        printf("Test failed on line %u. Unable to parse %s\n", t->line, t->arg1);
        return false;
    }

    if(!domain(e, (Symbol)t->arg2[0], &d)) {
        strcpy(got, "unknown");
    } else if(d.empty) {
        strcpy(got, "empty");
        domain_free(&d);
    } else {
        strcpy(got, "lower");
        asym_list(got, sizeof(got), &d.lower, d.lower != NULL);
        if(d.lower_open)
            strncat(got, "o", sizeof(got) - strlen(got) - 1);
        strncat(got, " | upper", sizeof(got) - strlen(got) - 1);
        asym_list(got, sizeof(got), &d.upper, d.upper != NULL);
        if(d.upper_open)
            strncat(got, "o", sizeof(got) - strlen(got) - 1);
        strncat(got, " | holes", sizeof(got) - strlen(got) - 1);
        asym_list(got, sizeof(got), d.holes, d.nholes);
        if(d.partial)
            strncat(got, " | partial", sizeof(got) - strlen(got) - 1);
        domain_free(&d);
    }

    trimmed = trim(got);
    passed = strcmp(trimmed, t->arg3) == 0;
    free(trimmed);
    if(!passed)
        printf("Test failed on line %u. Expected %s but got %s\n", t->line, t->arg3, got);

    ast_Cleanup(e);
    return passed;
}

/*Fluxion: system; eq,eq,...; vars; expected. each eq is A=B, or A for A=0. vars run
together (XY), one letter each. expected is the values in var order, solution sets
split by |, or false / many / unknown*/
static bool test_system(test_t *t) {
    pcas_ast_t *eqs[SYSTEM_MAX] = {0}, *vals[SYSTEM_SETS][SYSTEM_MAX] = {{0}}, *e, *rhs;
    Symbol vars[SYSTEM_MAX];
    char got[MAX_LINE] = "";
    char *s, *start, *eq, *trimmed;
    pcas_error_t err;
    int n = 0, nvars = (int)strlen(t->arg2), r = SOLVE_UNKNOWN, i, k;
    bool passed = true, last;

    if(nvars < 1 || nvars > SYSTEM_MAX) {
        printf("Test failed on line %u. Need 1 to %d variables, got %s\n", t->line, SYSTEM_MAX, t->arg2);
        return false;
    }
    for(i = 0; i < nvars; i++)
        vars[i] = (Symbol)t->arg2[i];

    /*one equation per comma, the = turned into A+(-1)B like test_solve*/
    for(start = s = t->arg1; passed; s++) {
        if(*s != ',' && *s != '\0')
            continue;

        last = *s == '\0';
        *s = '\0';
        if(n == SYSTEM_MAX) {
            printf("Test failed on line %u. More than %d equations\n", t->line, SYSTEM_MAX);
            passed = false;
            break;
        }

        rhs = NULL;
        eq = strchr(start, '=');
        if(eq != NULL)
            *eq = '\0';
        e = parse((uint8_t*)start, strlen(start), str_table, &err);
        if(err == E_SUCCESS && eq != NULL)
            rhs = parse((uint8_t*)eq + 1, strlen(eq + 1), str_table, &err);
        if(err != E_SUCCESS || e == NULL || (eq != NULL && rhs == NULL)) {
            printf("Test failed on line %u. Unable to parse equation %d\n", t->line, n + 1);
            passed = false;
            break;
        }
        if(rhs != NULL)
            e = ast_MakeBinary(OP_ADD, e, ast_MakeBinary(OP_MULT, ast_MakeNumber(num_FromInt(-1)), rhs));
        if(e == NULL) {
            printf("Test failed on line %u. Out of memory building equation %d\n", t->line, n + 1);
            passed = false;
            break;
        }
        eqs[n++] = e;

        if(last)
            break;
        start = s + 1;
    }

    if(passed && n != nvars) {
        printf("Test failed on line %u. %d equations but %d variables\n", t->line, n, nvars);
        passed = false;
    }

    if(passed) {
        r = solve_system(eqs, n, vars, vals);

        if(r == 0)
            strcpy(got, "false");
        else if(r == SOLVE_ALL)
            strcpy(got, "many");
        else if(r == SOLVE_UNKNOWN)
            strcpy(got, "unknown");
        else if(r > 0 && r <= SYSTEM_SETS)
            for(k = 0; k < r; k++) {
                if(k > 0)
                    strncat(got, " |", sizeof(got) - strlen(got) - 1);
                asym_list(got, sizeof(got), vals[k], n);
            }
        else /*shouldn't happen, but say what it was*/
            sprintf(got, "solve_system() returned %d", r);

        /*the expected one lost its spaces in test_Parse*/
        trimmed = trim(got);
        passed = strcmp(trimmed, t->arg3) == 0;
        free(trimmed);
        if(!passed) /*asym_list starts with a space*/
            printf("Test failed on line %u. Expected %s but got %s\n", t->line, t->arg3, got[0] == ' ' ? got + 1 : got);
    }

    for(i = 0; i < SYSTEM_MAX; i++) {
        for(k = 0; k < SYSTEM_SETS; k++)
            if(vals[k][i] != NULL)
                ast_Cleanup(vals[k][i]);
        if(eqs[i] != NULL)
            ast_Cleanup(eqs[i]);
    }
    return passed;
}

/*Fluxion: decimal; expression without variables; what approx_format() should print*/
static bool test_decimal(test_t *t) {
    pcas_error_t err;
    pcas_ast_t *e = parse((uint8_t*)t->arg1, strlen(t->arg1), str_table, &err);
    char out[32];
    bool passed;

    if(err != E_SUCCESS || e == NULL) {
        printf("Test failed on line %u. Unable to parse %s\n", t->line, t->arg1);
        return false;
    }

    simplify(e, SIMP_ALL);
    approx(e);
    simplify(e, SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_RATIONAL | SIMP_EVAL);

    /*simplify keeps a fraction as a/b over whole numbers*/
    if(isoptype(e, OP_DIV) && ast_ChildGet(e, 0)->type == NODE_NUMBER && ast_ChildGet(e, 1)->type == NODE_NUMBER) {
        mp_rat q = num_Copy(ast_ChildGet(e, 0)->op.num);

        mp_rat_div(q, ast_ChildGet(e, 1)->op.num, q);
        replace_node(e, ast_MakeNumber(q));
    }

    passed = e->type == NODE_NUMBER;
    if(passed) {
        approx_format(e->op.num, 6, out);
        passed = strcmp(out, t->arg2) == 0;
    } else {
        strcpy(out, "not a number");
    }
    if(!passed)
        printf("Test failed on line %u. Expected %s but got %s\n", t->line, t->arg2, out);

    ast_Cleanup(e);
    return passed;
}

/*Fluxion: e against arg2 as text, the way export prints it*/
static bool check_text(test_t *t, pcas_ast_t *e) {
    pcas_error_t err;
    unsigned len;
    char *out;
    bool passed;

    out = (char*)export_to_binary(e, &len, str_table, &err);
    if(err != E_SUCCESS || out == NULL) {
        printf("Test failed on line %u. Error when exporting ast to text.\n", t->line);
        free(out);
        return false;
    }

    passed = len == strlen(t->arg2) && strncmp(out, t->arg2, len) == 0;
    if(!passed)
        printf("Test failed on line %u. Expected %s but got %.*s\n", t->line, t->arg2, len, out);
    free(out);
    return passed;
}

bool test_Run(test_t *t) {
    pcas_ast_t *a = NULL, *b = NULL, *c = NULL, *expected, *actual;
    pcas_error_t err;
    bool passed = false;

    /*Fluxion: its arguments aren't all expressions*/
    if(t->type == TEST_SOLVE)
        return test_solve(t);
    if(t->type == TEST_TURN)
        return test_turn(t);
    if(t->type == TEST_CSOLVE)
        return test_csolve(t);
    if(t->type == TEST_DECIMAL)
        return test_decimal(t);
    if(t->type == TEST_ASYM)
        return test_asym(t);
    if(t->type == TEST_DOMAIN)
        return test_domain(t);
    if(t->type == TEST_SYSTEM)
        return test_system(t);
    if(t->type == TEST_STEPS)
        return test_steps(t);

    a = parse((uint8_t*)t->arg1, strlen(t->arg1), str_table, &err);
    if(err != E_SUCCESS) {
        printf("Test failed on line %u. Unable to parse first argument %s\n", t->line, t->arg1);
        return false;
    }

    b = parse((uint8_t*)t->arg2, strlen(t->arg2), str_table, &err);
    if(err != E_SUCCESS) {
        ast_Cleanup(a);
        printf("Test failed on line %u. Unable to parse second argument %s\n", t->line, t->arg2);
        return false;
    }

    c = parse((uint8_t*)t->arg3, strlen(t->arg3), str_table, &err);
    if(err != E_SUCCESS) {
        ast_Cleanup(a);
        ast_Cleanup(b);
        printf("Test failed on line %u. Unable to parse third argument %s\n", t->line, t->arg3);
        return false;
    }

    if(a == NULL || b == NULL) {
        ast_Cleanup(c);
        printf("Test failed on line %u. Empty %s argument.\n", t->line, a == NULL ? "first" : "second");
        return false;
    }

    switch(t->type) {
    case TEST_SIMPLIFY:
        expected = b;
        actual = a;

        eval(actual, EVAL_ALL);
        simplify(actual, SIMP_ALL);

        /*We do this to change -1 * 23 to -23 to be able to compare*/
        simplify(expected, SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_RATIONAL | SIMP_EVAL);

        passed = check(t, actual, expected);
        break;
    case TEST_CANCEL:
        expected = b;
        actual = a;

        cancel_fractions(actual);
        simplify(actual, SIMP_ALL);
        simplify(expected, SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_RATIONAL | SIMP_EVAL);

        passed = check(t, actual, expected);
        break;
    case TEST_COMDENOM:
    case TEST_COMDENOMF:
        expected = b;
        actual = a;

        simplify(actual, SIMP_ALL);
        comdenom(actual, t->type == TEST_COMDENOMF);
        simplify(expected, SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_RATIONAL | SIMP_EVAL);

        passed = check(t, actual, expected);
        break;
    case TEST_GCD:
        if(c == NULL) {
            printf("Test failed on line %u. Empty third argument.\n", t->line);
            break;
        }

        expected = c;

        /*We do this to change -1 * 23 to -23 to be able to compare*/
        simplify(expected, SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_RATIONAL | SIMP_EVAL);

        simplify(a, SIMP_ALL);
        simplify(b, SIMP_ALL);
        actual = gcd(a, b);
        simplify(actual, SIMP_ALL);

        passed = check(t, actual, expected);

        ast_Cleanup(actual);
        break;
    case TEST_FACTOR:
        expected = b;
        actual = a;

        /*Fluxion: a whole number comes out as a product of numbers and simplifying the
        expected would just multiply it back, so those compare as text (-12 is -2^2*3)*/
        if(factor_command(actual)) {
            passed = check_text(t, actual);
            break;
        }

        /*We do this to change -1 * 23 to -23 to be able to compare*/
        simplify(expected, SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_RATIONAL | SIMP_EVAL);

        passed = check(t, actual, expected);
        break;
    case TEST_FACTOR_TEXT:
        /*Fluxion: what the factor command shows, the canonical form packs things back up after the tree compare*/
        if(!factor_command(a))
            simplify_canonical_form(a, CANONICAL_ALL);
        passed = check_text(t, a);
        break;
    case TEST_SIMPLIFY_TEXT:
        cancel_fractions(a);
        simplify(a, SIMP_ALL);
        simplify_canonical_form(a, CANONICAL_ALL);
        passed = check_text(t, a);
        break;
    case TEST_EXPAND:
        expected = b;
        actual = a;

        /*We do this to change -1 * 23 to -23 to be able to compare*/
        simplify(expected, SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_RATIONAL | SIMP_EVAL);

        simplify(actual, SIMP_ALL);
        expand(actual, EXP_ALL);
        simplify(actual, SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_RATIONAL | SIMP_LIKE_TERMS | SIMP_EVAL);

        passed = check(t, actual, expected);
        break;
    case TEST_DERIV: {

        if(c == NULL) {
            printf("Test failed on line %u. Empty third argument.\n", t->line);
            break;
        }

        actual = a;
        expected = c;

        derivative(a, b, b);

        /*We do this to change -1 * 23 to -23 to be able to compare*/
        simplify(expected, SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_RATIONAL | SIMP_EVAL);

        /*Don't simplify derivatives because derivative() should have done that*/
        simplify(actual, SIMP_ALL ^ SIMP_DERIV);

        passed = check(t, actual, expected);

        break;
    }
    default:
        break;
    }

    /*This is just here to ensure that the canonical form function terminates on every test case*/
    if(a != NULL) {
        simplify_canonical_form(a, CANONICAL_ALL);
        ast_Cleanup(a);
    }
    if(b != NULL) {
        simplify_canonical_form(b, CANONICAL_ALL);
        ast_Cleanup(b);
    }
    if(c != NULL) {
        simplify_canonical_form(c, CANONICAL_ALL);
        ast_Cleanup(c);
    }

    return passed;
}

void test_Cleanup(test_t *t) {
    free(t);
}

void test_CleanupArr(test_t **arr, unsigned len) {
    unsigned i;
    for(i = 0; i < len; i++) {
        test_Cleanup(arr[i]);
    }
    free(arr);
}

#else
typedef int make_iso_compilers_happy;
#endif