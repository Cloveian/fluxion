#include "editor.h"

#include <stdlib.h>
#include <string.h>

struct ed_node {
    ed_kind_t kind;
    char ch; /* ED_CHAR */
    char name[8];
    int slots;
    ed_row_t *slot[2];
    ed_row_t *in;   /* the row this node lives in */
};

struct ed_row {
    ed_node_t **items;
    int len, cap;
    ed_node_t *parent;  /* template this row is a slot of, NULL at the root */
    int parent_slot;
};

/* rows and nodes */

static int slot_count(ed_kind_t kind) {
    switch(kind) {
    case ED_CHAR:
        return 0;
    case ED_FRAC: case ED_NROOT: case ED_LOGB:
        return 2;
    default:
        return 1;
    }
}

static ed_row_t *row_new(ed_node_t *parent, int slot) {
    ed_row_t *r = calloc(1, sizeof(ed_row_t));

    if(r != NULL) {
        r->parent = parent;
        r->parent_slot = slot;
    }
    return r;
}

static void row_free(ed_row_t *r);

static void node_free(ed_node_t *n) {
    int i;

    for(i = 0; i < n->slots; i++)
        row_free(n->slot[i]);
    free(n);
}

static void row_free(ed_row_t *r) {
    int i;

    if(r == NULL)
        return;
    for(i = 0; i < r->len; i++)
        node_free(r->items[i]);
    free(r->items);
    free(r);
}

static ed_node_t *node_new(ed_kind_t kind, char ch, const char *name) {
    ed_node_t *n = calloc(1, sizeof(ed_node_t));
    int i;

    if(n == NULL)
        return NULL;

    n->kind = kind;
    n->ch = ch;
    if(name != NULL)
        strncpy(n->name, name, sizeof n->name - 1);

    n->slots = slot_count(kind);
    for(i = 0; i < n->slots; i++) {
        n->slot[i] = row_new(n, i);
        if(n->slot[i] == NULL) {
            node_free(n);
            return NULL;
        }
    }
    return n;
}

static bool row_insert(ed_row_t *r, int pos, ed_node_t *n) {
    if(r->len == r->cap) {
        int cap = r->cap ? r->cap * 2 : 8;
        ed_node_t **items = realloc(r->items, cap * sizeof(ed_node_t *));

        if(items == NULL)
            return false;
        r->items = items;
        r->cap = cap;
    }

    memmove(&r->items[pos + 1], &r->items[pos], (r->len - pos) * sizeof(ed_node_t *));
    r->items[pos] = n;
    r->len++;
    n->in = r;
    return true;
}

static ed_node_t *row_remove(ed_row_t *r, int pos) {
    ed_node_t *n = r->items[pos];

    memmove(&r->items[pos], &r->items[pos + 1], (r->len - pos - 1) * sizeof(ed_node_t *));
    r->len--;
    return n;
}

static int index_of(const ed_node_t *n) {
    int i;

    for(i = 0; i < n->in->len; i++)
        if(n->in->items[i] == n)
            return i;
    return -1;
}

static bool all_empty(const ed_node_t *n) {
    int i;

    for(i = 0; i < n->slots; i++)
        if(n->slot[i]->len > 0)
            return false;
    return true;
}

/* the editor itself */

bool ed_init(editor_t *ed) {
    ed->root = row_new(NULL, 0);
    ed->row = ed->root;
    ed->pos = 0;
    return ed->root != NULL;
}

void ed_free(editor_t *ed) {
    row_free(ed->root);
    ed->root = ed->row = NULL;
}

void ed_clear(editor_t *ed) {
    ed_free(ed);
    ed_init(ed);
}

bool ed_is_empty(const editor_t *ed) {
    return ed->root->len == 0;
}

static void enter_first(editor_t *ed, ed_node_t *n) {
    ed->row = n->slot[0];
    ed->pos = 0;
}

static void enter_last(editor_t *ed, ed_node_t *n) {
    ed->row = n->slot[n->slots - 1];
    ed->pos = ed->row->len;
}

static void before(editor_t *ed, ed_node_t *n) {
    ed->row = n->in;
    ed->pos = index_of(n);
}

static void after(editor_t *ed, ed_node_t *n) {
    ed->row = n->in;
    ed->pos = index_of(n) + 1;
}

/* typing */

bool ed_insert_char(editor_t *ed, char c) {
    ed_node_t *n;

    if(c == '(')
        return ed_insert(ed, ED_PARENS, NULL);

    if(c == ')') {
        /* brackets come in pairs: ) just steps out of the brackets your in */
        ed_row_t *r;

        for(r = ed->row; r->parent != NULL; r = r->parent->in) {
            if(r->parent->kind == ED_PARENS || r->parent->kind == ED_FUNC || r->parent->kind == ED_LOGB) {
                after(ed, r->parent);
                return true;
            }
        }
        return false;
    }

    /* there's only one x, and it's lowercase */
    if(c >= 'A' && c <= 'Z')
        c = (char)(c - 'A' + 'a');

    n = node_new(ED_CHAR, c, NULL);
    if(n == NULL || !row_insert(ed->row, ed->pos, n)) {
        free(n);
        return false;
    }
    ed->pos++;
    return true;
}

