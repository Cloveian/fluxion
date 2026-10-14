#include "app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "engine/ast.h"
#include "engine/parser.h"
#include "engine/cas/cas.h"
#include "engine/cas/identities.h"
#include "editor/editor.h"
#include "render/draw.h"
#include "platform/screen.h"
#include "platform/keys.h"
#include "platform/power.h"
#include "ui/catalog.h"

/*
    screen, top to bottom:
        history     input left, result right, newest at the bottom
        ─────────
        input line  with the cursor
        soft keys   Algebra | Calculus | Functions | Templates | Tools
    a folder opens as a list above its soft key. keys: docs/design-choices/input.md
*/

#define MARGIN      4
#define GAP         4
#define BAR_H       15          /* soft-key labels */
#define HISTORY_MAX 16          /* enough for now, old ones just scroll off the top */

typedef struct {
    box_t *in, *out;    /* out == NULL means a message instead */
    const char *message;
    unsigned long ms;
} entry_t;

static entry_t history[HISTORY_MAX];
static int history_len;

static editor_t ed;
static bool second, alpha;      /* [2nd] / [alpha] pressed, for the next key */
static bool second_lock, alpha_lock;    /* stay on after a key */
static bool ctl, ctl_lock;      /* [on] tapped, so the next key becomes [on]+key (Ctrl) */
static const char *status;      /* one-line message above the input, or NULL */

static int menu = -1;           /* open folder, -1 = none */
static int menu_sel, menu_top;

static unsigned short simp_flags = SIMP_ALL;
static unsigned char canon_flags = CANONICAL_ALL;

/* history */

static void push(box_t *in, box_t *out, const char *message, unsigned long ms) {
    if(history_len == HISTORY_MAX) {
        layout_free(history[0].in);
        layout_free(history[0].out);
        memmove(&history[0], &history[1], (HISTORY_MAX - 1) * sizeof(entry_t));
        history_len--;
    }

    history[history_len].in = in;
    history[history_len].out = out;
    history[history_len].message = message;
    history[history_len].ms = ms;
    history_len++;
}

static void clear_history(void) {
    while(history_len > 0) {
        history_len--;
        layout_free(history[history_len].in);
        layout_free(history[history_len].out);
    }
}

/* running the engine */

#define SIMP_BASIC (SIMP_NORMALIZE | SIMP_COMMUTATIVE | SIMP_RATIONAL)

/* a word as a result, like true / false */
static box_t *word_box(const char *text) {
    box_t *b = layout_text(text, 0);

    if(b != NULL)
        layout_finish(b, &draw_font);
    return b;
}

/* x = -5 or x = 6, like the Nspire */
static box_t *solutions_box(char var, pcas_ast_t **roots, int n) {
    char name[2] = { var, '\0' };
    box_t *row = layout_box(BOX_ROW, 0);
    int i;

    for(i = 0; i < n; i++) {
        if(i > 0) {
            layout_add(row, layout_space(2 * SPACE_BINARY));
            layout_add(row, layout_text("or", 0));
            layout_add(row, layout_space(2 * SPACE_BINARY));
        }
        layout_add(row, layout_text(name, 0));
        layout_add(row, layout_space(LAYOUT_SPACE(0)));
        layout_add(row, layout_text("=", 0));
        layout_add(row, layout_space(LAYOUT_SPACE(0)));

        simplify_canonical_form(roots[i], canon_flags);
        layout_add(row, layout_build(roots[i], &draw_font));
    }

    layout_finish(row, &draw_font);
    return row;
}

