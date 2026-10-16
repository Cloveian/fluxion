#include "page.h"

#include <string.h>

#include "platform/screen.h"
#include "render/draw.h"

#define PAGE_DEPTH  4
#define HELP_LINES  4

typedef struct {
    const page_t *page;
    int sel, top;
} open_t;

static open_t stack[PAGE_DEPTH];
static int depth;
static int shown_rows = 1;  /* rows that fit last time it was drawn, for paging */

void page_open(const page_t *page) {
    if(depth == PAGE_DEPTH)
        return;     /* 4 deep is already silly lol */
    stack[depth].page = page;
    stack[depth].sel = 0;
    stack[depth].top = 0;
    depth++;
}

void page_close(void) {
    if(depth > 0)
        depth--;
}

bool page_is_open(void) {
    return depth > 0;
}

static char lower(char c) {
    return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c;
}

void page_key(keypress_t p, char letter) {
    open_t *o = &stack[depth - 1];
    int n = o->page->count;

    if(letter != 0) {
        /* next row starting with it after the current one, so pressing the
           letter again walks through them (like the TI's catalog) */
        int i;

        for(i = 1; i <= n; i++) {
            int k = (o->sel + i) % n;
            const char *s = o->page->label(k);

            if(lower(s[0]) == lower(letter)) {
                o->sel = k;
                break;
            }
        }
        return;
    }

    switch(p.key) {
    case K_UP:    o->sel = (o->sel + n - 1) % n; break;
    case K_DOWN:  o->sel = (o->sel + 1) % n; break;
    case K_LEFT:  /* a page at a time */
        o->sel = o->sel > shown_rows ? o->sel - shown_rows : 0;
        break;
    case K_RIGHT:
        o->sel = o->sel + shown_rows < n - 1 ? o->sel + shown_rows : n - 1;
        break;
    case K_ENTER:
        if(o->page->pick != NULL && o->page->pick(o->sel))
            page_close();
        else if(o->sel >= o->page->count)  /* it deleted the bottom row */
            o->sel = o->page->count - 1;
        break;
    case K_CLEAR:
        page_close();
        break;
    default:
        break;
    }
}

/* word wraps text into w pixels, at most lines lines. returns how many it used */
static int wrapped(int x, int y, int w, const char *text, const font_t *font, int lines) {
    int line_h = font->ascent + font->descent + 1, used = 0;
    char buf[64];

    while(*text != '\0' && used < lines) {
        int len = 0, fit = 0;

        /* the most whole words that fit (or one cut word if it's that long) */
        while(text[len] != '\0' && len < (int)sizeof buf - 1) {
            buf[len] = text[len];
            buf[len + 1] = '\0';
            if(draw_text_width(buf, font) > w)
                break;
            len++;
            if(text[len] == ' ' || text[len] == '\0')
                fit = len;
        }
        if(fit == 0)
            fit = len > 0 ? len : 1;

        memcpy(buf, text, fit);
        buf[fit] = '\0';
        draw_text(x, y + used * line_h, buf, font);
        used++;

        text += fit;
        while(*text == ' ')
            text++;
    }
    return used;
}

void page_draw(int top) {
    const open_t *o = &stack[depth - 1];
    const page_t *pg = o->page;
    const font_t *font = draw_label_font(), *big = draw_font_for(draw_font.base);
    int row_h = font->ascent + font->descent + 2;
    int help_h = HELP_LINES * (font->ascent + font->descent + 1) + 4;
    int list_top = top + big->ascent + big->descent + 4;
    int list_bottom = SCREEN_H - help_h - 2;
    int rows, i;
    const char *help;

    /* title */
    screen_color(COL_ACCENT);
    draw_text(4, top + 1, pg->title, big);
    screen_color(COL_DIM);
    screen_hline(4, list_top - 2, SCREEN_W - 8);

    rows = (list_bottom - list_top) / row_h;
    if(rows < 1)
        rows = 1;
    shown_rows = rows;

    /* scroll so the selection shows (const page, the stack entry is ours) */
    if(o->sel < o->top)
        stack[depth - 1].top = o->sel;
    if(o->sel >= o->top + rows)
        stack[depth - 1].top = o->sel - rows + 1;

    for(i = 0; i < rows && o->top + i < pg->count; i++) {
        int n = o->top + i, y = list_top + i * row_h;
        const char *value = pg->value != NULL ? pg->value(n) : NULL;

        if(n == o->sel) {
            screen_color(COL_ACCENT);
            screen_fill(2, y - 1, SCREEN_W - 4, row_h);
        }
        screen_color(n == o->sel ? COL_BG : COL_FG);
        draw_text(6, y, pg->label(n), font);
        if(value != NULL) {
            screen_color(n == o->sel ? COL_BG : COL_DIM);
            draw_text(SCREEN_W - 6 - draw_text_width(value, font), y, value, font);
        }
    }

    /* the scrollbar, only if it doesn't all fit */
    if(pg->count > rows) {
        int bar_h = (list_bottom - list_top) * rows / pg->count;
        int bar_y = list_top + (list_bottom - list_top - bar_h) * o->top / (pg->count - rows);

        screen_color(COL_DIM);
        screen_vline(SCREEN_W - 2, bar_y, bar_h > 2 ? bar_h : 2);
    }

    /* help for the highlighted row */
    screen_color(COL_DIM);
    screen_hline(4, list_bottom + 1, SCREEN_W - 8);
    help = pg->help != NULL ? pg->help(o->sel) : NULL;
    if(help != NULL) {
        screen_color(COL_FG);
        wrapped(6, list_bottom + 4, SCREEN_W - 12, help, font, HELP_LINES);
    }
}
