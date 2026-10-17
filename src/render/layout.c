#include "layout.h"

#include <string.h>

#include "engine/cas/cas.h"

/*
    two passes:
      build()   engine tree -> box tree (structure only)
      measure() sizes every box bottom-up and places its children
*/

static const char STR_MINUS[] = { CH_MINUS, '\0' };
static const char STR_CDOT[]  = { CH_CDOT, '\0' };

static bool oom;    /* set when malloc fails; layout_build() then gives up */
static int frac_depth;
static bool decimal;    /* the decimal form, numbers come out as 0.333333 */

void layout_decimal(bool on) {
    decimal = on;
}

/* making boxes */

static box_t *new_box(box_kind_t kind, unsigned char level) {
    box_t *b = calloc(1, sizeof(box_t));

    if(b == NULL) {
        oom = true;
        return NULL;
    }

    b->kind = kind;
    b->level = level;
    return b;
}

static box_t *new_text(const char *text, unsigned char level) {
    box_t *b = new_box(BOX_TEXT, level);

    if(b == NULL)
        return NULL;

    b->text = malloc(strlen(text) + 1);
    if(b->text == NULL) {
        free(b);
        oom = true;
        return NULL;
    }

    strcpy(b->text, text);
    return b;
}

static box_t *new_space(int width) {
    box_t *b = new_box(BOX_SPACE, 0);

    if(b != NULL)
        b->width = width;
    return b;
}

/* either can be NULL after an out-of-memory. */
static void add(box_t *parent, box_t *child) {
    box_t **p;

    if(child == NULL)
        return;

    if(parent == NULL) {
        layout_free(child);
        return;
    }

    for(p = &parent->child; *p != NULL; p = &(*p)->next);
    *p = child;
}

static box_t *wrap(box_kind_t kind, box_t *content, unsigned char level) {
    box_t *b = new_box(kind, level);

    add(b, content);
    return b;
}

static box_t *pair(box_kind_t kind, box_t *first, box_t *second, unsigned char level) {
    box_t *b = new_box(kind, level);

    add(b, first);
    add(b, second);
    return b;
}

/* numbers + symbols */

static bool is_negative_number(pcas_ast_t *e) {
    return e->type == NODE_NUMBER && mp_rat_compare_zero(e->op.num) < 0;
}

/* digits of z without its sign. caller frees. */
static char *digits(mp_int z) {
    int len = mp_int_string_len(z, RADIX);
    char *s = malloc(len);

    if(s == NULL) {
        oom = true;
        return NULL;
    }

    if(mp_int_to_string(z, RADIX, s, len) != MP_OK) {
        free(s);
        oom = true;
        return NULL;
    }

    if(s[0] == '-')
        memmove(s, s + 1, strlen(s));

    return s;
}

static box_t *build_number(pcas_ast_t *e, unsigned char level) {
    mp_rat r = e->op.num;
    box_t *b;

    if(decimal && !mp_rat_is_integer(r)) {
        char s[24]; /* approx_format writes its own minus, so bail before the one below adds a second */

        approx_format(r, 6, s);
        return new_text(s, level);
    }

    if(mp_rat_is_integer(r)) {
        char *s;

        mp_rat_reduce(r);
        s = digits(mp_rat_numer_ref(r));
        if(s == NULL)
            return NULL;

        b = new_text(s, level);
        free(s);
    } else {
        /* canonical form usally makes fractions DIV nodes, but just in case */
        char *num = digits(mp_rat_numer_ref(r));
        char *den = digits(mp_rat_denom_ref(r));

        b = (num && den) ? pair(BOX_FRAC, new_text(num, level), new_text(den, level), level) : NULL;
        free(num);
        free(den);
    }

    if(mp_rat_compare_zero(r) < 0) {
        box_t *row = new_box(BOX_ROW, level);
        add(row, new_text(STR_MINUS, level));
        add(row, b);
        b = row;
    }

    return b;
}