/*
    solve(equation, variable). the engine's parser has no =, so the line is split
    here: at the = and the last comma outside brackets. no variable: x.
    takes src apart (mutates src! careful), in is the input line's boxes for the history.
*/
static void run_solve(char *src, box_t *in) {
    pcas_ast_t *roots[SOLVE_MAX] = { NULL }, *e, *rhs = NULL;
    char *eq = NULL, *comma = NULL, *s, shown = 'x';
    Symbol var = SYM_X;
    pcas_error_t err;
    clock_t start;
    unsigned long ms;
    int depth = 0, n, i;

    for(s = src; *s != '\0'; s++) {
        if(*s == '(')
            depth++;
        else if(*s == ')')
            depth--;
        else if(depth == 0 && *s == '=' && eq == NULL)
            eq = s;
        else if(depth == 0 && *s == ',')
            comma = s;
    }

    if(comma != NULL) {
        *comma = '\0';
        if(strcmp(comma + 1, "theta") == 0) {
            var = SYM_THETA;
            shown = CH_THETA;
        } else if(comma[1] >= 'A' && comma[1] <= 'Z' && comma[2] == '\0') {
            var = (Symbol)comma[1];
            shown = (char)(comma[1] - 'A' + 'a');
        } else {
            push(in, NULL, "solve(equation, variable)", 0);
            return;
        }
    }

    if(eq != NULL)
        *eq = '\0';
    e = parse((const uint8_t *)src, strlen(src), str_table, &err);
    if(err == E_SUCCESS && eq != NULL)
        rhs = parse((const uint8_t *)eq + 1, strlen(eq + 1), str_table, &err);

    if(err != E_SUCCESS || e == NULL || (eq != NULL && rhs == NULL)) {
        if(e != NULL)
            ast_Cleanup(e);
        if(rhs != NULL)
            ast_Cleanup(rhs);
        push(in, NULL, err != E_SUCCESS ? error_text[err] : "solve(equation, variable)", 0);
        return;
    }

    /* a = b  ->  a - b = 0 */
    if(rhs != NULL)
        e = ast_MakeBinary(OP_ADD, e, ast_MakeBinary(OP_MULT, ast_MakeNumber(num_FromInt(-1)), rhs));

    start = clock();
    n = solve(e, var, roots);
    ms = (unsigned long)((clock() - start) * 1000 / CLOCKS_PER_SEC);
    ast_Cleanup(e);

    if(pcas_cancelled)
        push(in, NULL, "cancelled", ms);
    else if(n == SOLVE_UNKNOWN)
        push(in, NULL, "can't solve this one yet", ms);
    else if(n == SOLVE_ALL)
        push(in, word_box("true"), NULL, ms);
    else if(n == 0)
        push(in, word_box("false"), NULL, ms);
    else
        push(in, solutions_box(shown, roots, n), NULL, ms);

    for(i = 0; i < n; i++)
        ast_Cleanup(roots[i]);
}

static void evaluate(void) {
    const char *cmd = ed_command(&ed);
    char *src;
    pcas_error_t err;
    pcas_ast_t *e;
    box_t *in;
    clock_t start;

    if(ed_is_empty(&ed))
        return;

    src = cmd != NULL ? ed_command_arg(&ed) : ed_to_engine(&ed);
    if(src == NULL) {
        status = "fill in the empty boxes first";
        return;
    }

    /* no nesting commands yet, expand(...) has to be the whole line (todo, ugh) */
    if(ed_has_command(src)) {
        free(src);
        status = "commands have to be the whole line (for now)";
        return;
    }

    if(cmd != NULL && strcmp(cmd, "solve") == 0) {
        run_solve(src, ed_layout(&ed, &draw_font, false));
        free(src);
        ed_clear(&ed);
        return;
    }
    if(strchr(src, '=') != NULL) {
        free(src);
        status = "= only works in solve( for now";
        return;
    }

    e = parse((const uint8_t *)src, strlen(src), str_table, &err);
    free(src);
    in = ed_layout(&ed, &draw_font, false);

    if(err != E_SUCCESS || e == NULL) {
        push(in, NULL, error_text[err], 0);
        ed_clear(&ed);
        return;
    }

    start = clock();

    /* same steps PineappleCAS's own UI does for each command, no need to be clever */
    if(cmd != NULL && strcmp(cmd, "expand") == 0) {
        simplify(e, SIMP_BASIC);
        expand(e, EXP_ALL);
        simplify(e, SIMP_BASIC | SIMP_EVAL | SIMP_LIKE_TERMS);
    } else if(cmd != NULL && strcmp(cmd, "factor") == 0) {
        simplify(e, SIMP_BASIC);
        factor(e, FAC_ALL);
        simplify(e, SIMP_BASIC | SIMP_EVAL);
    } else if(cmd != NULL && strcmp(cmd, "eval") == 0) {
        simplify(e, simp_flags);
        eval(e, EVAL_ALL);
        simplify(e, SIMP_BASIC | SIMP_EVAL | SIMP_LIKE_TERMS);
    } else {
        /* before simplify splits the fractions apart */
        cancel_fractions(e);
        simplify(e, simp_flags);
    }
    simplify_canonical_form(e, canon_flags);

    /* cancelled: the tree is half simplified, nobody wants to see that lol */
    push(in, pcas_cancelled ? NULL : layout_build(e, &draw_font), pcas_cancelled ? "cancelled" : NULL,
         (unsigned long)((clock() - start) * 1000 / CLOCKS_PER_SEC));
    ast_Cleanup(e);
    ed_clear(&ed);
}

