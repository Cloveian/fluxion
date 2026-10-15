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

/* copies. mark is a row in the source; *marked gets its copy (how the undo steps
   know where the cursor was) */

static ed_row_t *row_copy(const ed_row_t *src, int from, int to, ed_node_t *parent, int slot,
                          const ed_row_t *mark, ed_row_t **marked);

static ed_node_t *node_copy(const ed_node_t *src, const ed_row_t *mark, ed_row_t **marked) {
    ed_node_t *n = calloc(1, sizeof(ed_node_t));
    int i;

    if(n == NULL)
        return NULL;
    n->kind = src->kind;
    n->ch = src->ch;
    memcpy(n->name, src->name, sizeof n->name);

    for(i = 0; i < src->slots; i++) {
        n->slot[i] = row_copy(src->slot[i], 0, src->slot[i]->len, n, i, mark, marked);
        if(n->slot[i] == NULL) {
            node_free(n);
            return NULL;
        }
        n->slots++;     /* only counts the slots that made it, for node_free */
    }
    return n;
}

static ed_row_t *row_copy(const ed_row_t *src, int from, int to, ed_node_t *parent, int slot,
                          const ed_row_t *mark, ed_row_t **marked) {
    ed_row_t *r = row_new(parent, slot);
    int i;

    if(r == NULL)
        return NULL;
    if(src == mark)
        *marked = r;

    for(i = from; i < to; i++) {
        ed_node_t *n = node_copy(src->items[i], mark, marked);

        if(n == NULL || !row_insert(r, r->len, n)) {
            if(n != NULL)
                node_free(n);
            row_free(r);
            return NULL;
        }
    }
    return r;
}

/* the editor itself */

bool ed_init(editor_t *ed) {
    ed->root = row_new(NULL, 0);
    ed->row = ed->root;
    ed->pos = 0;
    ed->anchor = -1;
    ed->undo_len = 0;
    return ed->root != NULL;
}

void ed_free(editor_t *ed) {
    row_free(ed->root);
    ed->root = ed->row = NULL;
    while(ed->undo_len > 0)
        row_free(ed->undo[--ed->undo_len].root);
}

