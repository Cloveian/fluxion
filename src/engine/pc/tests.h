#ifndef TESTS_H_
#define TESTS_H_

#include <stdio.h>
#include <stdbool.h>

/*Max length for one parameter*/
#define MAX_PAR 256
/*Max length for a line in the file*/
#define MAX_LINE (MAX_PAR * 3)
/*Max tests we will have*/
#define MAX_TESTS 1024

typedef enum {
    TEST_SIMPLIFY,
    TEST_GCD,
    TEST_FACTOR,
    TEST_EXPAND,
    TEST_DERIV,
    TEST_SOLVE,     /*Fluxion: solve; equation; var; roots (a,b,...) or true / false / unknown*/
    TEST_CANCEL,    /*Fluxion: cancel; expression; expected. cancel_fractions(), then simplify like the app*/
    TEST_COMDENOM,  /*Fluxion: comdenom; expression; expected. simplify, then comdenom() expanded*/
    TEST_COMDENOMF, /*Fluxion: comdenomf; expression; expected. the same, factored*/
    TEST_DECIMAL,   /*Fluxion: decimal; expression; text. approx(), then approx_format() to 6 digits*/
    TEST_ASYM,      /*Fluxion: asym; expression; var; holes <list> | vert <list> | horiz <y>, - for none, or unknown*/
    TEST_SYSTEM,    /*Fluxion: system; eq,eq,...; vars; values in var order, sets split by |, or false / many / unknown*/
    TEST_TURN,      /*Fluxion: turn; equation; var; like solve, but the roots from 0 up to 2pi*/
    TEST_CSOLVE,    /*Fluxion: csolve; equation; var; like solve, with the complex roots*/
    TEST_DOMAIN,    /*Fluxion: domain; expression; var; lower <x> | upper <x> | holes <list>, o for open, or empty / unknown*/

    TEST_INVALID
} TestType;

typedef struct {

    TestType type;

    char arg1[MAX_PAR], arg2[MAX_PAR], arg3[MAX_PAR];

    unsigned line;

} test_t;

/*Returns one test from a line*/
test_t *test_Parse(char *line);
/*Returns an array of tests from a file*/
test_t **test_Load(char *file, unsigned *len);

bool test_Run(test_t *t);
void test_Cleanup(test_t *t);
void test_CleanupArr(test_t **arr, unsigned len);
void test_Print(test_t *t);

#endif