/* -- menus -- */

static const item_t *menu_item(int i) {
    return folders[menu].items[i];
}

/* Tools - which flag each setting maps to */
static bool setting(const item_t *it, unsigned short **simp, unsigned short *bit,
                    unsigned char **canon, unsigned char *cbit) {
    static const struct { const char *label; unsigned short bit; } ids[] = {
        { "basic identities",       SIMP_ID_GENERAL },
        { "trig identities",        SIMP_ID_TRIG },
        { "hyperbolic identities",  SIMP_ID_HYPERBOLIC },
        { "complex identities",     SIMP_ID_COMPLEX },
        { "trig constants",         SIMP_ID_TRIG_CONSTANTS },
        { "inverse trig constants", SIMP_ID_TRIG_INV_CONSTANTS },
    };
    unsigned i;

    *simp = NULL;
    *canon = NULL;
    for(i = 0; i < sizeof ids / sizeof ids[0]; i++) {
        if(strcmp(it->label, ids[i].label) == 0) {
            *simp = &simp_flags;
            *bit = ids[i].bit;
            return true;
        }
    }
    if(strcmp(it->label, "rationalize") == 0) {
        *canon = &canon_flags;
        *cbit = CANONICAL_RATIONALIZE;
        return true;
    }
    return false;
}

static bool setting_on(const item_t *it) {
    unsigned short *simp, bit = 0;
    unsigned char *canon, cbit = 0;

    if(!setting(it, &simp, &bit, &canon, &cbit))
        return false;
    return simp != NULL ? (*simp & bit) != 0 : (*canon & cbit) != 0;
}

/*
    what picking an item does, worked out from its engine syntax (catalog.c).
    returns false if the editor can't do it yet. with dry set, only checks.
*/
static bool pick(const item_t *it, bool dry) {
    const char *s = it->engine;

    if(it->have == HAVE_NO)
        return false;

    if(it->kind == ITEM_SETTING) {
        unsigned short *simp, bit = 0;
        unsigned char *canon, cbit = 0;

        if(strcmp(it->label, "clear history") == 0) {
            if(!dry)
                clear_history();
            return true;
        }
        if(!setting(it, &simp, &bit, &canon, &cbit))
            return false;
        if(!dry) {
            if(simp != NULL)
                *simp ^= bit;
            else
                *canon ^= cbit;
        }
        return true;
    }

    if(s == NULL)
        return false;

    if(it->kind == ITEM_CHAR) {
        char c = strcmp(s, "pi") == 0 ? CH_PI : strcmp(s, "e") == 0 ? CH_EULER :
                 strcmp(s, "i") == 0 ? CH_IMAG : strcmp(s, "theta") == 0 ? CH_THETA :
                 strcmp(s, "!") == 0 ? '!' : 0;
        if(c == 0)
            return false;
        if(!dry)
            ed_insert_char(&ed, c);
        return true;
    }

    {
        static const struct { const char *engine; ed_kind_t kind; } shapes[] = {
            { "($)^($)", ED_POW }, { "sqrt($)", ED_SQRT }, { "abs($)", ED_ABS },
            { "logb($,$)", ED_LOGB }, { "($)/($)", ED_FRAC }, { "($)", ED_PARENS },
        };
        unsigned i;

        for(i = 0; i < sizeof shapes / sizeof shapes[0]; i++) {
            if(strcmp(s, shapes[i].engine) == 0) {
                if(!dry)
                    ed_insert(&ed, shapes[i].kind, NULL);
                return true;
            }
        }
    }

    if(strcmp(s, "($)root($)") == 0) {
        if(!dry) {
            ed_insert(&ed, ED_NROOT, NULL);
            if(strcmp(it->label, "cbrt") == 0) {
                ed_insert_char(&ed, '3');
                ed_down(&ed);
            }
        }
        return true;
    }
    if(strcmp(s, "e^($)") == 0 || strcmp(s, "10^($)") == 0) {
        if(!dry) {
            if(s[0] == 'e') {
                ed_insert_char(&ed, CH_EULER);
            } else {
                ed_insert_char(&ed, '1');
                ed_insert_char(&ed, '0');
            }
            ed_insert(&ed, ED_POW, NULL);
        }
        return true;
    }
    if(strcmp(s, "deriv($,X,X)") == 0) {
        if(!dry)
            ed_insert(&ed, ED_FUNC, "deriv");
        return true;
    }

    /* name($), the functions plus the commands the app runs itself */
    {
        const char *paren = strchr(s, '(');
        char name[8];
        size_t n = paren != NULL ? (size_t)(paren - s) : 0;

        if(n == 0 || n >= sizeof name || strcmp(paren, "($)") != 0)
            return false;
        memcpy(name, s, n);
        name[n] = '\0';
        if(it->kind == ITEM_COMMAND && !ed_is_command(name))
            return false;
        if(!dry)
            ed_insert(&ed, ED_FUNC, name);
        return true;
    }
}

