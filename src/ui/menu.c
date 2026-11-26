/* the calculator's navigation: the folder bar under the screen and the menus that open from it */

#include "menu.h"

#include <stdio.h>
#include <string.h>

#include "areas.h"
#include "hover.h"
#include "page.h"
#include "work.h"
#include "platform/screen.h"
#include "render/draw.h"

static const menu_host_t *host;

static int menu = -1;           /* open folder, -1 = none */
static const folder_t *menu_in; /* folders[menu] or one of its subfolders */
static int menu_sel, menu_top;
/* the folders above the one showing and the row picked in each, for going back up.
   the nspire's [menu] starts a level higher, at the list of the five */
#define MENU_DEPTH_MAX 4
static const folder_t *menu_back[MENU_DEPTH_MAX];
static int menu_back_sel[MENU_DEPTH_MAX], menu_depth;
static int menu_x, menu_y, menu_w, menu_row_h, menu_rows; /* the open menu's box, where draw_menu put it, for clicks */

void menu_init(const menu_host_t *h) {
    host = h;
}

bool menu_is_open(void) {
    return menu >= 0;
}

static const item_t *menu_item(int i) {
    return menu_in->items[i];
}

/* rows drawn as what picking them types, buitl on open and freed on close */
static box_t *menu_boxes[32];
static int menu_box_count;

static void menu_free_boxes(void) {
    int i;

    for(i = 0; i < menu_box_count; i++) {
        layout_free(menu_boxes[i]);
        menu_boxes[i] = NULL;
    }
    menu_box_count = 0;
}

static void menu_build_boxes(void) {
    const folder_t *f = menu_in;
    layout_font_t small = draw_font;
    unsigned i;

    /* one size under normal math so it still reads as a menu */
    if(small.base > 0)
        small.base--;
    for(i = 0; i < f->count && i < sizeof menu_boxes / sizeof menu_boxes[0]; i++) {
        const item_t *it = f->items[i];

        menu_boxes[i] = NULL;
        if(it->kind == ITEM_FOLDER || it->kind == ITEM_SETTING || it->kind == ITEM_SOFTKEY || !host->pick(it, true))
            continue; /* subfolders, settings, keys and the greyed out ones stay text */
        /* the power's base is whatever's left of it, so alone it's just a
           floating slot. "x^n" says it better */
        if(strcmp(it->engine, "($)^($)") == 0)
            continue;
        menu_boxes[i] = host->preview(it, &small);
    }
    menu_box_count = (int)i;
}

void menu_free(void) {
    menu_free_boxes();
}

void menu_close(void) {
    menu_free_boxes();
    menu = -1;
}

static void menu_show(const folder_t *f, int sel) {
    menu_free_boxes();
    menu_in = f;
    menu_sel = sel;
    menu_top = 0;
    menu_build_boxes();
}

void menu_open(int folder) {
    menu = folder;
    menu_depth = 0;
    menu_show(&folders[folder], 0);
}

/* nspire: the list of the five, its folders open like subfolders */
void menu_open_top(void) {
    menu = 0;
    menu_depth = 0;
    menu_show(&menu_top_folder, 0);
}

/* nspire: [menu] on a page with keys under the screen lists those five */
static item_t soft_items[5];
static const item_t *soft_list[5];
static const folder_t soft_folder = { NULL, "keys", soft_list, 5 };

void menu_open_soft(const char *const *names) {
    int i;

    for(i = 0; i < 5; i++) {
        soft_items[i].label = names[i];
        soft_items[i].kind = ITEM_SOFTKEY;
        soft_items[i].have = HAVE_YES;
        soft_list[i] = &soft_items[i];
    }
    menu = 0;
    menu_depth = 0;
    menu_show(&soft_folder, 0);
}

/* back up one, false at the top already */
static bool menu_up(void) {
    if(menu_depth == 0)
        return false;
    menu_depth--;
    menu_show(menu_back[menu_depth], menu_back_sel[menu_depth]);
    return true;
}