static box_t *build_symbol(pcas_ast_t *e, unsigned char level) {
    char s[2];

    switch(e->op.symbol) {
    case SYM_PI:    s[0] = CH_PI;    break;
    case SYM_THETA: s[0] = CH_THETA; break;
    case SYM_EULER: s[0] = CH_EULER; break;
    case SYM_IMAG:  s[0] = CH_IMAG;  break;
    default:
        /* the engine's variables are A-Z, shown lowercase like the nspire.
           there's only one x and it's lowercase, sorry X */
        s[0] = (char)(e->op.symbol - 'A' + 'a');
        break;
    }
    s[1] = '\0';

    return new_text(s, level);
}

/* when brackets and dots are needed */

static bool parens_as_factor(pcas_ast_t *e) {
    return isoptype(e, OP_ADD);
}

/* as the base of a power or before !: anything that isn't a single symbol,
number or function call. (X+1)^2, (2X)^2, (1/2)^2, (-3)^2 */
static bool parens_as_base(pcas_ast_t *e) {
    if(e->type == NODE_OPERATOR) {
        switch(optype(e)) {
        case OP_ADD: case OP_MULT: case OP_DIV: case OP_POW: case OP_ROOT: case OP_FACTORIAL:
            return true;
        default:
            return false;
        }
    }

    return is_negative_number(e) || (e->type == NODE_NUMBER && !mp_rat_is_integer(e->op.num));
}

/* what's drawn first / last on the baseline */
static pcas_ast_t *first_drawn(pcas_ast_t *e) {
    while(isoptype(e, OP_POW) || isoptype(e, OP_FACTORIAL))
        e = ast_ChildGet(e, 0);
    return e;
}

static pcas_ast_t *last_drawn(pcas_ast_t *e) {
    while(isoptype(e, OP_POW))
        e = ast_ChildGetLast(e);
    return e;
}

/* a dot between two factors only when it'd otherwise read as one number: 2·3, 2·3^X, 3·(1/2). fiddly af */
/* number? i hardly know 'er bwaHAHHa */
static bool needs_dot(pcas_ast_t *factor, pcas_ast_t *next) {
    pcas_ast_t *last = last_drawn(factor), *first = first_drawn(next);

    if(parens_as_factor(factor) || parens_as_factor(next))
        return false;

    if(last->type != NODE_NUMBER || is_ast_int(last, -1))
        return false;

    return first->type == NODE_NUMBER || isoptype(next, OP_DIV);
}

/* --- operators --- */

static box_t *build(pcas_ast_t *e, unsigned char level);

static box_t *build_parens(pcas_ast_t *e, unsigned char level, bool parens) {
    box_t *b = build(e, level);
    return parens ? wrap(BOX_PARENS, b, level) : b;
}

/* total degree, for ordering terms. x^3 -> 3, 2xy -> 2, 7 -> 0, x^2/(x-3) -> 1 */
static int degree(pcas_ast_t *e) {
    pcas_ast_t *k;
    int d = 0;

    switch(e->type) {
    case NODE_SYMBOL:
        /* pi, e, i, theta are constants */
        return e->op.symbol < SYM_IMAG ? 1 : 0;
    case NODE_NUMBER:
        return 0;
    case NODE_OPERATOR:
        break;
    }

    switch(optype(e)) {
    case OP_MULT:
        for(k = ast_ChildGet(e, 0); k != NULL; k = k->next)
            d += degree(k);
        return d;
    case OP_ADD:
        for(k = ast_ChildGet(e, 0); k != NULL; k = k->next)
            if(degree(k) > d)
                d = degree(k);
        return d;
    case OP_DIV:
        return degree(ast_ChildGet(e, 0)) - degree(ast_ChildGet(e, 1));
    case OP_POW: {
        pcas_ast_t *exp = ast_ChildGet(e, 1);
        int base = degree(ast_ChildGet(e, 0));

        mp_small num, den;

        if(exp->type == NODE_NUMBER && mp_rat_to_ints(exp->op.num, &num, &den) == MP_OK && den == 1)
            return base * (int)num;
        return base;
    }
    default:
        /* sin(x), ln(x), ...: like a constant, after the polynomial part */
        return 0;
    }
}

