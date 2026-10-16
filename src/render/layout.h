#ifndef FLUXION_LAYOUT_H
#define FLUXION_LAYOUT_H

/*
    box layout: turns an engine expression tree into nested boxes with sizes and
    positions, ready to draw. no drawing and no graphx here, so it runs on the PC
    too. see docs/design-choices/rendering.md ("box thing").

    every box has an origin on its baseline, at its left edge:

            ascent  ┐  above the baseline
        ────────────┼── baseline (y = 0)
            descent ┘  below the baseline

    y grows downwards, like the screen. a child's (x, y) is its origin relative to
    its parent's origin.
*/

#include "engine/ast.h"

/*
    characters that aren't plain ASCII. they go in BOX_TEXT strings like any other
    character, and the font layer (calculator or PC) decides how to draw them.
*/
#define CH_PI       '\x01'
#define CH_THETA    '\x02'
#define CH_MINUS    '\x03'      /* a real minus, wider than a hyphen */
#define CH_CDOT     '\x04'      /* multiplication dot, only between two numbers */
#define CH_EULER    '\x05'      /* the constant e, bold so it isn't the variable e */
#define CH_IMAG     '\x06'      /* the constant i, bold so it isn't the variable i */

/*
    spacing, in pixels. copied from TeX's habits: room around + and -, none for
    implied multiplication.
*/
#define SPACE_BINARY    3       /* each side of + and - */
#define FRAC_GAP        1       /* between the bar and the numerator / denominator */
#define FRAC_OVERHANG   1       /* the bar sticks out this far past the wider part */
#define FRAC_OVERHANG_OUTER 4   /* ...or this far if there's a fraction inside, so the main bar stands out */
#define FRAC_GAP_OUTER  2       /* ...with this much room above and below it */
#define SUP_MIN_RAISE   3       /* an exponent's baseline is at least this far up */
#define SUP_GAP         1       /* between the base and its exponent */
#define SUB_DROP        3       /* a subscript's baseline is this far down */
#define ROOT_SIGN_W     8       /* the √ sign incl. 2 px of room before the radicand */
#define ROOT_GAP        2       /* between the radicand's top and the bar */
#define ROOT_TAIL       2       /* the bar goes this far past the radicand */
#define ROOT_INDEX_W    3       /* how far the index may overlap the sign's tick */
#define PAREN_W         5       /* each bracket, including its gap */
#define ABS_W           3       /* each |bar|, including its gap */
#define COL_GAP         2 /* between stacked lines */

typedef enum {
    BOX_TEXT, /* text - numbers, variables, operators, function names */
    BOX_SPACE, /* empty, a fixed width */
    BOX_ROW, /* children side by side on one baseline */
    BOX_COL, /* children stacked top to bottom, right-aligned. origin is the last line's baseline */
    BOX_FRAC, /* child is the numerator, ->next the denominator, aux the bar row */
    BOX_SUP, /* base, then ->next exponent, raised */
    BOX_SUB, /* child = base, child->next = subscript, lowered (log base) */
    BOX_ROOT, /* radicand, ->next the index (none for sqrt), aux the bar row */
    BOX_PARENS, /* content, brackets grown to its height */
    BOX_ABS, /* content, |bars| as tall as it */
    BOX_CURSOR, /* the editor's cursor, no width and a line's height */
    BOX_SLOT, /* an empty slot in a template, drawn as a box */
    BOX_SELECT /* a row, highlighted (the editor's selection) */
} box_kind_t;

typedef struct box {
    box_kind_t kind;

    /*
        rows a box covers: from y - ascent (top) to y + descent - 1 (bottom).
        the baseline row is the first row of the descent.
    */
    int x, y; /* origin, relative to the parent's */
    int width, ascent, descent;
    int aux; /* BOX_FRAC/BOX_ROOT, the bar's row relative to the origin */

    unsigned char level; /* nesting. 0 normal, 1 exponent, 2 an exponent's exponent... */
    unsigned char size; /* the font size it ended up at (see layout_font_t), 0 = smallest */
    unsigned char style; /* font_style_t bits from draw.h, only BOX_TEXT reads it. 0 = normal */
    char *text;

    struct box *child; /* first child */
    struct box *next; /* the next sibling */
} box_t;

/*
    what the layout needs to know about the fonts: a ladder of sizes, smallest first.

    every level of nesting (exponents, root indexes, fractions inside fractions) is
    one size smaller than the level around it. normal math starts at `base`. if an
    expression nests deeper than there are sizes below base, its whole ladder moves
    up instead (bigger base), so the deepest part still lands on size 0, never below.
*/
#define LAYOUT_MAX_SIZES 8

typedef struct {
    int (*text_width)(const char *text, unsigned char size, unsigned char style); /* style: a draw.h font_style_t, bold is wider */
    unsigned char sizes; /* how many sizes there are, 0 being smallest */
    unsigned char base; /* the normal-math size, nothing nested */
    unsigned char ascent[LAYOUT_MAX_SIZES], descent[LAYOUT_MAX_SIZES];
} layout_font_t;

/* builds and measures the boxes for e. returns NULL if out of memory. */
box_t *layout_build(pcas_ast_t *e, const layout_font_t *font);

/* the decimal form: numbers that aren't whole (and a/b of two numbers) come out as
   1.25 / 0.333333 instead of fractions. stays on until it's turned off */
void layout_decimal(bool on);

void layout_free(box_t *b);

/*
    for building boxes from something other than an engine tree (the editor).
    make the tree, then layout_finish() picks font sizes and measures it.
    layout_add() takes ownership of child; NULL children are ignored.
*/
box_t *layout_box(box_kind_t kind, unsigned char level);
box_t *layout_text(const char *text, unsigned char level);
box_t *layout_space(int width);
void layout_add(box_t *parent, box_t *child);
void layout_finish(box_t *root, const layout_font_t *font);

/* room each side of + and -, so the editor spaces them like layout_build does */
#define LAYOUT_SPACE(level) ((level) == 0 ? SPACE_BINARY : 1)

#endif