static void menu_choose(int i) {
    const item_t *it = menu_item(i);

    if(it->kind == ITEM_FOLDER) {
        if(menu_depth < MENU_DEPTH_MAX) {
            menu_back[menu_depth] = menu_in;
            menu_back_sel[menu_depth++] = i;
        }
        menu_show(it->sub, 0);
        return;
    }
    if(it->kind == ITEM_SOFTKEY) {  /* as if it was pressed under the page */
        action_t a = { A_SOFT, i, NULL };

        menu_close();
        if(page_is_open())
            page_act(a);
        else
            work_act(a);
        return;
    }
    if(it->kind != ITEM_SETTING && host->pick(it, true))
        host->will_edit();
    if(!host->pick(it, false)) {
        host->say("not in Fluxion yet");
        return;
    }
    /* settings stay open so several can be toggled. show steps opened its page, hidden under it the menu ate the keys */
    if(it->kind != ITEM_SETTING || work_is_open() || host->said()) /* the menu sits over the status message */
        menu_close();
}

/* left / right go round the five on the CE. the nspire has them in a list, and a page's keys aren't folders */
static bool round_five(void) {
    return !NSPIRE && menu_in == &folders[menu];
}

/* false if it means nothing in a menu. rows past 9 go A, B, C like the nspire */
bool menu_act(action_t a) {
    int count = (int)menu_in->count, n = -1;

    switch(a.op) {
    case A_UP:    menu_sel = (menu_sel + count - 1) % count; break;
    case A_DOWN:  menu_sel = (menu_sel + 1) % count; break;
    case A_LEFT:
        if(!menu_up() && round_five())
            menu_open((menu + FOLDER_COUNT - 1) % FOLDER_COUNT);
        break;
    case A_RIGHT:
        if(menu_item(menu_sel)->kind == ITEM_FOLDER)
            menu_choose(menu_sel);
        else if(round_five())
            menu_open((menu + 1) % FOLDER_COUNT);
        break;
    case A_ENTER: menu_choose(menu_sel); break;
    case A_CLEAR: case A_MENU: menu_close(); break;
    case A_ESC:   /* one level up, out from the top */
        if(!menu_up())
            menu_close();
        break;
    case A_SOFT:
        if(menu_in == &soft_folder)     /* a page's keys listed, so that one */
            menu_choose(a.n);
        else if(a.n == menu)
            menu_close();
        else
            menu_open(a.n);
        break;
    case A_CHAR:
        if(a.n >= '1' && a.n <= '9')
            n = a.n - '1';
        else if(a.n >= 'a' && a.n <= 'z')
            n = 9 + (a.n - 'a');
        else if(a.n >= 'A' && a.n <= 'Z')
            n = 9 + (a.n - 'A');
        else
            return false;
        if(n < count)
            menu_choose(n);
        break;
    default:
        return false;
    }
    return true;
}

bool menu_click(int x, int y, action_t *a) {
    int row = (y - menu_y - 2) / (menu_row_h > 0 ? menu_row_h : 1);

    if(menu < 0 || x < menu_x || x >= menu_x + menu_w || y < menu_y + 2 || row >= menu_rows)
        return false;
    row += menu_top; /* its key: 1 to 9, then a to z */
    a->op = A_CHAR;
    a->n = row < 9 ? '1' + row : 'a' + (row - 9);
    return true;
}

bool menu_bar_click(int x, int y, action_t *a) {
    if(NSPIRE || y < areas.keys.y || y >= areas.keys.y + areas.keys.h)
        return false;
    a->op = A_SOFT;
    a->n = x * FOLDER_COUNT / (areas.keys.w > 0 ? areas.keys.w : 1);
    if(a->n >= FOLDER_COUNT)
        a->n = FOLDER_COUNT - 1;
    return true;
}

void menu_draw_keys(const char *const names[5], int lit, unsigned off) {
    draw_softkeys(&areas.keys, names, lit, off, hover_cell(&areas.keys, 5));
}

void menu_draw_bar(void) {
    const char *names[FOLDER_COUNT];
    int i;

    for(i = 0; i < FOLDER_COUNT; i++)
        names[i] = folders[i].name;
    draw_softkeys(&areas.keys, names, menu, 0, hover_cell(&areas.keys, FOLDER_COUNT));
}