static int power_of(pcas_ast_t *e, Symbol s) {
    pcas_ast_t *k;
    int p = 0;

    if(e->type == NODE_SYMBOL)
        return e->op.symbol == s;

    if(isoptype(e, OP_MULT)) {
        for(k = ast_ChildGet(e, 0); k != NULL; k = k->next)
            p += power_of(k, s);
        return p;
    }

    /* (x+y) is as much x as its most-x term */
    if(isoptype(e, OP_ADD)) {
        for(k = ast_ChildGet(e, 0); k != NULL; k = k->next)
            if(power_of(k, s) > p)
                p = power_of(k, s);
        return p;
    }

    if(isoptype(e, OP_POW) && power_of(ast_ChildGet(e, 0), s) > 0)
        return degree(e);

    return 0;
}

/* same degree, go alphabetical like a dictionary. x^2 before xy before y^2 */
static bool comes_before(pcas_ast_t *a, pcas_ast_t *b) {
    int s;

    for(s = SYM_A; s <= SYM_Z; s++) {
        int pa = power_of(a, (Symbol)s), pb = power_of(b, (Symbol)s);
        if(pa != pb)
            return pa > pb;
    }
    return false;
}

static box_t *build_add(pcas_ast_t *e, unsigned char level) {
    /* absolute_val() changes the node, so work on a copy */
    pcas_ast_t *copy = ast_Copy(e), *child;
    box_t *row = new_box(BOX_ROW, level);
    int space = LAYOUT_SPACE(level);
    unsigned n = ast_ChildLength(copy), i, j;
    pcas_ast_t **terms = malloc(n * sizeof(pcas_ast_t *));
    int *degrees = malloc(n * sizeof(int));

    if(terms == NULL || degrees == NULL) {
        free(terms);
        free(degrees);
        ast_Cleanup(copy);
        oom = true;
        return row;
    }

    /* highest degree first, like a textbook. x^3 + x^2 - 8x - 12.
       insertion sort, so terms of equal degree keep the engine's order. */
    for(i = 0, child = ast_ChildGet(copy, 0); child != NULL; child = child->next, i++) {
        int d = degree(child);

        for(j = i; j > 0 && (degrees[j - 1] < d ||
                             (degrees[j - 1] == d && comes_before(child, terms[j - 1]))); j--) {
            terms[j] = terms[j - 1];
            degrees[j] = degrees[j - 1];
        }
        terms[j] = child;
        degrees[j] = d;
    }

    for(i = 0; i < n; i++) {
        child = terms[i];

        if(i > 0) {
            bool minus = absolute_val(child);

            add(row, new_space(space));
            add(row, new_text(minus ? STR_MINUS : "+", level));
            add(row, new_space(space));
        }

        add(row, build(child, level));
    }

    free(terms);
    free(degrees);
    ast_Cleanup(copy);
    return row;
}

static box_t *build_mult(pcas_ast_t *e, unsigned char level) {
    pcas_ast_t *child;
    box_t *row = new_box(BOX_ROW, level);
    int real = 0;   /* factors that aren't 1 or -1 */

    for(child = ast_ChildGet(e, 0); child != NULL; child = child->next)
        if(!is_ast_int(child, 1) && !is_ast_int(child, -1))
            real++;

    for(child = ast_ChildGet(e, 0); child != NULL; child = child->next) {
        if(is_ast_int(child, -1)) {
            add(row, new_text(STR_MINUS, level));
            /* -1 on its own (or times 1) still needs its 1 */
            if(real == 0) {
                add(row, new_text("1", level));
                real = -1;
            }
            continue;
        }

        if(is_ast_int(child, 1)) {
            if(real == 0) {
                add(row, new_text("1", level));
                real = -1;
            }
            continue;
        }

        add(row, build_parens(child, level, parens_as_factor(child)));

        if(child->next != NULL && needs_dot(child, child->next))
            add(row, new_text(STR_CDOT, level));
    }

    return row;
}

