#include "names.h"
#include "catalog.h"

#include <stdlib.h>
#include <string.h>

/* the special characters a word is drawn as */
static const char SHOW_PI[]    = { CH_PI, '\0' };
static const char SHOW_THETA[] = { CH_THETA, '\0' };
static const char SHOW_E[]     = { CH_EULER, '\0' };
static const char SHOW_I[]     = { CH_IMAG, '\0' };

typedef struct {
    char name[NAME_LEN + 1];
    ed_row_t *row;
} var_t;

static var_t vars[NAMES_MAX];
static int count;

int names_count(void) {
    return count;
}

const char *names_name(int i) {
    return vars[i].name;
}

const ed_row_t *names_row(int i) {
    return vars[i].row;
}

int names_find(const char *name, int n) {
    int i;

    for(i = 0; i < count; i++)
        if((int)strlen(vars[i].name) == n && strncmp(vars[i].name, name, (size_t)n) == 0)
            return i;
    return -1;
}

bool names_delete(int i) {
    if(i < 0 || i >= count)
        return false;
    ed_row_free(vars[i].row);
    memmove(&vars[i], &vars[i + 1], (size_t)(count - i - 1) * sizeof(var_t));
    count--;
    return true;
}

bool names_is_stored_letter(char c) {
    if(c >= 'A' && c <= 'Z')
        c = (char)(c - 'A' + 'a');
    return names_find(&c, 1) >= 0;
}

/* the longest built-in word that starts the run, NULL if none */
static const item_t *builtin(const char *s, int n, int *len) {
    const item_t *best = NULL;
    unsigned k;

    *len = 0;
    for(k = 0; k < catalog_count; k++) {
        const item_t *it = catalog[k];
        int wl = it->word != NULL ? (int)strlen(it->word) : 0;

        if(wl > *len && wl <= n && strncmp(it->word, s, (size_t)wl) == 0) {
            best = it;
            *len = wl;
        }
    }
    return best;
}

/* same for the stored ones, -1 if none */
static int stored(const char *s, int n, int *len) {
    int i, best = -1;

    *len = 0;
    for(i = 0; i < count; i++) {
        int wl = (int)strlen(vars[i].name);

        if(wl > *len && wl <= n && strncmp(vars[i].name, s, (size_t)wl) == 0) {
            best = i;
            *len = wl;
        }
    }
    return best;
}

/* the editor's hook. a stored name can't be a built-in word (storing one is
   refused) so whichever is longer wins, no ties */
static bool lookup(const char *s, int n, ed_name_t *out) {
    int bl, sl;
    const item_t *it = builtin(s, n, &bl);
    int st = stored(s, n, &sl);

    memset(out, 0, sizeof *out);
    if(st >= 0 && sl >= bl) {
        out->len = sl;
        out->row = vars[st].row;
        out->bold = true;
        return true;
    }
    if(it == NULL)
        return false;

    out->len = bl;
    out->engine = it->word;
    if(strcmp(it->word, "pi") == 0)
        out->shown = SHOW_PI;
    else if(strcmp(it->word, "theta") == 0)
        out->shown = SHOW_THETA;
    else if(strcmp(it->word, "e") == 0)
        out->shown = SHOW_E; /* that glyph's already bold, thats the whole point of it */
    else if(strcmp(it->word, "i") == 0)
        out->shown = SHOW_I;
    return true;
}

void names_init(void) {
    ed_name_lookup = lookup;
}

/* storing */

static char message[64];

static const char *about(const char *name, const char *what) {
    strcpy(message, name);
    strcat(message, what);
    return message;
}

const char *names_store_line(const ed_row_t *line, int arrow, bool *deleted) {
    char name[NAME_LEN + 1];
    int n, i, bl;
    ed_row_t *value;

    *deleted = false;

    n = ed_row_word(line, arrow + 1, name, NAME_LEN);
    if(n < 0)
        return "a name is just letters, up to 8 of them";
    if(n == 0)
        return "nothing to store it as";
    if(builtin(name, n, &bl) != NULL && bl == n)
        return about(name, " is built in, pick another");

    i = names_find(name, n);

    /* nothing left of the arrow means delete */
    if(arrow == 0) {
        if(i < 0)
            return about(name, " isn't stored");
        names_delete(i);
        *deleted = true;
        return NULL;
    }

    if(i < 0 && count == NAMES_MAX)
        return "32 names stored, delete one first";

    value = ed_row_slice(line, 0, arrow);
    if(value == NULL)
        return "out of memory";

    /* a+1 -> a, or a -> b -> a: it'd loop forever at the next keypress */
    if(i >= 0 && ed_row_reaches(value, vars[i].row)) {
        ed_row_free(value);
        return about(name, " would loop");
    }

    if(i < 0) {
        i = count++;
        strcpy(vars[i].name, name);
        vars[i].row = NULL;
    }
    ed_row_free(vars[i].row);
    vars[i].row = value;
    return NULL;
}