bool ed_insert(editor_t *ed, ed_kind_t kind, const char *name) {
    ed_node_t *n;

    if(kind == ED_CHAR)
        return false;

    n = node_new(kind, 0, name);
    if(n == NULL)
        return false;
    if(!row_insert(ed->row, ed->pos, n)) {
        node_free(n);
        return false;
    }

    enter_first(ed, n);
    return true;
}

/* -- moving -- */

void ed_right(editor_t *ed) {
    ed_node_t *p;

    if(ed->pos < ed->row->len) {
        ed_node_t *n = ed->row->items[ed->pos];

        if(n->slots > 0)
            enter_first(ed, n);
        else
            ed->pos++;
        return;
    }

    /* at the end of a slot, next slot or out past the template */
    p = ed->row->parent;
    if(p == NULL)
        return;
    if(ed->row->parent_slot + 1 < p->slots) {
        ed->row = p->slot[ed->row->parent_slot + 1];
        ed->pos = 0;
    } else {
        after(ed, p);
    }
}

void ed_left(editor_t *ed) {
    ed_node_t *p;

    if(ed->pos > 0) {
        ed_node_t *n = ed->row->items[ed->pos - 1];

        if(n->slots > 0)
            enter_last(ed, n);
        else
            ed->pos--;
        return;
    }

    /* at the start of a slot, previous slot or out before the template */
    p = ed->row->parent;
    if(p == NULL)
        return;
    if(ed->row->parent_slot > 0) {
        ed->row = p->slot[ed->row->parent_slot - 1];
        ed->pos = ed->row->len;
    } else {
        before(ed, p);
    }
}

static int clamp(int pos, const ed_row_t *r) {
    return pos < r->len ? pos : r->len;
}

void ed_down(editor_t *ed) {
    ed_row_t *r;

    for(r = ed->row; r->parent != NULL; r = r->parent->in) {
        ed_node_t *p = r->parent;

        /* top of a fraction -> bottom, root index -> radicand */
        if((p->kind == ED_FRAC || p->kind == ED_NROOT) && r->parent_slot == 0) {
            ed->pos = clamp(r == ed->row ? ed->pos : p->slot[1]->len, p->slot[1]);
            ed->row = p->slot[1];
            return;
        }

        /* out of an exponent, back down to its base line */
        if(p->kind == ED_POW) {
            after(ed, p);
            return;
        }
    }
}

void ed_up(editor_t *ed) {
    ed_row_t *r;

    /* just after an exponent, back up into it */
    if(ed->pos > 0 && ed->row->items[ed->pos - 1]->kind == ED_POW) {
        enter_last(ed, ed->row->items[ed->pos - 1]);
        return;
    }

    for(r = ed->row; r->parent != NULL; r = r->parent->in) {
        ed_node_t *p = r->parent;

        /* bottom of a fraction -> top, radicand -> root index */
        if((p->kind == ED_FRAC || p->kind == ED_NROOT) && r->parent_slot == 1) {
            ed->pos = clamp(r == ed->row ? ed->pos : p->slot[0]->len, p->slot[0]);
            ed->row = p->slot[0];
            return;
        }
    }
}

/* deleting (the fiddly bit) */

void ed_backspace(editor_t *ed) {
    ed_node_t *p;

    if(ed->pos > 0) {
        ed_node_t *n = ed->row->items[ed->pos - 1];

        if(n->slots > 0) {
            enter_last(ed, n);  /* walk into it */
        } else {
            node_free(row_remove(ed->row, ed->pos - 1));
            ed->pos--;
        }
        return;
    }

    p = ed->row->parent;
    if(p == NULL)
        return;

    if(all_empty(p)) {
        ed_row_t *in = p->in;
        int at = index_of(p);

        node_free(row_remove(in, at));
        ed->row = in;
        ed->pos = at;
    } else if(ed->row->parent_slot > 0) {
        ed->row = p->slot[ed->row->parent_slot - 1];
        ed->pos = ed->row->len;
    } else {
        before(ed, p);
    }
}

void ed_delete(editor_t *ed) {
    if(ed->pos < ed->row->len)
        node_free(row_remove(ed->row, ed->pos));
}

/* turning the tree into an engine string */

typedef struct {
    char *s;
    int len, cap;
    bool bad;       /* empty slot, exponent with no base, out of memory */
} out_t;