void menu_draw(void) {
    const font_t *font = draw_label_font();
    const folder_t *f = menu_in;
    int row_h = font->ascent + font->descent + 2;
    int bottom = areas_body_bottom() - 1;
    int rows = (int)f->count, max_rows;
    int width = 0, x, top, i;

    /* rows are as wide and as tall as the biggest preview (a fraction makes the
       whole folder taller, fine) */
    for(i = 0; i < rows; i++) {
        const box_t *b = i < menu_box_count ? menu_boxes[i] : NULL;
        int w = b != NULL ? b->width : draw_text_width(f->items[i]->label, font);
        int h = b != NULL ? b->ascent + b->descent + 2 : 0;

        if(f->items[i]->kind == ITEM_SETTING)
            w += draw_text_width("[x] ", font); /* its drawn with the box in front so measure that too */
        if(w > width)
            width = w;
        if(h > row_h)
            row_h = h;
    }
    width += 34;    /* for the "1:" plus room for a * or [x] */
    max_rows = (bottom - areas.body.y - 4) / row_h;

    if(rows > max_rows)
        rows = max_rows;
    if(menu_sel < menu_top)
        menu_top = menu_sel;
    if(menu_sel >= menu_top + rows)
        menu_top = menu_sel - rows + 1;

    if(NSPIRE) {   /* down from the top, a step right for each level like the nspire's own */
        x = areas.body.x + 2 + menu_depth * 14;
        top = areas.body.y + 1;
        bottom = top + rows * row_h + 2;
    } else {
        x = areas.keys.x + menu * (areas.keys.w / FOLDER_COUNT); /* over its key */
        top = bottom - rows * row_h - 2;
    }
    if(x + width > areas.body.x + areas.body.w - 1)
        x = areas.body.x + areas.body.w - 1 - width;

    menu_x = x;
    menu_y = top;
    menu_w = width;
    menu_row_h = row_h;
    menu_rows = rows;
    screen_color(COL_BG);
    screen_fill(x, top, width, bottom - top + 1);
    screen_color(COL_DIM);
    screen_hline(x, top, width);
    if(NSPIRE) /* nothing under it to close it off */
        screen_hline(x, bottom, width);
    screen_vline(x, top, bottom - top + 1);
    screen_vline(x + width - 1, top, bottom - top + 1);

    for(i = 0; i < rows; i++) {
        int n = menu_top + i, y = top + 2 + i * row_h;
        int text_y = y + (row_h - 2 - font->ascent - font->descent) / 2;
        const item_t *it = f->items[n];
        bool can = it->kind == ITEM_FOLDER || it->kind == ITEM_SOFTKEY || host->pick(it, true);
        int on = it->kind == ITEM_SETTING ? host->checked(it) : -1;
        char label[48];

        if(on >= 0)
            snprintf(label, sizeof label, "%s %s", on ? "[x]" : "[ ]", it->label);
        else
            snprintf(label, sizeof label, "%s%s", it->label, can ? "" : " *");

        if(n == menu_sel) {
            screen_color(COL_ACCENT);
            screen_fill(x + 1, y - 1, width - 2, row_h);
            screen_color(COL_BG);
        } else {
            if(hover_in(x + 1, y, width - 2, row_h)) {
                screen_color(COL_SELECT);
                screen_fill(x + 1, y - 1, width - 2, row_h);
            }
            screen_color(can ? COL_FG : COL_DIM);
        }

        if(n < 9 + 26) {
            char num[3] = { (char)(n < 9 ? '1' + n : 'A' + (n - 9)), ':', '\0' };
            draw_text(x + 3, text_y, num, font);
        }
        if(it->kind == ITEM_FOLDER)
            draw_text(x + width - 3 - draw_text_width(">", font), text_y, ">", font);
        if(n < menu_box_count && menu_boxes[n] != NULL) {
            const box_t *b = menu_boxes[n];

            draw_box(b, x + 18, y + (row_h - 2 - b->ascent - b->descent) / 2 + b->ascent);
        } else {
            draw_text(x + 18, text_y, label, font);
        }
    }
}