static void menu_open(int folder) {
    menu = folder;
    menu_sel = 0;
    menu_top = 0;
}

static void menu_choose(int i) {
    const item_t *it = menu_item(i);

    if(!pick(it, false)) {
        status = "not in Fluxion yet";
        return;
    }
    /* settings stay open so several can be toggled */
    if(it->kind != ITEM_SETTING)
        menu = -1;
}

static void menu_key(keypress_t p) {
    int count = (int)folders[menu].count;

    switch(p.key) {
    case K_UP:    menu_sel = (menu_sel + count - 1) % count; break;
    case K_DOWN:  menu_sel = (menu_sel + 1) % count; break;
    case K_LEFT:  menu_open((menu + FOLDER_COUNT - 1) % FOLDER_COUNT); break;
    case K_RIGHT: menu_open((menu + 1) % FOLDER_COUNT); break;
    case K_ENTER: menu_choose(menu_sel); break;
    case K_CLEAR: menu = -1; break;

    case K_YEQU: case K_WINDOW: case K_ZOOM: case K_TRACE: case K_GRAPH: {
        int f = (int)(p.key - K_YEQU);
        if(f == menu)
            menu = -1;
        else
            menu_open(f);
        break;
    }

    default:
        if(p.key >= K_1 && p.key <= K_9 && (int)(p.key - K_1) < count)
            menu_choose((int)(p.key - K_1));
        break;
    }
}

/* typing (key -> editor) */

/* x^2, x^-1. make an exponent with this in it, then step back out */
static void power_of(const char *what) {
    ed_insert(&ed, ED_POW, NULL);
    for(; *what != '\0'; what++)
        ed_insert_char(&ed, *what);
    ed_right(&ed);
}

/* [alpha] + key gives the letter printed above the key, like the TI */
static char alpha_letter(key_t k) {
    switch(k) {
    case K_MATH: return 'a';   case K_APPS: return 'b';   case K_PRGM: return 'c';
    case K_RECIP: return 'd';  case K_SIN: return 'e';    case K_COS: return 'f';
    case K_TAN: return 'g';    case K_POW: return 'h';    case K_SQUARE: return 'i';
    case K_COMMA: return 'j';  case K_LPAREN: return 'k'; case K_RPAREN: return 'l';
    case K_DIV: return 'm';    case K_LOG: return 'n';    case K_7: return 'o';
    case K_8: return 'p';      case K_9: return 'q';      case K_MUL: return 'r';
    case K_LN: return 's';     case K_4: return 't';      case K_5: return 'u';
    case K_6: return 'v';      case K_SUB: return 'w';    case K_STO: return 'x';
    case K_1: return 'y';      case K_2: return 'z';      case K_3: return CH_THETA;
    default: return 0;
    }
}

