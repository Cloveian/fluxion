/*
    PineappleCAS: the multi-purpose CAS specifically for the TI-84+ CE

    Authors:
    Nathan Farlow
*/

#ifdef COMPILE_PC

#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include <string.h>

#include "../ast.h"
#include "../parser.h"

#include "../dbg.h"

#include "../cas/cas.h"

#include "tests.h"

void display_help() {
    printf("Usage: ./pineapple [operation] [args]\n");
    printf("Valid operations include:\n");
    printf("\ttest [file]\t\t\tRuns all tests in file\n");
    printf("\tbudget [file]\t\t\tWork, PC time and CE guess per test, most work first\n"); /*Fluxion*/
    printf("\tsimplify [expression]\t\tSimplifies expression. Also evaluates constants.\n");
    printf("\tgcd [expression1] [expression2]\tPrints the GCD of the two expressions\n");
    printf("\tfactor [expression]\t\tFactors expression\n");
    printf("\texpand [expression]\t\tExpands expression\n");
    printf("\tderivative [expression] [respect to] [(optional) eval at]\n");
    printf("\tsolve [expression]\t\tReal roots of expression = 0 for X, one per Root line\n");
    printf("\tsystem [expression]...\t\tEach expression = 0, 2 or 3 of them for X Y (Z), one Set: line per solution\n");
}

/*Trim null terminated string*/
uint8_t *trim(char *input, unsigned *len) {
    unsigned i, trimmed_len = 0, trim_index = 0;
    uint8_t *trimmed;

    for(i = 0; i < strlen(input) + 1; i++) {
        if(input[i] != ' ' && input[i] != '\t')
            trimmed_len++;
    }

    trimmed = malloc(trimmed_len * sizeof(uint8_t));

    for(i = 0; i < strlen(input) + 1; i++) {
        if(input[i] != ' ' && input[i] != '\t')
            trimmed[trim_index++] = (uint8_t)input[i];
    }

    *len = trimmed_len - 1; /*Don't include null byte*/

    return trimmed;
}

int run_gcd(int argc, char **argv) {

    uint8_t *trimmed_a, *trimmed_b;
    unsigned trimmed_a_len, trimmed_b_len;

    pcas_error_t err;
    pcas_ast_t *a, *b, *g;

    uint8_t *output;
    unsigned output_len;

    if(argc <= 3) {
        display_help();
        return -1;
    }

    trimmed_a = trim(argv[2], &trimmed_a_len);

    printf("Parsing \"%s\"\n", trimmed_a);

    a = parse(trimmed_a, trimmed_a_len, str_table, &err);

    printf("%s\n", error_text[err]);

    if(err != E_SUCCESS || a == NULL)
        return -1;

    simplify(a, SIMP_ALL);

    trimmed_b = trim(argv[3], &trimmed_b_len);

    printf("Parsing \"%s\"\n", trimmed_b);

    b = parse(trimmed_b, trimmed_b_len, str_table, &err);

    printf("%s\n", error_text[err]);

    if(err != E_SUCCESS || b == NULL)
        return -1;

    simplify(b, SIMP_ALL);

    printf("Computing gcd...\n\n");

    g = gcd(a, b);

    simplify(g, SIMP_ALL);
    simplify_canonical_form(g, CANONICAL_ALL);

    dbg_print_tree(g, 4);

    printf("\n");

    output = export_to_binary(g, &output_len, str_table, &err);

    if(err == E_SUCCESS && output != NULL) {
        printf("Output: ");
        printf("%.*s\n", output_len, output);

        free(output);
    }

    free(trimmed_a);
    free(trimmed_b);

    ast_Cleanup(a);
    ast_Cleanup(b);
    ast_Cleanup(g);

    return 0;
}
extern bool simplify_periodic(pcas_ast_t *e);
int run_simplify(int argc, char **argv) {

    uint8_t *trimmed;
    unsigned trimmed_len;

    pcas_error_t err;
    pcas_ast_t *e;

    uint8_t *output;
    unsigned output_len;

    if(argc <= 2) {
        display_help();
        return -1;
    }

    trimmed = trim(argv[2], &trimmed_len);

    printf("Parsing \"%s\"\n", trimmed);

    e = parse(trimmed, trimmed_len, str_table, &err);

    printf("%s\n", error_text[err]);

    if(err == E_SUCCESS && e != NULL) {

        printf("\n");
        dbg_print_tree(e, 4);

        printf("\n");

        printf("Simplifying...\n\n");

        simplify(e, SIMP_ALL);
        simplify_canonical_form(e, CANONICAL_ALL);

        if(pcas_oom) { /*Fluxion: oom*/
            printf("Out of memory\n");
            free(trimmed);
            ast_Cleanup(e);
            return 0;
        }

        dbg_print_tree(e, 4);

        printf("\n");

        output = export_to_binary(e, &output_len, str_table, &err);

        if(err == E_SUCCESS && output != NULL) {
            printf("Output: ");
            printf("%.*s\n", output_len, output);

            free(output);
        }

    }

    free(trimmed);
    ast_Cleanup(e);

    return 0;
}

