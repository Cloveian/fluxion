#ifndef FLUXION_CATALOG_H
#define FLUXION_CATALOG_H

/*
    everything you can type or do in Fluxion, in one place: what it is, which key
    types it, which soft-key folder it lives in, what it becomes for the engine,
    and whether the engine can actually do it yet. the menus and the editor are
    built from this, so adding something means adding it here.

    see docs/design-choices/input.md (nspire style) and ui.md (the folders).
*/

/* can the engine do it? */
typedef enum {
    HAVE_YES, /* works today */
    HAVE_PARTLY, /* some of it works, see the note */
    HAVE_NO /* not yet, hybrid engine work (the docs show a *) */
} have_t;

typedef enum {
    ITEM_CHAR, /* one thing in the line - a digit, letter, operator, constant */
    ITEM_TEMPLATE, /* a 2D shape with slots, like a fraction, exponent, root, |x| */
    ITEM_FUNCTION, /* name then a slot, sin, ln, ... */
    ITEM_COMMAND, /* something you do to an expression, expand, solve, ... */
    ITEM_SETTING /* Tools folder */
} item_kind_t;

typedef struct {
    const char *label; /* how it looks in a menu */
    item_kind_t kind;
    const char *keys; /* TI keys that type it (a proposal), NULL means folder-only */
    const char *engine; /* engine string syntax, $ is a slot to fill */
    have_t have;
    const char *note;
} item_t;

typedef struct {
    const char *key; /* the soft key under the screen */
    const char *name;
    const item_t *const *items;
    unsigned count;
} folder_t;

/* every item, in typing order (what the editor has to accept) */
extern const item_t *const catalog[];
extern const unsigned catalog_count;

/* the five soft-key folders, left to right */
#define FOLDER_COUNT 5
extern const folder_t folders[FOLDER_COUNT];

#endif