void ed_clear(editor_t *ed) {
    ed_row_t *root = row_new(NULL, 0);

    if(root == NULL)
        return;
    row_free(ed->root);
    ed->root = ed->row = root;
    ed->pos = 0;
    ed->anchor = -1;
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

/* the selection is items [sel_start, sel_end) of ed->row */
static int sel_start(const editor_t *ed) {
    return ed->anchor < ed->pos ? ed->anchor : ed->pos;
}

static int sel_end(const editor_t *ed) {
    return ed->anchor > ed->pos ? ed->anchor : ed->pos;
}

/* typing */

bool ed_insert_char(editor_t *ed, char c) {
    ed_node_t *n;

    if(c == '(')
        return ed_insert(ed, ED_PARENS, NULL);

    if(c == ')') {
        /* brackets come in pairs: ) just steps out of the brackets your in */
        ed_row_t *r;

        ed_deselect(ed, false);
        for(r = ed->row; r->parent != NULL; r = r->parent->in) {
            if(r->parent->kind == ED_PARENS || r->parent->kind == ED_FUNC || r->parent->kind == ED_LOGB) {
                after(ed, r->parent);
                return true;
            }
        }
        return false;
    }

    /* capitals stay capitals on screen but they're the same variable (nspire is
       case insensitive too), see put_char */
    ed_erase_selection(ed);
    n = node_new(ED_CHAR, c, NULL);
    if(n == NULL || !row_insert(ed->row, ed->pos, n)) {
        free(n);
        return false;
    }
    ed->pos++;
    return true;
}

/* the selection moves into n's main slot (the bottom of a root, the argument of
   log_b), then the cursor goes to the first slot still empty or after n */
static void wrap(editor_t *ed, ed_node_t *n) {
    ed_row_t *into = n->slot[n->kind == ED_NROOT || n->kind == ED_LOGB ? 1 : 0];
    int from = sel_start(ed), to = sel_end(ed), i;

    for(i = from; i < to; i++)
        row_insert(into, into->len, row_remove(ed->row, from));  /* can't fail, see below */
    ed->pos = from;
    ed->anchor = -1;

    for(i = 0; i < n->slots; i++) {
        if(n->slot[i]->len == 0) {
            ed->row = n->slot[i];
            ed->pos = 0;
            return;
        }
    }
}

bool ed_insert(editor_t *ed, ed_kind_t kind, const char *name) {
    ed_node_t *n;

    if(kind == ED_CHAR)
        return false;

    /* x+1 selected, then ^: (x+1)^ */
    if(kind == ED_POW && ed_has_selection(ed) && sel_end(ed) - sel_start(ed) > 1) {
        if(!ed_insert(ed, ED_PARENS, NULL))
            return false;
    }
    if(kind == ED_POW)
        ed_deselect(ed, false);

    n = node_new(kind, 0, name);
    if(n == NULL)
        return false;

    if(ed_has_selection(ed)) {
        /* the wrapped slot needs room for every selected item first, so wrap()
           can't run out of memory halfway */
        ed_row_t *into = n->slot[kind == ED_NROOT || kind == ED_LOGB ? 1 : 0];
        int need = sel_end(ed) - sel_start(ed);
        ed_node_t **items = malloc(need * sizeof(ed_node_t *));

        if(items == NULL || !row_insert(ed->row, sel_end(ed), n)) {
            free(items);
            node_free(n);
            return false;
        }
        into->items = items;
        into->cap = need;
        wrap(ed, n);
        if(ed->row == n->in)
            after(ed, n);
        return true;
    }

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

    ed->anchor = -1;
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

    ed->anchor = -1;
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

bool ed_down(editor_t *ed) {
    ed_row_t *r;

    ed->anchor = -1;
    for(r = ed->row; r->parent != NULL; r = r->parent->in) {
        ed_node_t *p = r->parent;

        /* top of a fraction -> bottom, root index -> radicand */
        if((p->kind == ED_FRAC || p->kind == ED_NROOT) && r->parent_slot == 0) {
            ed->pos = clamp(r == ed->row ? ed->pos : p->slot[1]->len, p->slot[1]);
            ed->row = p->slot[1];
            return true;
        }

        /* out of an exponent, back down to its base line */
        if(p->kind == ED_POW) {
            after(ed, p);
            return true;
        }
    }
    return false;
}

bool ed_up(editor_t *ed) {
    ed_row_t *r;

    ed->anchor = -1;

    /* just after an exponent, back up into it */
    if(ed->pos > 0 && ed->row->items[ed->pos - 1]->kind == ED_POW) {
        enter_last(ed, ed->row->items[ed->pos - 1]);
        return true;
    }

    for(r = ed->row; r->parent != NULL; r = r->parent->in) {
        ed_node_t *p = r->parent;

        /* bottom of a fraction -> top, radicand -> root index */
        if((p->kind == ED_FRAC || p->kind == ED_NROOT) && r->parent_slot == 1) {
            ed->pos = clamp(r == ed->row ? ed->pos : p->slot[0]->len, p->slot[0]);
            ed->row = p->slot[0];
            return true;
        }
    }
    return false;
}

/* [2nd] + left/right, like the TI's own line editing */
void ed_home(editor_t *ed) {
    ed->anchor = -1;
    ed->row = ed->root;
    ed->pos = 0;
}

void ed_end(editor_t *ed) {
    ed->anchor = -1;
    ed->row = ed->root;
    ed->pos = ed->root->len;
}

/* --- selecting --- */

bool ed_has_selection(const editor_t *ed) {
    return ed->anchor >= 0 && ed->anchor != ed->pos;
}

/* at the edge of a slot the whole template it's in gets selected instead */
static void select_parent(editor_t *ed, bool leftwards) {
    ed_node_t *p = ed->row->parent;
    int at;

    if(p == NULL)
        return;
    at = index_of(p);
    ed->row = p->in;
    ed->pos = leftwards ? at : at + 1;
    ed->anchor = leftwards ? at + 1 : at;
}

void ed_select_left(editor_t *ed) {
    if(ed->anchor < 0)
        ed->anchor = ed->pos;
    if(ed->pos > 0)
        ed->pos--;
    else
        select_parent(ed, true);
}

void ed_select_right(editor_t *ed) {
    if(ed->anchor < 0)
        ed->anchor = ed->pos;
    if(ed->pos < ed->row->len)
        ed->pos++;
    else
        select_parent(ed, false);
}

void ed_deselect(editor_t *ed, bool to_start) {
    if(ed_has_selection(ed))
        ed->pos = to_start ? sel_start(ed) : sel_end(ed);
    ed->anchor = -1;
}

void ed_erase_selection(editor_t *ed) {
    int from, to;

    if(!ed_has_selection(ed)) {
        ed->anchor = -1;
        return;
    }
    from = sel_start(ed);
    to = sel_end(ed);
    while(to > from)
        node_free(row_remove(ed->row, --to));
    ed->pos = from;
    ed->anchor = -1;
}

/* copy paste */

void ed_row_free(ed_row_t *r) {
    row_free(r);
}

ed_row_t *ed_copy(const editor_t *ed) {
    if(!ed_has_selection(ed))
        return NULL;
    return row_copy(ed->row, sel_start(ed), sel_end(ed), NULL, 0, NULL, NULL);
}

ed_row_t *ed_row_copy(const ed_row_t *r) {
    return r != NULL ? row_copy(r, 0, r->len, NULL, 0, NULL, NULL) : NULL;
}

ed_row_t *ed_copy_line(const editor_t *ed) {
    if(ed->root->len == 0)
        return NULL;
    return row_copy(ed->root, 0, ed->root->len, NULL, 0, NULL, NULL);
}

bool ed_paste(editor_t *ed, const ed_row_t *clip) {
    int i;

    if(clip == NULL)
        return false;
    ed_erase_selection(ed);
    for(i = 0; i < clip->len; i++) {
        ed_node_t *n = node_copy(clip->items[i], NULL, NULL);

        if(n == NULL || !row_insert(ed->row, ed->pos, n)) {
            if(n != NULL)
                node_free(n);
            return false;
        }
        ed->pos++;
    }
    return true;
}

/*
    boxes back into editor rows, so an answer in the history can be pasted and
    picked apart. the boxes are what's on screen, already in display order, and
    nearly every kind is one of ours (frac, sup, root, parens, abs). the only
    guessing: a name before brackets is a function, and a minus with a space
    before it is a subtract
*/

static bool from_box(ed_row_t *r, const box_t *b);

static bool append(ed_row_t *r, ed_node_t *n) {
    if(n == NULL)
        return false;
    if(!row_insert(r, r->len, n)) {
        node_free(n);
        return false;
    }
    return true;
}

static bool from_children(ed_row_t *r, const box_t *k) {
    for(; k != NULL; k = k->next)
        if(!from_box(r, k))
            return false;
    return true;
}

static bool is_name(const box_t *b) {
    const char *s;

    if(b == NULL || b->kind != BOX_TEXT || b->text == NULL || b->text[0] == '\0' || b->text[1] == '\0')
        return false;
    for(s = b->text; *s != '\0'; s++)
        if(*s < 'a' || *s > 'z')
            return false;
    return true;
}

/* what comes before a function's brackets (sin, sin^-1, log_b, d/dx). NULL if k
   isn't one. the base of log_b goes in here too */
static ed_node_t *func_head(const box_t *k) {
    char name[8];

    if(is_name(k))
        return node_new(ED_FUNC, 0, k->text);

    if(k->kind == BOX_SUP && is_name(k->child) && strlen(k->child->text) < sizeof name - 1) {
        name[0] = 'a';
        strcpy(name + 1, k->child->text);
        return node_new(ED_FUNC, 0, name);
    }

    if(k->kind == BOX_SUB && is_name(k->child)) {
        ed_node_t *n = node_new(ED_LOGB, 0, NULL);

        if(n != NULL && !from_box(n->slot[0], k->child->next)) {
            node_free(n);
            return NULL;
        }
        return n;
    }

    if(k->kind == BOX_FRAC && k->child->kind == BOX_TEXT && strcmp(k->child->text, "d") == 0)
        return node_new(ED_FUNC, 0, "deriv");
    return NULL;
}

static bool from_text(ed_row_t *r, const box_t *b, bool after_space) {
    const char *s;

    for(s = b->text; *s != '\0'; s++) {
        char c = *s;

        if(c == CH_MINUS && !after_space)
            c = ED_NEG;
        else if(c == CH_CDOT)
            c = ED_TIMES;
        if(!append(r, node_new(ED_CHAR, c, NULL)))
            return false;
    }
    return true;
}

static bool from_box(ed_row_t *r, const box_t *b) {
    const box_t *k;
    ed_node_t *n;

    if(b == NULL)
        return true;

    switch(b->kind) {
    case BOX_ROW:
    case BOX_SELECT: {
        const box_t *prev = NULL;

        for(k = b->child; k != NULL; prev = k, k = k->next) {
            const box_t *arg = k->next;

            /* a function and its brackets (or ln|x|, the bars are its brackets) */
            if(arg != NULL && (arg->kind == BOX_PARENS || arg->kind == BOX_ABS) &&
               (n = func_head(k)) != NULL) {
                ed_row_t *slot = n->slot[n->kind == ED_LOGB ? 1 : 0];

                if(!append(r, n))
                    return false;
                if(!(arg->kind == BOX_PARENS ? from_children(slot, arg->child) : from_box(slot, arg)))
                    return false;
                k = arg;
                continue;
            }

            if(k->kind == BOX_TEXT) {
                if(!from_text(r, k, prev != NULL && prev->kind == BOX_SPACE))
                    return false;
            } else if(!from_box(r, k)) {
                return false;
            }
        }
        return true;
    }

    case BOX_TEXT:
        return from_text(r, b, false);

    case BOX_FRAC:
        n = node_new(ED_FRAC, 0, NULL);
        return append(r, n) && from_box(n->slot[0], b->child) && from_box(n->slot[1], b->child->next);

    case BOX_SUP:
        /* the base is just whatever's before the exponent */
        if(!from_box(r, b->child))
            return false;
        n = node_new(ED_POW, 0, NULL);
        return append(r, n) && from_box(n->slot[0], b->child->next);

    case BOX_ROOT:
        if(b->child->next == NULL) {
            n = node_new(ED_SQRT, 0, NULL);
            return append(r, n) && from_box(n->slot[0], b->child);
        }
        n = node_new(ED_NROOT, 0, NULL);
        return append(r, n) && from_box(n->slot[0], b->child->next) && from_box(n->slot[1], b->child);

    case BOX_PARENS:
    case BOX_ABS:
        n = node_new(b->kind == BOX_PARENS ? ED_PARENS : ED_ABS, 0, NULL);
        return append(r, n) && from_children(n->slot[0], b->child);

    case BOX_SUB:
        return from_children(r, b->child);

    default:
        return true;    /* spaces, cursor, empty slots */
    }
}

bool ed_load(editor_t *ed, const ed_row_t *row, bool at_end) {
    ed_row_t *copy = row_copy(row, 0, row->len, NULL, 0, NULL, NULL);

    if(copy == NULL)
        return false;
    ed_free(ed);
    ed_init(ed);
    row_free(ed->root);
    ed->root = ed->row = copy;
    ed->pos = at_end ? copy->len : 0;
    return true;
}

ed_row_t *ed_from_box(const box_t *b) {
    ed_row_t *r = row_new(NULL, 0);

    if(r != NULL && !from_box(r, b)) {
        row_free(r);
        return NULL;
    }
    return r;
}

/* undo */

void ed_checkpoint(editor_t *ed) {
    ed_state_t s;

    s.row = NULL;
    s.root = row_copy(ed->root, 0, ed->root->len, NULL, 0, ed->row, &s.row);
    s.pos = ed->pos;
    if(s.root == NULL)
        return;     /* no memory, that step just can't be undone */

    if(ed->undo_len == ED_UNDO_MAX) {
        row_free(ed->undo[0].root);
        memmove(&ed->undo[0], &ed->undo[1], (ED_UNDO_MAX - 1) * sizeof(ed_state_t));
        ed->undo_len--;
    }
    ed->undo[ed->undo_len++] = s;
}

bool ed_undo(editor_t *ed) {
    ed_state_t *s;

    if(ed->undo_len == 0)
        return false;
    s = &ed->undo[--ed->undo_len];
    row_free(ed->root);
    ed->root = s->root;
    ed->row = s->row;
    ed->pos = s->pos;
    ed->anchor = -1;
    return true;
}

/* deleting (the fiddly bit) */

/* room for extra more items, so moving nodes in can't fail halfway (a node that's
   been taken out and can't go back in would just be lost) */
static bool row_reserve(ed_row_t *r, int extra) {
    ed_node_t **items;

    if(r->len + extra <= r->cap)
        return true;
    items = realloc(r->items, (r->len + extra) * sizeof(ed_node_t *));
    if(items == NULL)
        return false;
    r->items = items;
    r->cap = r->len + extra;
    return true;
}

/* moves everything in from into to at pos, returns how many */
static int move_items(ed_row_t *from, ed_row_t *to, int pos) {
    int n = 0;

    while(from->len > 0)
        row_insert(to, pos + n++, row_remove(from, 0));
    return n;
}

/* half of a fraction going inline: x+1 needs brackets to stay the top, x doesn't */
static int move_part(ed_row_t *from, ed_row_t *to, int pos) {
    ed_node_t *parens;

    if(from->len > 1 && (parens = node_new(ED_PARENS, 0, NULL)) != NULL) {
        if(row_reserve(parens->slot[0], from->len)) {
            move_items(from, parens->slot[0], 0);
            row_insert(to, pos, parens);
            return 1;
        }
        node_free(parens);
    }
    return move_items(from, to, pos);
}

/*
    takes template n out and leaves what was in it, cursor where n started.
    sin(x+1) -> x+1, (a)/(b) -> a/b, x^(2) -> x2. roots and log_b keep the main
    part, the index / base goes (theres nowhere sensible to put it ugh)
*/
static void unwrap(editor_t *ed, ed_node_t *n) {
    ed_row_t *in = n->in, *main = n->slot[n->kind == ED_NROOT || n->kind == ED_LOGB ? 1 : 0];
    int at = index_of(n), pos = at;
    ed_node_t *bar = NULL;

    if(n->kind == ED_FRAC && n->slot[1]->len > 0)
        bar = node_new(ED_CHAR, ED_DIVIDE, NULL);
    if(!row_reserve(in, n->slot[0]->len + (n->slots > 1 ? n->slot[1]->len : 0) + 1)) {
        if(bar != NULL)
            node_free(bar);
        return;     /* out of memory, leave it be */
    }

    row_remove(in, at);
    if(n->kind == ED_FRAC) {
        pos += move_part(n->slot[0], in, pos);
        if(bar != NULL) {
            row_insert(in, pos++, bar);
            pos += move_part(n->slot[1], in, pos);
        }
    } else {
        move_items(main, in, pos);
    }
    node_free(n);

    ed->row = in;
    ed->pos = at;
    ed->anchor = -1;
}

void ed_backspace(editor_t *ed) {
    ed_node_t *p;

    if(ed_has_selection(ed)) {
        ed_erase_selection(ed);
        return;
    }
    ed->anchor = -1;
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
        unwrap(ed, p);  /* start of sin(x+1), so the sin( goes and x+1 stays */
    }
}

void ed_delete(editor_t *ed) {
    if(ed_has_selection(ed)) {
        ed_erase_selection(ed);
        return;
    }
    ed->anchor = -1;
    if(ed->pos >= ed->row->len)
        return;
    if(ed->row->items[ed->pos]->slots > 0 && !all_empty(ed->row->items[ed->pos]))
        unwrap(ed, ed->row->items[ed->pos]);
    else
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
    case ED_SCI:    put(o, "[E]");   return;
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
    case '+': case ED_SUB: case '=': case '<': case '>': case CH_LE: case CH_GE: case CH_NE:
        return spaced(s, level);
    case ED_NEG:
        s[0] = CH_MINUS;
        break;
    case ED_TIMES:
        s[0] = CH_CDOT;
        break;
    case ED_SCI:
        s[0] = 'E';
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
        box_t *item;

        if(here && l->ed->pos == i)
            layout_add(row, layout_box(BOX_CURSOR, level));
        item = lay_node(l, r->items[i], row, level);

        /* each selected item gets its own highlight, so an exponent still finds
           its base as the last box in the row */
        if(here && ed_has_selection(l->ed) && i >= sel_start(l->ed) && i < sel_end(l->ed)) {
            box_t *sel = layout_box(BOX_SELECT, level);

            layout_add(sel, item);
            item = sel;
        }
        layout_add(row, item);
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