/* returns false to quit */
static bool key_2nd(keypress_t p) {
    switch(p.key) {
    case K_MODE:   return false;   /* quit */
    case K_POW:    ed_insert_char(&ed, CH_PI); break;
    case K_DIV:    ed_insert_char(&ed, CH_EULER); break;
    case K_DOT:    ed_insert_char(&ed, CH_IMAG); break;
    case K_SQUARE: ed_insert(&ed, ED_SQRT, NULL); break;
    case K_SIN:    ed_insert(&ed, ED_FUNC, "asin"); break;
    case K_COS:    ed_insert(&ed, ED_FUNC, "acos"); break;
    case K_TAN:    ed_insert(&ed, ED_FUNC, "atan"); break;
    case K_MATH:   ed_insert_char(&ed, '='); break; /* TI's [2nd][math] is TEST, = is first */
    case K_LN:     ed_insert_char(&ed, CH_EULER); ed_insert(&ed, ED_POW, NULL); break;
    case K_LOG:    ed_insert_char(&ed, '1'); ed_insert_char(&ed, '0'); ed_insert(&ed, ED_POW, NULL); break;
    default:       break;
    }
    return true;
}

static bool key_plain(keypress_t p) {
    if(p.key >= K_0 && p.key <= K_9) {
        ed_insert_char(&ed, (char)('0' + (p.key - K_0)));
        return true;
    }
    if(p.key >= K_LETTER_A && p.key <= K_LETTER_Z) {
        ed_insert_char(&ed, (char)('a' + (p.key - K_LETTER_A)));
        return true;
    }

    switch(p.key) {
    case K_QUIT:   return false;
    case K_DOT:    ed_insert_char(&ed, '.'); break;
    case K_NEG:    ed_insert_char(&ed, ED_NEG); break;
    case K_ADD:    ed_insert_char(&ed, ED_PLUS); break;
    case K_SUB:    ed_insert_char(&ed, ED_SUB); break;
    case K_MUL:    ed_insert_char(&ed, ED_TIMES); break;
    case K_DIV:    ed_insert_char(&ed, ED_DIVIDE); break;
    case K_POW:    ed_insert(&ed, ED_POW, NULL); break;
    case K_SQUARE: power_of("2"); break;
    case K_RECIP: {
        char minus_one[3] = { ED_NEG, '1', '\0' };
        power_of(minus_one);
        break;
    }
    case K_LPAREN: ed_insert_char(&ed, '('); break;
    case K_RPAREN: ed_insert_char(&ed, ')'); break;
    case K_COMMA:  ed_insert_char(&ed, ','); break;
    case K_XT:     ed_insert_char(&ed, 'x'); break;
    case K_SIN:    ed_insert(&ed, ED_FUNC, "sin"); break;
    case K_COS:    ed_insert(&ed, ED_FUNC, "cos"); break;
    case K_TAN:    ed_insert(&ed, ED_FUNC, "tan"); break;
    case K_LN:     ed_insert(&ed, ED_FUNC, "ln"); break;
    case K_LOG:    ed_insert(&ed, ED_FUNC, "log"); break;

    case K_LEFT:   ed_left(&ed); break;
    case K_RIGHT:  ed_right(&ed); break;
    case K_UP:     ed_up(&ed); break;
    case K_DOWN:   ed_down(&ed); break;

    /* [clear] is backspace, [on]+[clear] clears the line (Ctrl+Backspace) */
    case K_CLEAR:
        if(p.on)
            ed_clear(&ed);
        else
            ed_backspace(&ed);
        break;
    case K_DEL:    ed_delete(&ed); break;
    case K_ENTER:  evaluate(); break;

    case K_YEQU: case K_WINDOW: case K_ZOOM: case K_TRACE: case K_GRAPH:
        menu_open((int)(p.key - K_YEQU));
        break;

    default:
        break;
    }
    return true;
}

/* two taps of [2nd] or [alpha] this close together lock it */
#define DOUBLE_TAP (CLOCKS_PER_SEC * 2 / 5)

/* [2nd] / [alpha] / [on]: on for the next key, a double tap or [2nd][alpha]
   locks (alpha lock, like the TI), one more tap unlocks. [on] goes with either */
/* one modifier tapped. a quick second tap locks it whatever the first one did
   (it may have just turned it off), a tap while locked unlocks */
static void tap(bool *on, bool *lock, bool again) {
    if(again)
        *on = *lock = true;
    else if(*lock)
        *on = *lock = false;
    else
        *on = !*on;
}

