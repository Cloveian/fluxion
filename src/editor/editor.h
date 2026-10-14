#ifndef FLUXION_EDITOR_H
#define FLUXION_EDITOR_H

/*
    the input line: what's been typed so far, as a tree, plus the cursor.
    nspire style (docs/design-choices/input.md): mostly plain typing, templates
    for the 2D things (fraction, exponent, roots, |x|, functions).

    this file knows nothing about keys. the key handling (later) calls these.

    a row is a list of items. an item is a character or a template; templates have
    slots, and each slot is a row. the cursor is a position in one row.
*/

#include <stdbool.h>
#include "render/layout.h"

typedef enum {
    ED_CHAR, /* one character */
    ED_FRAC, /* slot 0 top, slot 1 bottom */
    ED_POW, /* slot 0 is the exponent, base is whatever's right before it */
    ED_SQRT, /* just slot 0 */
    ED_NROOT, /* slot 0 index, slot 1 radicand */
    ED_PARENS, /* slot 0, brackets come in pairs (nspire) */
    ED_ABS,
    ED_FUNC, /* name then slot 0, so sin, asin, ln, log, int ... */
    ED_LOGB /* log_b(x), slot 0 the base and slot 1 the argument */
} ed_kind_t;

/* characters, besides digits, a-z, . , ! = and layout.h's CH_PI / CH_THETA / CH_EULER / CH_IMAG */
#define ED_PLUS     '+'
#define ED_SUB      CH_MINUS  /* a subtract, spaced like + */
#define ED_NEG      '\x07'      /* negate, the (-) key. minus with no space after it */
#define ED_TIMES    '*'         /* shown as a dot */
#define ED_DIVIDE   '/'         /* typed inline (nspire), stacked once it's a result */

typedef struct ed_row ed_row_t;
typedef struct ed_node ed_node_t;

typedef struct {
    ed_row_t *root;
    ed_row_t *row; /* row the cursor sits in */
    int pos; /* index of the item we're before (0 = start of row) */
} editor_t;

bool ed_init(editor_t *ed);
void ed_free(editor_t *ed);
void ed_clear(editor_t *ed);
bool ed_is_empty(const editor_t *ed);

/* typing. return false if out of memory or not allowed here. */
bool ed_insert_char(editor_t *ed, char c);              /* ')' leaves the brackets it's in */
bool ed_insert(editor_t *ed, ed_kind_t kind, const char *name);  /* name: ED_FUNC only */

/* moving */
void ed_left(editor_t *ed);
void ed_right(editor_t *ed);
void ed_up(editor_t *ed);
void ed_down(editor_t *ed);

/*
    deleting, with rules that are always the same (MathPrint's aren't):
      backspace  deletes the character on the left. a template on the left: steps
                 into it. at the start of an empty template: removes it.
      delete     deletes what's on the right, a whole template if it's one.
*/
void ed_backspace(editor_t *ed);
void ed_delete(editor_t *ed);

/* the engine's string syntax (see catalog.c), or NULL if something's unfinished
   (an empty slot, nothing typed). caller frees. */
char *ed_to_engine(const editor_t *ed);

/* when the whole line is a single command (expand, factor, eval, solve), its name. else NULL.
   the app runs those itself; the engine's parser doesn't know them. */
const char *ed_command(const editor_t *ed);

/* ...and what's inside the command, in engine syntax (NULL if unfinished). caller frees. */
char *ed_command_arg(const editor_t *ed);

/* boxes for drawing, measured with font. the cursor is drawn if cursor is true. */
box_t *ed_layout(const editor_t *ed, const layout_font_t *font, bool cursor);

#endif