int run_factor(int argc, char **argv) {
uint8_t *trimmed;
    unsigned trimmed_len;

    pcas_error_t err;
    pcas_ast_t *e;

    uint8_t *output;
    unsigned output_len;

    if(argc <= 2) {
        display_help();
        return -1;
    }

    trimmed = trim(argv[2], &trimmed_len);

    printf("Parsing \"%s\"\n", trimmed);

    e = parse(trimmed, trimmed_len, str_table, &err);

    printf("%s\n", error_text[err]);

    if(err == E_SUCCESS && e != NULL) {

        printf("\n");
        dbg_print_tree(e, 4);

        printf("\n");

        printf("Simplifying...\n\n");

        simplify(e, SIMP_ALL);

        dbg_print_tree(e, 4);

        printf("\n");

        printf("Factoring...\n\n");

        /*Fluxion: comdenom / comdenomf share this*/
        if(!strncmp(argv[1], "comdenom", 8))
            comdenom(e, argv[1][8] == 'f');
        else
            factor(e, FAC_ALL);

        /*simplify(e, SIMP_ALL);*/
        simplify_canonical_form(e, CANONICAL_ALL);

        if(pcas_oom) { /*Fluxion: oom*/
            printf("Out of memory\n");
            free(trimmed);
            ast_Cleanup(e);
            return 0;
        }

        dbg_print_tree(e, 4);
        printf("\n");

        output = export_to_binary(e, &output_len, str_table, &err);

        if(err == E_SUCCESS && output != NULL) {
            printf("Output: ");
            printf("%.*s\n", output_len, output);

            free(output);
        }

    }

    free(trimmed);
    ast_Cleanup(e);

    return 0;
}

int run_expand(int argc, char **argv) {

    uint8_t *trimmed;
    unsigned trimmed_len;

    pcas_error_t err;
    pcas_ast_t *e;

    uint8_t *output;
    unsigned output_len;

    if(argc <= 2) {
        display_help();
        return -1;
    }

    trimmed = trim(argv[2], &trimmed_len);

    printf("Parsing \"%s\"\n", trimmed);

    e = parse(trimmed, trimmed_len, str_table, &err);

    printf("%s\n", error_text[err]);

    if(err == E_SUCCESS && e != NULL) {

        printf("\n");
        dbg_print_tree(e, 4);

        printf("\n");

        printf("Simplifying...\n\n");

        simplify(e, SIMP_ALL);

        dbg_print_tree(e, 4);

        printf("\n");

        printf("Expanding...\n\n");

        simplify(e, SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_RATIONAL);
        expand(e, EXP_ALL);
        simplify(e, SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_RATIONAL | SIMP_LIKE_TERMS | SIMP_EVAL);
        simplify_canonical_form(e, CANONICAL_ALL ^ CANONICAL_COMBINE_POWERS);

        if(pcas_oom) { /*Fluxion: oom*/
            printf("Out of memory\n");
            free(trimmed);
            ast_Cleanup(e);
            return 0;
        }

        dbg_print_tree(e, 4);
        printf("\n");

        output = export_to_binary(e, &output_len, str_table, &err);

        if(err == E_SUCCESS && output != NULL) {
            printf("Output: ");
            printf("%.*s\n", output_len, output);

            free(output);
        }

    }

    free(trimmed);
    ast_Cleanup(e);

    return 0;
}

