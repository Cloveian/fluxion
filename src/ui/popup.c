/* the little grids of symbols and fuctions that open at the cursor: [math], the nspire's [trig], pi and ?!> keys */

#include "popup.h"

#include "areas.h"
#include "hover.h"
#include "editor/editor.h"
#include "platform/screen.h"
#include "render/draw.h"

static const popup_host_t *host;

/* [math] pops up the plain characters with no key of their own, in a little grid that
   pops up at the cursor. 1-9 and 0 pick them straight away */

static const char popup_chars[] = {
    '=', CH_NE, '<', '>', CH_LE,
    CH_GE, '!', CH_INF, CH_DEG, CH_THETA
};

/* the nspire's [trig], all 12. an a in front draws as ^-1 */
static const char *const popup_trig[] = {
    "sin", "cos", "tan", "csc", "sec", "cot",
    "asin", "acos", "atan", "acsc", "asec", "acot"
};

/* the nspire's pi key */
static const char popup_constants[] = { CH_PI, CH_EULER, CH_IMAG, CH_INF, CH_THETA, CH_DEG };

/* the nspire's ?!> key */
static const char popup_marks[] = { '!', '%', CH_DEG };

/* one grid of characters, or one of functions */
typedef struct {
    const char *chars;
    const char *const *funcs;
    int count, cols, cell_w;
} popup_set_t;

static const popup_set_t math_set = { popup_chars, NULL, (int)sizeof popup_chars, 5, 22 };
static const popup_set_t trig_set = { NULL, popup_trig, (int)(sizeof popup_trig / sizeof popup_trig[0]), 6, 44 };
static const popup_set_t constants_set = { popup_constants, NULL, (int)sizeof popup_constants, 6, 22 };
static const popup_set_t marks_set = { popup_marks, NULL, (int)sizeof popup_marks, 3, 22 };

static const popup_set_t *pset = &math_set;
static int popup = -1; /* selected one, -1 = closed */
static int pop_x, pop_y, pop_cell_w = 1, pop_cell_h = 1; /* where draw_popup put it, for clicks */

static const popup_set_t *const popup_sets[] = { &math_set, &trig_set, &constants_set, &marks_set };

void popup_init(const popup_host_t *h) {
    host = h;
}

bool popup_is_open(void) {
    return popup >= 0;
}

void popup_open(int which) {
    pset = popup_sets[which];
    popup = 0;
}

static void popup_pick(int i) {
    host->will_edit();
    if(pset->funcs != NULL)
        host->insert_func(pset->funcs[i]);
    else
        host->insert_char(pset->chars[i]);
    popup = -1;
}

action_t popup_click(int x, int y) {
    action_t a = { A_ESC, 0, NULL, 0 };
    int col = (x - pop_x) / pop_cell_w, line = (y - pop_y) / pop_cell_h;
    int i = line * pset->cols + col;

    if(x >= pop_x && y >= pop_y && col < pset->cols && i < pset->count) {
        a.op = A_POPUP_PICK;
        a.n = i;
    }
    return a;
}

/* false if it means nothing in a popup */
bool popup_act(action_t a) {
    int n = pset->count;

    switch(a.op) {
    case A_LEFT:  popup = (popup + n - 1) % n; break;
    case A_RIGHT: popup = (popup + 1) % n; break;
    case A_UP: case A_DOWN:
        popup = (popup + pset->cols) % n;
        break;
    case A_ENTER: popup_pick(popup); break;
    case A_POPUP_PICK: popup_pick(a.n); break;
    case A_CLEAR: case A_ESC: case A_POPUP: popup = -1; break;
    case A_CHAR:
        if(a.n >= '1' && a.n <= '9' && a.n - '1' < n)
            popup_pick(a.n - '1');
        else if(a.n == '0' && n >= 10)
            popup_pick(9);
        else
            return false;
        break;
    default:
        return false;
    }
    return true;
}

/* the [math] or [trig] grid, just above the cursor, each cell numbered in the corner */
void popup_draw(int cur_x, int cur_top) {
    const font_t *font = draw_font_for(draw_font.base), *tiny = draw_font_for(0);
    int cols = pset->cols, cell_w = pset->cell_w, cell_h = font->ascent + font->descent + 6;
    int rows = (pset->count + cols - 1) / cols;
    int w = cols * cell_w + 1, h = rows * cell_h + 1;
    int x = cur_x - w / 2, y = cur_top - h - 2, i;

    if(x < areas.body.x + 1)
        x = areas.body.x + 1;
    if(x + w > areas.body.x + areas.body.w - 1)
        x = areas.body.x + areas.body.w - 1 - w;
    if(y < areas.body.y)
        y = areas.body.y;
    pop_x = x;
    pop_y = y;
    pop_cell_w = cell_w;
    pop_cell_h = cell_h;

    screen_color(COL_BG);
    screen_fill(x, y, w, h);
    screen_color(COL_DIM);
    screen_hline(x, y, w);
    screen_hline(x, y + h - 1, w);
    screen_vline(x, y, h);
    screen_vline(x + w - 1, y, h);

    for(i = 0; i < pset->count; i++) {
        int cx = x + (i % cols) * cell_w, cy = y + (i / cols) * cell_h, lw, lx;
        char s[2] = { 0, '\0' }, num[2] = { (char)(i < 9 ? '1' + i : '0'), '\0' }, minus_one[3] = { CH_MINUS, '1', '\0' };
        const char *label = s;
        bool inv = false;

        if(pset->funcs != NULL) {
            label = pset->funcs[i];
            inv = label[0] == 'a';  /* asin as sin^-1 */
            label += inv;
        } else {
            s[0] = pset->chars[i];
        }
        lw = draw_text_width(label, font) + (inv ? draw_text_width(minus_one, tiny) : 0);
        lx = cx + (cell_w - lw) / 2 + 2;

        if(i == popup) {
            screen_color(COL_ACCENT);
            screen_fill(cx + 1, cy + 1, cell_w - 1, cell_h - 1);
        } else if(hover_in(cx, cy, cell_w, cell_h)) {
            screen_color(COL_SELECT);
            screen_fill(cx + 1, cy + 1, cell_w - 1, cell_h - 1);
        }
        screen_color(i == popup ? COL_BG : COL_DIM);
        if(i < 10)  /* 1-9 and 0 pick, past that it's the arrows */
            draw_text(cx + 2, cy + 1, num, tiny);
        screen_color(i == popup ? COL_BG : COL_FG);
        draw_text(lx, cy + 4, label, font);
        if(inv)
            draw_text(lx + draw_text_width(label, font), cy + 1, minus_one, tiny);
    }
}