static box_t *build_call(const char *name, bool inverse, box_t *content, unsigned char level) {
    box_t *row = new_box(BOX_ROW, level);
    box_t *head = new_text(name, level);

    if(inverse) {
        char minus_one[3];

        minus_one[0] = CH_MINUS;
        minus_one[1] = '1';
        minus_one[2] = '\0';
        head = pair(BOX_SUP, head, new_text(minus_one, level + 1), level);
    }

    add(row, head);
    /* ln|x| instead of ln(|x|), the bars already do the brackets' job */
    if(content != NULL && content->kind == BOX_ABS)
        add(row, content);
    else
        add(row, wrap(BOX_PARENS, content, level));
    return row;
}

static box_t *build_log(pcas_ast_t *e, unsigned char level) {
    pcas_ast_t *base = ast_ChildGet(e, 0), *arg = ast_ChildGet(e, 1);
    box_t *row;

    if(base->type == NODE_SYMBOL && base->op.symbol == SYM_EULER)
        return build_call("ln", false, build(arg, level), level);

    if(is_ast_int(base, 10))
        return build_call("log", false, build(arg, level), level);

    row = new_box(BOX_ROW, level);
    add(row, pair(BOX_SUB, new_text("log", level), build(base, level + 1), level));
    add(row, wrap(BOX_PARENS, build(arg, level), level));
    return row;
}

static box_t *build_deriv(pcas_ast_t *e, unsigned char level) {
    pcas_ast_t *f = ast_ChildGet(e, 0), *var = ast_ChildGet(e, 1), *at = ast_ChildGet(e, 2);
    box_t *row = new_box(BOX_ROW, level);
    box_t *dvar = new_box(BOX_ROW, level);

    add(dvar, new_text("d", level));
    add(dvar, build(var, level));
    add(row, pair(BOX_FRAC, new_text("d", level), dvar, level));
    add(row, wrap(BOX_PARENS, build(f, level), level));

    /* evaluated somewhere other than the variable itself: |X=3 */
    if(!ast_Compare(var, at)) {
        box_t *where = new_box(BOX_ROW, level + 1);

        add(where, build(var, level + 1));
        add(where, new_text("=", level + 1));
        add(where, build(at, level + 1));
        add(row, pair(BOX_SUB, new_text("|", level), where, level));
    }

    return row;
}