/* the last modifier tapped, K_NONE after any other key. a double tap is two
   taps with nothing in between (alpha a alpha b isn't one) */
static key_t last;
static clock_t last_time;

static void modifier(key_t k) {
    /* modifier? i hardly know 'er bahahwahaa */
    bool again = k == last && clock() - last_time < DOUBLE_TAP;

    last = k;
    last_time = clock();

    if(k == K_ON) {
        tap(&ctl, &ctl_lock, again);
    } else if(k == K_ALPHA && second && !second_lock) {
        second = false;
        alpha = alpha_lock = true;
        again = true;
    } else if(k == K_2ND) {
        tap(&second, &second_lock, again);
        alpha = alpha_lock = false;
    } else {
        tap(&alpha, &alpha_lock, again);
        second = second_lock = false;
    }

    /* locking ate the double tap, so a third quick tap starts a fresh one */
    if(again)
        last = K_NONE;
}

static bool handle(keypress_t p) {
    status = NULL;

    if(p.key == K_QUIT)
        return false;

    if(menu >= 0) {
        menu_key(p);
        return true;
    }

    if(p.key == K_2ND || p.key == K_ALPHA || p.key == K_ON) {
        modifier(p.key);
        return true;
    }
    last = K_NONE;
    if(ctl) {
        p.on = true;
        ctl = ctl_lock;
    }

    if(second) {
        second = second_lock;
        return key_2nd(p);
    }

    if(alpha) {
        char c = alpha_letter(p.key);

        alpha = alpha_lock;
        if(p.key == K_YEQU) {   /* [alpha][y=] is n/d, like the TI */
            ed_insert(&ed, ED_FRAC, NULL);
            return true;
        }
        if(c != 0) {
            ed_insert_char(&ed, c);
            return true;
        }
    }

    return key_plain(p);
}

/* drawing */

/* screen x of the cursor inside b drawn at x, or -1 */
static int cursor_x(const box_t *b, int x) {
    const box_t *k;

    x += b->x;
    if(b->kind == BOX_CURSOR)
        return x;
    for(k = b->child; k != NULL; k = k->next) {
        int found = cursor_x(k, x);
        if(found >= 0)
            return found;
    }
    return -1;
}

static void draw_bar(void) {
    const font_t *font = draw_font_for(0);     /* smallest, five names have to fit somehow */
    int i, w = SCREEN_W / FOLDER_COUNT, top = SCREEN_H - BAR_H;

    for(i = 0; i < FOLDER_COUNT; i++) {
        const char *name = folders[i].name;
        int x = i * w + (w - draw_text_width(name, font)) / 2;

        if(i == menu) {
            screen_color(COL_ACCENT);
            screen_fill(i * w + 1, top, w - 2, BAR_H);
            screen_color(COL_BG);
        } else {
            screen_color(COL_DIM);
        }
        draw_text(x, top + (BAR_H - font->ascent - font->descent) / 2, name, font);
    }
}

static void draw_menu(void) {
    const font_t *font = draw_label_font();
    const folder_t *f = &folders[menu];
    int row_h = font->ascent + font->descent + 2;
    int bottom = SCREEN_H - BAR_H - 1;
    int rows = (int)f->count, max_rows = (bottom - 4) / row_h;
    int width = 0, x, top, i;

    for(i = 0; i < rows; i++) {
        int w = draw_text_width(f->items[i]->label, font);
        if(w > width)
            width = w;
    }
    width += 34;    /* for the "1:" plus room for a * or [x] */

    if(rows > max_rows)
        rows = max_rows;
    if(menu_sel < menu_top)
        menu_top = menu_sel;
    if(menu_sel >= menu_top + rows)
        menu_top = menu_sel - rows + 1;

    x = menu * (SCREEN_W / FOLDER_COUNT);
    if(x + width > SCREEN_W - 1)
        x = SCREEN_W - 1 - width;
    top = bottom - rows * row_h - 2;

    screen_color(COL_BG);
    screen_fill(x, top, width, bottom - top + 1);
    screen_color(COL_DIM);
    screen_hline(x, top, width);
    screen_vline(x, top, bottom - top + 1);
    screen_vline(x + width - 1, top, bottom - top + 1);

    for(i = 0; i < rows; i++) {
        int n = menu_top + i, y = top + 2 + i * row_h;
        const item_t *it = f->items[n];
        bool can = pick(it, true);
        char label[48];

        if(it->kind == ITEM_SETTING && strcmp(it->label, "clear history") != 0)
            snprintf(label, sizeof label, "%s %s", setting_on(it) ? "[x]" : "[ ]", it->label);
        else
            snprintf(label, sizeof label, "%s%s", it->label, can ? "" : " *");

        if(n == menu_sel) {
            screen_color(COL_ACCENT);
            screen_fill(x + 1, y - 1, width - 2, row_h);
            screen_color(COL_BG);
        } else {
            screen_color(can ? COL_FG : COL_DIM);
        }

        if(n < 9) {
            char num[3] = { (char)('1' + n), ':', '\0' };
            draw_text(x + 3, y, num, font);
        }
        draw_text(x + 18, y, label, font);
    }
}

