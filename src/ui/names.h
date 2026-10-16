#ifndef FLUXION_NAMES_H
#define FLUXION_NAMES_H

/*
    names: what a run of typed letters can mean. the built-in words come from the
    catalog (sin, pi, solve, ...) and the stored ones from [sto->]. a run like
    "sinh" gets cut into the longest names from the left, on every edit, so the
    letters stay letters in the editor and this is just how they're read and
    drawn. bold = it has a value (e, i, anything stored), plain = a variable.
    design in docs/design-choices/input.md, Names.
*/

#include "editor/editor.h"

#define NAMES_MAX 32 /* stored at once. its RAM, not forever */
#define NAME_LEN  8 /* letters, same as a TI variable (and an appvar, later) */

/* plug the editor's lookup in. call once at startup */
void names_init(void);

/* the stored ones */
int names_count(void);
const char *names_name(int i);
const ed_row_t *names_row(int i);
int names_find(const char *name, int n); /* -1 if there's no such name */
bool names_delete(int i);

/* a whole input line with a sto arrow in it: whats left of the arrow gets stored
   under the letters right of it, nothing left of it deletes the name. NULL if it
   went fine, else the message to show. *deleted says which happened */
const char *names_store_line(const ed_row_t *line, int arrow, bool *deleted);

/* does this one-letter variable have a value, for "x is stored" in solve */
bool names_is_stored_letter(char c);

#endif
