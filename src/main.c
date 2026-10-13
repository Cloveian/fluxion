#include <ti/screen.h>
#include <ti/getcsc.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "engine/ast.h"
#include "engine/parser.h"
#include "engine/cas/cas.h"
#include "engine/cas/identities.h"

/* Hello CAS: parse, simplify, print. Placeholder until the editor and renderer exist. */
static void simplify_and_print(const char *input)
{
    pcas_error_t err;
    pcas_ast_t *e;
    uint8_t *output;
    unsigned output_len;
    clock_t start = clock();
    char timing[24];

    os_PutStrFull(input);
    os_NewLine();

    e = parse((const uint8_t *)input, strlen(input), str_table, &err);
    if (err != E_SUCCESS || e == NULL) {
        os_PutStrFull(error_text[err]);
        os_NewLine();
        return;
    }

    simplify(e, SIMP_ALL);
    simplify_canonical_form(e, CANONICAL_ALL);

    output = export_to_binary(e, &output_len, str_table, &err);
    if (err == E_SUCCESS && output != NULL) {
        char line[64];
        unsigned len = output_len < sizeof line - 1 ? output_len : sizeof line - 1;

        memcpy(line, output, len);
        line[len] = '\0';
        os_PutStrFull("= ");
        os_PutStrFull(line);
        free(output);
    } else {
        os_PutStrFull(error_text[err]);
    }
    ast_Cleanup(e);

    /* Temporary: how long this took, to find where the startup time goes */
    sprintf(timing, " %lums", (unsigned long)((clock() - start) * 1000 / CLOCKS_PER_SEC));
    os_PutStrFull(timing);
    os_NewLine();
}

int main(void)
{
    os_ClrHome();

    simplify_and_print("2X+X+X*X");
    simplify_and_print("(5/13)/(5/12)");
    simplify_and_print("sin(X)^2+cos(X)^2");
    simplify_and_print("2X+X+X*X");

    while (!os_GetCSC());

    id_UnloadAll();
    return 0;
}