int run_test(int argc, char **argv) {
    unsigned len, i, failed = 0, passed = 0;
    test_t **arr;

    clock_t delta;

    if(argc < 3) {
        display_help();
        return -1;
    }

    arr = test_Load(argv[2], &len);

    if(arr == NULL) {
        printf("Could not load test file.\n");
        return -1;
    }

    printf("Running tests...\n");

    delta = clock();
    for(i = 0; i < len; i++) {
        test_t *t = arr[i];
        printf("Running test %d/%d on line %d... ", i + 1, len, t->line);
        if(!test_Run(t)) {
            puts("[FAIL]");
            failed++;
        } else {
            puts("[OK]");
            passed++;
        }
    }
    delta = clock() - delta;

    test_CleanupArr(arr, len);

    printf("Finished in %li microseconds.\n", delta / (CLOCKS_PER_SEC / 1000));
    printf("Passed: %u/%u, Failed: %u/%u\n", passed, len, failed, len);

    if(failed == 0) {
        printf("All tests passed!\n");
        return 0;
    }

    return -1;
}

/*Fluxion: budget [file]. work = pcas_check calls, CE guess = PC time * 6500*/
#define BUDGET_REPS 5   /*fastest of these, one run is mostly timer noise*/

typedef struct {
    unsigned long work, us;
    const test_t *t;
} budget_row_t;

static int budget_cmp(const void *a, const void *b) {
    unsigned long x = ((const budget_row_t *)a)->work, y = ((const budget_row_t *)b)->work;

    return x < y ? 1 : x > y ? -1 : 0;
}

int run_budget(int argc, char **argv) {
    unsigned len, i, r;
    budget_row_t *rows;
    test_t **arr;

    if(argc < 3) {
        display_help();
        return -1;
    }
    arr = test_Load(argv[2], &len);
    if(arr == NULL) {
        printf("Could not load test file.\n");
        return -1;
    }
    rows = malloc((len + 1) * sizeof *rows); /*+1, an empty file isn't malloc(0)*/
    if(rows == NULL) {
        test_CleanupArr(arr, len);
        return -1;
    }

    for(i = 0; i < len; i++) {
        rows[i].us = (unsigned long)-1;
        for(r = 0; r < BUDGET_REPS; r++) {
            test_t copy = *arr[i]; /*test_Run cuts its strings up*/
            clock_t start;
            unsigned long t;

            pcas_work = 0;
            start = clock();
            test_Run(&copy);
            t = (unsigned long)((clock() - start) * 1000000.0 / CLOCKS_PER_SEC);
            if(t < rows[i].us)
                rows[i].us = t;
        }
        rows[i].work = pcas_work;
        rows[i].t = arr[i];
    }
    qsort(rows, len, sizeof *rows, budget_cmp);

    printf("%5s %9s %9s %9s  %s\n", "line", "work", "pc us", "ce s", "input");
    for(i = 0; i < len; i++)
        printf("%5u %9lu %9lu %9.1f  %.50s\n", rows[i].t->line, rows[i].work, rows[i].us, rows[i].us * 6500.0 / 1e6, rows[i].t->arg1);

    free(rows);
    test_CleanupArr(arr, len);
    return 0;
}