static box_t *build_operator(pcas_ast_t *e, unsigned char level) {
    pcas_ast_t *a = ast_ChildGet(e, 0), *b = a != NULL ? a->next : NULL;

    switch(optype(e)) {
    case OP_ADD:
        return build_add(e, level);
    case OP_MULT:
        return build_mult(e, level);
    case OP_DIV: {
        /* stacked, so neither side needs brackets. a fraction inside a fraction
           drops a size, so the main bar stays easy to find. */
        unsigned char inner = frac_depth > 0 ? level + 1 : level;
        box_t *num, *den;

        /* in decimal a/b of two plain numbers is just one number, no bar */
        if(decimal && a->type == NODE_NUMBER && b->type == NODE_NUMBER) {
            mp_rat q = num_Copy(a->op.num);
            char s[24];

            mp_rat_div(q, b->op.num, q);
            approx_format(q, 6, s);
            num_Cleanup(q);
            return new_text(s, level);
        }

        frac_depth++;
        num = build(a, inner);
        den = build(b, inner);
        frac_depth--;
        return pair(BOX_FRAC, num, den, inner);
    }
    case OP_POW:
        return pair(BOX_SUP, build_parens(a, level, parens_as_base(a)), build(b, level + 1), level);
    case OP_ROOT:
        /* a = index, b = radicand */
        return pair(BOX_ROOT, build(b, level), is_ast_int(a, 2) ? NULL : build(a, level + 1), level);
    case OP_LOG:
        return build_log(e, level);
    case OP_DERIV:
        return build_deriv(e, level);
    case OP_FACTORIAL:
        return pair(BOX_ROW, build_parens(a, level, parens_as_base(a)), new_text("!", level), level);
    case OP_ABS:
        return wrap(BOX_ABS, build(a, level), level);
    case OP_INT:      return build_call("int",  false, build(a, level), level);
    case OP_SIN:      return build_call("sin",  false, build(a, level), level);
    case OP_SIN_INV:  return build_call("sin",  true,  build(a, level), level);
    case OP_COS:      return build_call("cos",  false, build(a, level), level);
    case OP_COS_INV:  return build_call("cos",  true,  build(a, level), level);
    case OP_TAN:      return build_call("tan",  false, build(a, level), level);
    case OP_TAN_INV:  return build_call("tan",  true,  build(a, level), level);
    case OP_COT:      return build_call("cot",  false, build(a, level), level);
    case OP_SEC:      return build_call("sec",  false, build(a, level), level);
    case OP_CSC:      return build_call("csc",  false, build(a, level), level);
    case OP_SINH:     return build_call("sinh", false, build(a, level), level);
    case OP_SINH_INV: return build_call("sinh", true,  build(a, level), level);
    case OP_COSH:     return build_call("cosh", false, build(a, level), level);
    case OP_COSH_INV: return build_call("cosh", true,  build(a, level), level);
    case OP_TANH:     return build_call("tanh", false, build(a, level), level);
    case OP_TANH_INV: return build_call("tanh", true,  build(a, level), level);
    case OP_GCD: case OP_LCM: case OP_NCR: case OP_NPR: {
        /* left alone when they had letters in, gcd(x,6). shown how they're typed */
        static const char *const two[] = { "gcd", "lcm", "nCr", "nPr" };
        box_t *args = new_box(BOX_ROW, level);

        add(args, build(a, level));
        add(args, new_text(",", level));
        add(args, build(b, level));
        return build_call(two[optype(e) - OP_GCD], false, args, level);
    }
    default:
        return new_text("?", level);
    }
}

static box_t *build(pcas_ast_t *e, unsigned char level) {
    if(e == NULL)
        return new_text("?", level);

    switch(e->type) {
    case NODE_NUMBER:   return build_number(e, level);
    case NODE_SYMBOL:   return build_symbol(e, level);
    case NODE_OPERATOR: return build_operator(e, level);
    }

    return NULL;
}

/* measuring */

#define MAX(a, b) ((a) > (b) ? (a) : (b))

/* is there a fraction inside b, on its own line? then a fraction around it gets the
   long bar. exponents, subscripts and root indexes don't count: they're already small. */
static bool has_frac(const box_t *b) {
    const box_t *k;

    if(b->kind == BOX_FRAC)
        return true;

    if(b->kind == BOX_SUP || b->kind == BOX_SUB)
        return has_frac(b->child);  /* the base only */
    if(b->kind == BOX_ROOT)
        return has_frac(b->child); /* radicand only */

    for(k = b->child; k != NULL; k = k->next)
        if(has_frac(k))
            return true;
    return false;
}

/* what's under the editor's highlight, if it's just one thing */
static const box_t *unselected(const box_t *b) {
    return b->kind == BOX_SELECT && b->child != NULL && b->child->next == NULL ? b->child : b;
}

static bool is_dot(const box_t *b) {
    b = unselected(b);
    return b->kind == BOX_TEXT && b->text != NULL && strcmp(b->text, STR_CDOT) == 0;
}

