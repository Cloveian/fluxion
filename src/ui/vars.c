#include "vars.h"

#include <stdlib.h>
#include <string.h>

#include "page.h"
#include "names.h"

#define VALUE_MAX 20    /* chars, so a long value doesn't run into the name */

static char value_buf[VALUE_MAX + 2];

/* nothing stored still gets one row, a blank page looks broken */
static bool empty(void) {
    return names_count() == 0;
}

static const char *vars_label(int i) {
    return empty() ? "nothing stored yet" : names_name(i);
}

static const char *vars_value(int i) {
    char *s;
    int k, n = 0;

    if(empty())
        return NULL;
    s = ed_row_to_engine(names_row(i));
    if(s == NULL)
        return "?";

    /* engine syntax back into about what was typed. one past the max is
       enough to know it got cut */
    for(k = 0; s[k] != '\0' && n <= VALUE_MAX; k++) {
        char c = s[k];

        if(c == '*')
            continue;
        if(c == '_')
            c = '-';
        else if(c >= 'A' && c <= 'Z')
            c = (char)(c - 'A' + 'a');
        value_buf[n++] = c;
    }
    if(n > VALUE_MAX) {
        memcpy(value_buf + VALUE_MAX - 3, "...", 3);
        n = VALUE_MAX;
    }
    value_buf[n] = '\0';
    free(s);
    return value_buf;
}

static const char *vars_help(int i) {
    (void)i;
    if(empty())
        return "type a value, then [sto->] and a name, like 2x+1 -> ab";
    return "[enter] deletes it. values are live, change what one uses and it follows";
}

static bool vars_pick(int i);

static page_t vars_page = { "stored", 0, vars_label, vars_value, vars_help, vars_pick, NULL };

static bool vars_pick(int i) {
    if(empty())
        return true;
    names_delete(i);
    if(empty())
        return true;    /* that was the last one, nothing left to look at */
    vars_page.count = names_count();  /* page.c pulls sel back if it fell off */
    return false;
}

void vars_open(void) {
    vars_page.count = empty() ? 1 : names_count();
    page_open(&vars_page);
}