/* the status bar */

/* along the top: 2nd / alpha (with a padlock when locked), the working dots,
   the battery */

#define BATTERY_W   13
#define BATTERY_READ (CLOCKS_PER_SEC * 10)     /* the level is read this often (reading every frame was slow) */

static int battery = -1;
static clock_t battery_next;

static int top_h(void) {
    const font_t *small = draw_label_font();

    return small->ascent + small->descent + 3;
}

/* 7 x 8, top left at x, y */
static void draw_lock(int x, int y) {
    screen_hline(x + 2, y, 3);
    screen_vline(x + 1, y + 1, 3);
    screen_vline(x + 5, y + 1, 3);
    screen_fill(x, y + 3, 7, 5);
}

/* BATTERY_W x 7, top left at x, y, two columns of fill per bar (0-4) */
static void draw_battery(int x, int y, int level) {
    screen_hline(x, y, 12);
    screen_hline(x, y + 6, 12);
    screen_vline(x, y, 7);
    screen_vline(x + 11, y, 7);
    screen_vline(x + 12, y + 2, 3);
    if(level > 0)
        screen_fill(x + 2, y + 2, level * 2, 3);
}

/* a modifier's name at x, after a padlock if it's locked. returns where the next goes */
static int draw_modifier(int x, const char *name, bool locked) {
    const font_t *small = draw_label_font();

    if(locked) {
        draw_lock(x, (top_h() - 1 - 8) / 2);
        x += 10;
    }
    draw_text(x, 1, name, small);
    return x + draw_text_width(name, small) + 8;
}

static void draw_top_bar(void) {
    int h = top_h(), x = MARGIN;

    if(battery < 0 || clock() >= battery_next) {
        battery = power_battery();
        battery_next = clock() + BATTERY_READ;
    }

    screen_color(COL_BG);
    screen_fill(0, 0, SCREEN_W, h);
    screen_color(COL_DIM);
    screen_hline(MARGIN, h - 1, SCREEN_W - 2 * MARGIN);
    draw_battery(SCREEN_W - MARGIN - BATTERY_W, (h - 1 - 7) / 2, battery);

    screen_color(COL_ACCENT);
    if(ctl)
        x = draw_modifier(x, "ctl", ctl_lock);
    if(second)
        draw_modifier(x, "2nd", second_lock);
    else if(alpha)
        draw_modifier(x, "alpha", alpha_lock);
}