/* the bar runs straight into a · next to it otherwise */
static bool dot_by_frac(const box_t *a, const box_t *b) {
    return (is_dot(a) && unselected(b)->kind == BOX_FRAC) || (unselected(a)->kind == BOX_FRAC && is_dot(b));
}

static void measure(const layout_font_t *f, box_t *b) {
    int fi = b->size;
    box_t *k;

    for(k = b->child; k != NULL; k = k->next)
        measure(f, k);

    switch(b->kind) {
    case BOX_TEXT:
        b->width = f->text_width(b->text, b->size, b->style);
        b->ascent = f->ascent[fi];
        b->descent = f->descent[fi];
        break;

    case BOX_SPACE:
        /* width was set when it was made */
        break;

    case BOX_ROW:
    case BOX_SELECT: {
        int x = 0;
        const box_t *prev = NULL;

        /* even an empty row is a line tall */
        b->ascent = f->ascent[fi];
        b->descent = f->descent[fi];

        for(k = b->child; k != NULL; k = k->next) {
            /* the same room as round a +, the editor's cursor doesn't count as being between */
            if(prev != NULL && k->kind != BOX_CURSOR && dot_by_frac(prev, k))
                x += LAYOUT_SPACE(b->level);
            if(k->kind != BOX_CURSOR)
                prev = k;
            k->x = x;
            k->y = 0;
            x += k->width;
            b->ascent = MAX(b->ascent, k->ascent);
            b->descent = MAX(b->descent, k->descent);
        }

        b->width = x;
        break;
    }

    /* origin on the last line's baseline, since the history draws an answer from
       its baseline and the bottom line should land where a one-line answer would */
    case BOX_COL: {
        box_t *last = NULL;
        int y = 0, shift;

        /* y's go top down from 0 first, the last line's y isn't known till the end */
        b->width = 0;
        for(k = b->child; k != NULL; k = k->next) {
            if(last != NULL)
                y += COL_GAP + k->ascent;
            k->y = y;
            y += k->descent;
            b->width = MAX(b->width, k->width);
            last = k;
        }

        /* even an empty column is a line tall */
        if(last == NULL) {
            b->ascent = f->ascent[fi];
            b->descent = f->descent[fi];
            break;
        }

        shift = last->y;
        for(k = b->child; k != NULL; k = k->next) {
            k->x = b->width - k->width;
            k->y -= shift;
        }

        b->ascent = b->child->ascent - b->child->y;
        b->descent = last->descent;
        break;
    }

    case BOX_FRAC: {
        box_t *num = b->child, *den = num->next;
        bool outer = has_frac(num) || has_frac(den);
        int gap = outer ? FRAC_GAP_OUTER : FRAC_GAP;

        /* the bar sits halfway up a digit, level with the middle of a + */
        b->aux = -((f->ascent[fi] + 1) / 2);
        b->width = MAX(num->width, den->width) + 2 * (outer ? FRAC_OVERHANG_OUTER : FRAC_OVERHANG);

        num->x = (b->width - num->width) / 2;
        den->x = (b->width - den->width) / 2;
        num->y = b->aux - gap - num->descent;
        den->y = b->aux + gap + 1 + den->ascent;

        b->ascent = num->ascent - num->y;
        b->descent = den->y + den->descent;
        break;
    }

    case BOX_SUP: {
        box_t *base = b->child, *sup = base->next;
        int raise = MAX(base->ascent - sup->ascent / 2, SUP_MIN_RAISE);

        base->x = 0;
        base->y = 0;
        sup->x = base->width + SUP_GAP;
        sup->y = -raise;

        b->width = sup->x + sup->width;
        b->ascent = MAX(base->ascent, raise + sup->ascent);
        b->descent = MAX(base->descent, sup->descent - raise);
        break;
    }

    case BOX_SUB: {
        box_t *base = b->child, *sub = base->next;

        base->x = 0;
        base->y = 0;
        sub->x = base->width;
        sub->y = SUB_DROP;

        b->width = sub->x + sub->width;
        b->ascent = MAX(base->ascent, sub->ascent - SUB_DROP);
        b->descent = MAX(base->descent, SUB_DROP + sub->descent);
        break;
    }

    case BOX_ROOT: {
        box_t *rad = b->child, *index = rad->next;
        /* with an index, 2 px of room first, so 2∛x doesn't read as 2³√x */
        int lead = index != NULL ? MAX(index->width - ROOT_INDEX_W, 0) + 2 : 0;

        rad->x = lead + ROOT_SIGN_W;
        rad->y = 0;
        b->aux = -rad->ascent - ROOT_GAP - 1;

        b->width = rad->x + rad->width + ROOT_TAIL;
        b->ascent = -b->aux;
        b->descent = rad->descent;

        if(index != NULL) {
            /* bottom of the index just above the middle of the sign */
            int middle = (b->aux + b->descent) / 2;

            index->x = 2;
            index->y = middle - index->descent;
            b->ascent = MAX(b->ascent, index->ascent - index->y);
        }
        break;
    }

    case BOX_CURSOR:
        b->width = 0;
        b->ascent = f->ascent[fi];
        b->descent = f->descent[fi];
        break;

    case BOX_SLOT:
        b->width = f->text_width("0", b->size, 0) + 2;
        b->ascent = f->ascent[fi];
        b->descent = f->descent[fi];
        break;

    case BOX_PARENS:
    case BOX_ABS: {
        box_t *content = b->child;
        int side = b->kind == BOX_PARENS ? PAREN_W : ABS_W;
        bool tall = content->ascent > f->ascent[fi] || content->descent > f->descent[fi];

        content->x = side;
        content->y = 0;

        b->width = content->width + 2 * side;
        /* taller than a line, a pixel of room above and below */
        b->ascent = MAX(content->ascent, f->ascent[fi]) + (tall ? 1 : 0);
        b->descent = MAX(content->descent, f->descent[fi]) + (tall ? 1 : 0);
        break;
    }
    }
}

