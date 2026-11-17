#include "args.h"
#include "settings.h"

#include <string.h>

#include "engine/parser.h"
#include "engine/cas/cas.h"
#include "render/layout.h"

int args_split(char *src, char **piece, int max) {
    char *s;
    int depth = 0, pieces = 1;

    piece[0] = src;
    for(s = src; *s != '\0'; s++) {
        if(*s == '(')
            depth++;
        else if(*s == ')')
            depth--;
        else if(depth == 0 && *s == ',') {
            if(pieces == max)
                return 0;
            *s = '\0';
            piece[pieces++] = s + 1;
        }
    }
    return pieces;
}

pcas_ast_t *args_parse(const char *s, pcas_error_t *err) {
    pcas_ast_t *e;

    *err = E_SUCCESS;
    e = parse((const uint8_t *)s, strlen(s), str_table, err);
    if(*err != E_SUCCESS || e == NULL) {
        ast_Cleanup(e);
        if(*err == E_SUCCESS)
            *err = E_GENERIC;
        return NULL;
    }
    return e;
}

pcas_ast_t *args_parse_deg(const char *s, pcas_error_t *err) {
    pcas_ast_t *e = args_parse(s, err);

    if(e != NULL && setting_angle == ANGLE_DEGREES)
        e = degrees_in(e);
    return e;
}

int args_read(char *src, pcas_ast_t **args, int max, pcas_error_t *err) {
    char *piece[ARGS_LIMIT];
    int n, i;

    n = args_split(src, piece, max);
    for(i = 0; i < n; i++) {
        if((args[i] = args_parse(piece[i], err)) == NULL) {
            while(i > 0)
                ast_Cleanup(args[--i]);
            return -1;
        }
    }
    return n;
}

bool args_var(const char *s, Symbol *var, char *shown) {
    if(strcmp(s, "theta") == 0) {
        *var = SYM_THETA;
        if(shown != NULL)
            *shown = CH_THETA;
        return true;
    }
    if(s[0] >= 'A' && s[0] <= 'Z' && s[1] == '\0') {
        *var = (Symbol)s[0];
        if(shown != NULL)
            *shown = (char)(s[0] - 'A' + 'a');
        return true;
    }
    return false;
}