int run_derivative(int argc, char **argv) {

    uint8_t *trimmed;
    unsigned trimmed_len;

    pcas_error_t err;
    pcas_ast_t *e = NULL, *respect_to = NULL, *at = NULL;

    uint8_t *output;
    unsigned output_len;

    if(argc <= 3) {
        display_help();
        return -1;
    }

    trimmed = trim(argv[2], &trimmed_len);
    printf("Parsing \"%s\"\n", trimmed);
    e = parse(trimmed, trimmed_len, str_table, &err);
    printf("%s\n", error_text[err]);
    free(trimmed);

    if(err == E_SUCCESS) {
        trimmed = trim(argv[3], &trimmed_len);
        printf("Parsing \"%s\"\n", trimmed);
        respect_to = parse(trimmed, trimmed_len, str_table, &err);
        printf("%s\n", error_text[err]);
        free(trimmed);
    }

    if(err == E_SUCCESS) {

        if(argc >= 5) {
            trimmed = trim(argv[4], &trimmed_len);
            printf("Parsing \"%s\"\n", trimmed);
            at = parse(trimmed, trimmed_len, str_table, &err);
            printf("%s\n", error_text[err]);
            free(trimmed);
        } else {
            at = ast_Copy(respect_to);
        }

    }

    if(err == E_SUCCESS && (e == NULL || respect_to == NULL || at == NULL) && pcas_oom) /*Fluxion: oom*/
        printf("Out of memory\n");

    if(err == E_SUCCESS && e != NULL && respect_to != NULL && at != NULL) {

        printf("\n");
        dbg_print_tree(e, 4);

        printf("\n");

        printf("Simplifying...\n\n");

        simplify(e, SIMP_ALL);

        dbg_print_tree(e, 4);

        printf("\n");

        printf("Taking derivative...\n");

        derivative(e, respect_to, at);

        printf("Simplifying...\n\n");

        simplify(e, SIMP_ALL);
        simplify_canonical_form(e, CANONICAL_ALL);

        if(pcas_oom) { /*Fluxion: oom*/
            printf("Out of memory\n");
            ast_Cleanup(e);
            ast_Cleanup(respect_to);
            ast_Cleanup(at);
            return 0;
        }

        dbg_print_tree(e, 4);
        printf("\n");

        output = export_to_binary(e, &output_len, str_table, &err);

        if(err == E_SUCCESS && output != NULL) {
            printf("Output: ");
            printf("%.*s\n", output_len, output);

            free(output);
        }

    }

    ast_Cleanup(e);
    ast_Cleanup(respect_to);
    ast_Cleanup(at);

    return 0;
}

/*Fluxion: roots of e = 0 for X, a Root: line each (all / none / unknown otherwise)*/
int run_solve(int argc, char **argv) {
    uint8_t *trimmed, *output;
    unsigned trimmed_len, output_len;
    pcas_error_t err;
    pcas_ast_t *e, *roots[SOLVE_MAX] = {NULL};
    int n, i;

    if(argc <= 2) {
        display_help();
        return -1;
    }

    trimmed = trim(argv[2], &trimmed_len);
    e = parse(trimmed, trimmed_len, str_table, &err);
    printf("%s\n", error_text[err]);
    free(trimmed);
    if(err != E_SUCCESS || e == NULL)
        return -1;

    n = solve(e, SYM_X, roots); /*no simplify first, like the app*/
    if(pcas_oom)
        printf("Out of memory\n");
    else if(n == SOLVE_ALL)
        printf("Root: all\n");
    else if(n == SOLVE_UNKNOWN) {
        double num[8];
        int m = solve_numeric(e, SYM_X, num, 8);

        printf("Root: unknown\n");
        for(i = 0; i < m; i++)
            printf("Numeric: %.7g\n", num[i]);
    }
    else if(n == 0)
        printf("Root: none\n");
    for(i = 0; i < n && !pcas_oom; i++) {
        output = export_to_binary(roots[i], &output_len, str_table, &err);
        if(err == E_SUCCESS && output != NULL) {
            printf("Root: %.*s\n", output_len, output);
            free(output);
        }
    }

    for(i = 0; i < n; i++)
        ast_Cleanup(roots[i]);
    ast_Cleanup(e);
    return 0;
}

