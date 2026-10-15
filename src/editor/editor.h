#ifndef FLUXION_EDITOR_H
#define FLUXION_EDITOR_H

/*
    the input line: what's been typed so far, as a tree, plus the cursor.
    nspire style (docs/design-choices/input.md): mostly plain typing, templates
    for the 2D things (fraction, exponent, roots, |x|, functions).

    this file knows nothing about keys. the key handling in app.c calls these.

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
#define ED_SCI      '\x08'      /* the E in 2E3, [2nd][,]. drawn as a capital E, the engine gets [E] */

/* more special characters, past ED_NEG. font codes like layout.h's, see tools/bdf2c.py */
#define CH_LE       '\x10'
#define CH_GE       '\x11'
#define CH_NE       '\x12'
#define CH_INF      '\x13'
#define CH_DEG      '\x14'

typedef struct ed_row ed_row_t;
typedef struct ed_node ed_node_t;

/* undo keeps whole copies of the line, so not too many (they're ~1 KB each on a long line) */
#define ED_UNDO_MAX 16

typedef struct {
    ed_row_t *root, *row;
    int pos;
} ed_state_t;

typedef struct {
    ed_row_t *root;
    ed_row_t *row; /* row the cursor sits in */
    int pos; /* index of the item we're before (0 = start of row) */
    int anchor; /* other end of the selection, in the same row. -1 when nothing's selected */

    ed_state_t undo[ED_UNDO_MAX];
    int undo_len;
} editor_t;

bool ed_init(editor_t *ed);
void ed_free(editor_t *ed);
void ed_clear(editor_t *ed); /* empties the line, keeps the undo steps */
bool ed_is_empty(const editor_t *ed);

/* typing. return false if out of memory or not allowed here. */
bool ed_insert_char(editor_t *ed, char c);              /* ')' leaves the brackets it's in */
bool ed_insert(editor_t *ed, ed_kind_t kind, const char *name);  /* name: ED_FUNC only */

/* moving. these drop the selection. up/down return false if there was nowhere to go
   (the app goes into the history then) */
void ed_left(editor_t *ed);
void ed_right(editor_t *ed);
bool ed_up(editor_t *ed);
bool ed_down(editor_t *ed);
void ed_home(editor_t *ed);
void ed_end(editor_t *ed);

/*
    selecting, whole items at a time (never goes into a template). at the edge of a
    slot it grows to the whole template, like the nspire. typing or pasting replaces
    the selection; a template (brackets, fraction, root, function) goes around it.
*/
void ed_select_left(editor_t *ed);
void ed_select_right(editor_t *ed);
bool ed_has_selection(const editor_t *ed);
void ed_deselect(editor_t *ed, bool to_start); /* cursor goes to the start or the end of it */
void ed_erase_selection(editor_t *ed);

/* copies of rows, for the clipboard and the history. NULL if there's nothing or no memory */
ed_row_t *ed_copy(const editor_t *ed); /* the selection */
ed_row_t *ed_copy_line(const editor_t *ed);
ed_row_t *ed_row_copy(const ed_row_t *r);
bool ed_paste(editor_t *ed, const ed_row_t *clip);
void ed_row_free(ed_row_t *r);

/* an answer's boxes (layout_build) back into a row, for the history */
ed_row_t *ed_from_box(const box_t *b);

/* puts a copy of row in ed, for browsing the history (nobody edits it, promise) */
bool ed_load(editor_t *ed, const ed_row_t *row, bool at_end);

/* undo. the app saves a step before a key changes the line */
void ed_checkpoint(editor_t *ed);
bool ed_undo(editor_t *ed);

/*
    deleting, with rules that are always the same (MathPrint's aren't):
      backspace  deletes the character on the left. a template on the left: steps
                 into it. at the start of a template: removes it, but what was in
                 it stays (sin(x+1) -> x+1).
      delete     deletes what's on the right. a template keeps its insides the
                 same way.
*/
void ed_backspace(editor_t *ed);
void ed_delete(editor_t *ed);

/* the engine's string syntax (see catalog.c), or NULL if something's unfinished
   (an empty slot, nothing typed). caller frees. */
char *ed_to_engine(const editor_t *ed);

/* when the whole line is a single command (expand, factor, eval, solve), its name. else NULL.
   the app runs those itself; the engine's parser doesn't know them. */
const char *ed_command(const editor_t *ed);

/* the list ed_command goes by, so there's one place to add a command. is name
   one, and does s (engine syntax) have one( anywhere in it */
bool ed_is_command(const char *name);
bool ed_has_command(const char *s);

/* ...and what's inside the command, in engine syntax (NULL if unfinished). caller frees. */
char *ed_command_arg(const editor_t *ed);

/* boxes for drawing, measured with font. the cursor is drawn if cursor is true. */
box_t *ed_layout(const editor_t *ed, const layout_font_t *font, bool cursor);

#endif