static void put(out_t *o, const char *text) {
    int n = (int)strlen(text);

    if(o->bad)
        return;
    if(o->len + n + 1 > o->cap) {
        int cap = (o->len + n + 1) * 2;
        char *s = realloc(o->s, cap);

        if(s == NULL) {
            o->bad = true;
            return;
        }
        o->s = s;
        o->cap = cap;
    }
    memcpy(o->s + o->len, text, n + 1);
    o->len += n;
}

static void put_row(out_t *o, const ed_row_t *r);

static void put_char(out_t *o, char c) {
    char s[2];

    switch(c) {
    case ED_SUB:    put(o, "_");     return;
    case ED_NEG:    put(o, "-");     return;
    case CH_PI:     put(o, "pi");    return;
    case CH_THETA:  put(o, "theta"); return;
    case CH_EULER:  put(o, "e");     return;
    case CH_IMAG:   put(o, "i");     return;
    default:
        break;
    }

    /* variables are capitals for the engine */
    s[0] = (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
    s[1] = '\0';
    put(o, s);
}

static void put_node(out_t *o, const ed_node_t *n, bool first) {
    switch(n->kind) {
    case ED_CHAR:
        put_char(o, n->ch);
        break;
    case ED_FRAC:
        put(o, "(("); put_row(o, n->slot[0]); put(o, ")/("); put_row(o, n->slot[1]); put(o, "))");
        break;
    case ED_POW:
        if(first)
            o->bad = true;          /* nothing to raise */
        put(o, "^("); put_row(o, n->slot[0]); put(o, ")");
        break;
    case ED_SQRT:
        put(o, "sqrt("); put_row(o, n->slot[0]); put(o, ")");
        break;
    case ED_NROOT:
        put(o, "(("); put_row(o, n->slot[0]); put(o, ")root("); put_row(o, n->slot[1]); put(o, "))");
        break;
    case ED_PARENS:
        put(o, "("); put_row(o, n->slot[0]); put(o, ")");
        break;
    case ED_ABS:
        put(o, "abs("); put_row(o, n->slot[0]); put(o, ")");
        break;
    case ED_FUNC:
        if(strcmp(n->name, "deriv") == 0) {
            /* d/dx, with respect to x, evaluated at x */
            put(o, "deriv("); put_row(o, n->slot[0]); put(o, ",X,X)");
        } else {
            put(o, n->name); put(o, "("); put_row(o, n->slot[0]); put(o, ")");
        }
        break;
    case ED_LOGB:
        put(o, "logb("); put_row(o, n->slot[1]); put(o, ","); put_row(o, n->slot[0]); put(o, ")");
        break;
    }
}

static void put_row(out_t *o, const ed_row_t *r) {
    int i;

    if(r->len == 0)
        o->bad = true;
    for(i = 0; i < r->len; i++)
        put_node(o, r->items[i], i == 0);
}

char *ed_to_engine(const editor_t *ed) {
    out_t o = { NULL, 0, 0, false };

    put_row(&o, ed->root);
    if(o.bad) {
        free(o.s);
        return NULL;
    }
    return o.s;
}

static const char *const commands[] = { "expand", "factor", "eval", "solve" };

const char *ed_command(const editor_t *ed) {
    const ed_node_t *n;
    unsigned i;

    if(ed->root->len != 1 || ed->root->items[0]->kind != ED_FUNC)
        return NULL;

    n = ed->root->items[0];
    for(i = 0; i < sizeof commands / sizeof commands[0]; i++)
        if(strcmp(n->name, commands[i]) == 0)
            return commands[i];
    return NULL;
}

char *ed_command_arg(const editor_t *ed) {
    out_t o = { NULL, 0, 0, false };

    if(ed_command(ed) == NULL)
        return NULL;

    put_row(&o, ed->root->items[0]->slot[0]);
    if(o.bad) {
        free(o.s);
        return NULL;
    }
    return o.s;
}

/* drawing */

typedef struct {
    const editor_t *ed;
    bool cursor;
    int frac_depth;
} lay_t;

static box_t *lay_row(lay_t *l, const ed_row_t *r, unsigned char level);

/* takes the last child off a box (the base for an exponent) */
static box_t *pop_last(box_t *row) {
    box_t **p, *last;

    if(row == NULL || row->child == NULL)
        return NULL;
    for(p = &row->child; (*p)->next != NULL; p = &(*p)->next);
    last = *p;
    *p = NULL;
    return last;
}

static box_t *pair(box_kind_t kind, box_t *a, box_t *b, unsigned char level) {
    box_t *box = layout_box(kind, level);

    layout_add(box, a);
    layout_add(box, b);
    return box;
}

static box_t *spaced(const char *text, unsigned char level) {
    box_t *row = layout_box(BOX_ROW, level);

    layout_add(row, layout_space(LAYOUT_SPACE(level)));
    layout_add(row, layout_text(text, level));
    layout_add(row, layout_space(LAYOUT_SPACE(level)));
    return row;
}

static box_t *lay_char(char c, unsigned char level) {
    char s[2] = { c, '\0' };

    switch(c) {
    case '+': case ED_SUB: case '=':
        return spaced(s, level);
    case ED_NEG:
        s[0] = CH_MINUS;
        break;
    case ED_TIMES:
        s[0] = CH_CDOT;
        break;
    default:
        break;
    }
    return layout_text(s, level);
}

static box_t *lay_func(lay_t *l, const ed_node_t *n, unsigned char level) {
    static const char *const inverses[] = { "asin", "acos", "atan", "asinh", "acosh", "atanh" };
    box_t *row = layout_box(BOX_ROW, level), *head;
    unsigned i;

    if(strcmp(n->name, "deriv") == 0) {
        layout_add(row, pair(BOX_FRAC, layout_text("d", level), layout_text("dx", level), level));
        layout_add(row, pair(BOX_PARENS, lay_row(l, n->slot[0], level), NULL, level));
        return row;
    }

    /* asin -> sin^-1 */
    head = layout_text(n->name, level);
    for(i = 0; i < sizeof inverses / sizeof inverses[0]; i++) {
        if(strcmp(n->name, inverses[i]) == 0) {
            char minus_one[3] = { CH_MINUS, '1', '\0' };

            layout_free(head);
            head = pair(BOX_SUP, layout_text(n->name + 1, level), layout_text(minus_one, level + 1), level);
            break;
        }
    }

    layout_add(row, head);
    layout_add(row, pair(BOX_PARENS, lay_row(l, n->slot[0], level), NULL, level));
    return row;
}

static box_t *lay_node(lay_t *l, const ed_node_t *n, box_t *row, unsigned char level) {
    switch(n->kind) {
    case ED_CHAR:
        return lay_char(n->ch, level);

    case ED_FRAC: {
        unsigned char inner = l->frac_depth > 0 ? level + 1 : level;
        box_t *num, *den;

        l->frac_depth++;
        num = lay_row(l, n->slot[0], inner);
        den = lay_row(l, n->slot[1], inner);
        l->frac_depth--;
        return pair(BOX_FRAC, num, den, inner);
    }

    case ED_POW: {
        /* the base is whatever came just before. if the cursor sits between them,
           it goes inside the base, so it shows up in the right place. */
        box_t *base = pop_last(row), *exp = lay_row(l, n->slot[0], level + 1);

        if(base != NULL && base->kind == BOX_CURSOR) {
            box_t *prev = pop_last(row), *with = layout_box(BOX_ROW, level);

            layout_add(with, prev);
            layout_add(with, base);
            base = with;
        }
        if(base == NULL)
            base = layout_box(BOX_ROW, level);
        return pair(BOX_SUP, base, exp, level);
    }

    case ED_SQRT:
        return pair(BOX_ROOT, lay_row(l, n->slot[0], level), NULL, level);

    case ED_NROOT:
        return pair(BOX_ROOT, lay_row(l, n->slot[1], level), lay_row(l, n->slot[0], level + 1), level);

    case ED_PARENS:
        return pair(BOX_PARENS, lay_row(l, n->slot[0], level), NULL, level);

    case ED_ABS:
        return pair(BOX_ABS, lay_row(l, n->slot[0], level), NULL, level);

    case ED_FUNC:
        return lay_func(l, n, level);

    case ED_LOGB: {
        box_t *out = layout_box(BOX_ROW, level);

        layout_add(out, pair(BOX_SUB, layout_text("log", level), lay_row(l, n->slot[0], level + 1), level));
        layout_add(out, pair(BOX_PARENS, lay_row(l, n->slot[1], level), NULL, level));
        return out;
    }
    }
    return NULL;
}

static box_t *lay_row(lay_t *l, const ed_row_t *r, unsigned char level) {
    box_t *row = layout_box(BOX_ROW, level);
    bool here = l->cursor && l->ed->row == r;
    int i;

    for(i = 0; i < r->len; i++) {
        if(here && l->ed->pos == i)
            layout_add(row, layout_box(BOX_CURSOR, level));
        layout_add(row, lay_node(l, r->items[i], row, level));
    }

    if(here && l->ed->pos == r->len)
        layout_add(row, layout_box(BOX_CURSOR, level));

    /* an empty slot shows a box to fill (not the empty input line itself) */
    if(r->len == 0 && r->parent != NULL)
        layout_add(row, layout_box(BOX_SLOT, level));

    return row;
}

box_t *ed_layout(const editor_t *ed, const layout_font_t *font, bool cursor) {
    lay_t l = { ed, cursor, 0 };
    box_t *b = lay_row(&l, ed->root, 0);

    if(b != NULL)
        layout_finish(b, font);
    return b;
}