/* font sizes */

static int deepest(const box_t *b) {
    int d = 0;

    for(; b != NULL; b = b->next) {
        /* into a local first! MAX evaluates it twice, which made this exponential
           in the nesting (16 nested sin( took over 30 s on the PC, found that
           one the hard way) */
        int child = deepest(b->child), here = MAX(b->level, child);

        d = MAX(d, here);
    }
    return d;
}

static void assign_sizes(box_t *b, int base) {
    for(; b != NULL; b = b->next) {
        int size = base - b->level;

        b->size = (unsigned char)(size > 0 ? size : 0);
        assign_sizes(b->child, base);
    }
}

/* the public entry points */

void layout_finish(box_t *root, const layout_font_t *font) {
    int base;

    /* normal math at font->base, unless something nests deeper than the sizes
       below it: then a bigger base, so the deepest part is still size 0 */
    base = deepest(root);
    base = MAX(font->base, base);
    if(base > font->sizes - 1)
        base = font->sizes - 1;
    assign_sizes(root, base);

    measure(font, root);
}

box_t *layout_build(pcas_ast_t *e, const layout_font_t *font) {
    box_t *b;

    oom = false;
    frac_depth = 0;
    b = build(e, 0);

    if(oom || b == NULL) {
        layout_free(b);
        return NULL;
    }

    layout_finish(b, font);
    return b;
}

box_t *layout_box(box_kind_t kind, unsigned char level) { return new_box(kind, level); }
box_t *layout_text(const char *text, unsigned char level) { return new_text(text, level); }
box_t *layout_space(int width) { return new_space(width); }
void layout_add(box_t *parent, box_t *child) { add(parent, child); }

void layout_free(box_t *b) {
    while(b != NULL) {
        box_t *next = b->next;

        layout_free(b->child);
        free(b->text);
        free(b);
        b = next;
    }
}