/*Fluxion: 2 or 3 expressions, each = 0, for X Y (Z). a Set: line per solution (or System: all / none / unknown)*/
int run_system(int argc, char **argv) {
    static const Symbol vars[3] = { SYM_X, SYM_Y, SYM_Z };
    pcas_ast_t *eqs[SYSTEM_MAX] = {NULL}, *out[SYSTEM_SETS][SYSTEM_MAX] = {{NULL}};
    pcas_error_t err;
    uint8_t *trimmed, *text;
    unsigned trimmed_len, text_len;
    int n = argc - 2, i, k, got = SOLVE_UNKNOWN;
    bool parsed = n == 2 || n == 3;

    if(!parsed) {
        display_help();
        return -1;
    }
    for(i = 0; i < n && parsed; i++) {
        trimmed = trim(argv[2 + i], &trimmed_len);
        eqs[i] = parse(trimmed, trimmed_len, str_table, &err);
        free(trimmed);
        parsed = err == E_SUCCESS && eqs[i] != NULL;
    }
    if(parsed)
        got = solve_system(eqs, n, vars, out);
    if(!parsed)
        printf("%s\n", error_text[err]);
    else if(pcas_oom)
        printf("Out of memory\n");
    else if(got == SOLVE_ALL)
        printf("System: all\n");
    else if(got == SOLVE_UNKNOWN)
        printf("System: unknown\n");
    else if(got == 0)
        printf("System: none\n");
    for(i = 0; i < got && i < SYSTEM_SETS && !pcas_oom; i++) {
        printf("Set:");
        for(k = 0; k < n; k++) {
            simplify(out[i][k], SIMP_NORMALIZE | SIMP_EVAL); /*rational as one number exports as a decmial*/
            text = export_to_binary(out[i][k], &text_len, str_table, &err);
            if(err == E_SUCCESS && text != NULL) {
                printf(" %c=%.*s", 'X' + k, text_len, text);
                free(text);
            }
        }
        printf("\n");
    }

    for(i = 0; i < SYSTEM_SETS; i++)
        for(k = 0; k < SYSTEM_MAX; k++)
            ast_Cleanup(out[i][k]);
    for(i = 0; i < SYSTEM_MAX; i++)
        ast_Cleanup(eqs[i]);
    return parsed ? 0 : -1;
}

int main(int argc, char **argv) {
    int ret;

    if(argc > 1) {
        if(!strcmp(argv[1], "test"))            ret = run_test(argc, argv);
        else if(!strcmp(argv[1], "budget"))     ret = run_budget(argc, argv); /*Fluxion*/
        else if(!strcmp(argv[1], "simplify"))   ret = run_simplify(argc, argv);
        else if(!strcmp(argv[1], "gcd"))        ret = run_gcd(argc, argv);
        else if(!strcmp(argv[1], "factor"))     ret = run_factor(argc, argv);
        else if(!strncmp(argv[1], "comdenom", 8)) ret = run_factor(argc, argv);
        else if(!strcmp(argv[1], "expand"))     ret = run_expand(argc, argv);
        else if(!strcmp(argv[1], "derivative")) ret = run_derivative(argc, argv);
        else if(!strcmp(argv[1], "solve"))      ret = run_solve(argc, argv); /*Fluxion*/
        else if(!strcmp(argv[1], "system"))     ret = run_system(argc, argv); /*Fluxion*/
        else {
            display_help();
            return -1;
        }
        id_UnloadAll();
        return ret;
    }

    display_help();
    return -1;
}

#else
typedef int make_iso_compilers_happy;
#endif