static void draw_screen(void) {
    const font_t *small = draw_label_font();
    box_t *in = ed_layout(&ed, &draw_font, true);
    int line_y, y, i, avail = SCREEN_W - 2 * MARGIN;

    screen_clear();
    draw_bar();

    /* input line, just above the soft keys. scrolls sideways to keep the cursor in view. */
    if(in != NULL) {
        int x = MARGIN, base = SCREEN_H - BAR_H - 3 - in->descent;

        if(in->width > avail) {
            int cx = cursor_x(in, 0), shift = cx - avail * 3 / 4;

            if(shift > in->width - avail)
                shift = in->width - avail;
            if(shift > 0)
                x -= shift;
        }
        screen_color(COL_FG);
        draw_box(in, x, base);
        line_y = base - in->ascent - 3;
        layout_free(in);
    } else {
        line_y = SCREEN_H - BAR_H - 20;
    }

    screen_color(COL_DIM);
    screen_hline(MARGIN, line_y, SCREEN_W - 2 * MARGIN);

    /* messages at the right of the line */
    if(status != NULL) {
        screen_color(COL_DIM);
        draw_text(SCREEN_W - MARGIN - draw_text_width(status, small),
                  line_y - small->ascent - small->descent - 1, status, small);
    }

    /* history, newest at the bottom, going up to the status bar (drawn over
       the top of the last one) */
    y = line_y - GAP;
    for(i = history_len - 1; i >= 0 && y > top_h(); i--) {
        entry_t *h = &history[i];

        if(h->out != NULL) {
            int base = y - h->out->descent;

            screen_color(COL_FG);
            draw_box(h->out, SCREEN_W - MARGIN - h->out->width, base);
            if(h->ms >= 100) {
                char t[32];

                snprintf(t, sizeof t, "%lu.%lus", h->ms / 1000, h->ms % 1000 / 100);
                screen_color(COL_DIM);
                draw_text(MARGIN, base - small->ascent, t, small);
            }
            y -= h->out->ascent + h->out->descent + 2;
        } else {
            screen_color(COL_DIM);
            draw_text(SCREEN_W - MARGIN - draw_text_width(h->message, small),
                      y - small->ascent - small->descent, h->message, small);
            y -= small->ascent + small->descent + 2;
        }

        if(h->in != NULL) {
            screen_color(COL_FG);
            draw_box(h->in, MARGIN, y - h->in->descent);
            y -= h->in->ascent + h->in->descent + GAP;
        }

        screen_color(COL_DIM);
        screen_hline(MARGIN + 20, y + 1, SCREEN_W - 2 * MARGIN - 40);
        y -= GAP;
    }
    draw_top_bar();

    if(menu >= 0)
        draw_menu();

    screen_show();
}

/* The key that stops a slow answer. K_NONE: not picked yet, nothing stops it */
#define CANCEL_KEY K_NONE

static clock_t busy_next;
static unsigned busy_frame;

/* a dot going round in the status bar (left of the battery) while the engine
   runs, so a slow simplify doesn't look frozen. drawn straight onto the screen,
   swapping buffers made the cursor flash */
static void draw_working(void) {
    /* 2x2 dots round an 8x8 box, clockwise from the top */
    static const unsigned char dots[8][2] = {
        { 3, 0 }, { 5, 1 }, { 6, 3 }, { 5, 5 }, { 3, 6 }, { 1, 5 }, { 0, 3 }, { 1, 1 }
    };
    int x = SCREEN_W - MARGIN - BATTERY_W - 6 - 8, y = (top_h() - 1 - 8) / 2;
    unsigned i;

    /* a head and two dimmer dots behind it, going round */
    screen_direct(true);
    for(i = 0; i < 8; i++) {
        unsigned behind = (busy_frame - i) % 8;

        screen_color(behind == 0 ? COL_ACCENT : behind <= 2 ? COL_DIM : COL_BG);
        screen_fill(x + dots[i][0], y + dots[i][1], 2, 2);
    }
    screen_direct(false);
}

/* pcas_poll, the engine calls this constantly, so check the clock and only
   draw a frame / read the keys 10 times a second */
static bool busy_poll(void) {
    if(clock() < busy_next)
        return false;
    busy_next = clock() + CLOCKS_PER_SEC / 10;
    busy_frame++;
    draw_working();
    return CANCEL_KEY != K_NONE && keys_peek().key == CANCEL_KEY;
}

static void busy_start(void) {
    pcas_cancelled = false;
    busy_frame = 0;
    busy_next = clock() + CLOCKS_PER_SEC / 10;
    draw_working();
}

/* main loop */

void app_run(void) {
    if(!ed_init(&ed))
        return;

    screen_begin();
    keys_begin();
    draw_screen();
    pcas_poll = busy_poll;

    for(;;) {
        keypress_t p = keys_wait();

        if(p.key == K_ENTER && menu < 0 && !second && !ed_is_empty(&ed))
            busy_start();
        if(!handle(p))
            break;
        draw_screen();
    }

    keys_end();
    screen_end();
    clear_history();
    ed_free(&ed);
    id_UnloadAll();
}
