#ifndef __TICE__

/* the browser build, for the showcase page. stands in for keys_pc.c, events_pc.c and window_pc.c
   (no SDL in there), screen_pc.c and store_pc.c work as is. the page hands keys in
   with web_press() and web_point() and gets the framebuffer back every window_present().
   the key names up top are the PC's too, keys_pc_script.c's scripts read them */

#include <string.h>

#include "keys.h"

static const struct { const char *name; keycode_t key; } names[] = {
    { "y=", K_YEQU }, { "window", K_WINDOW }, { "zoom", K_ZOOM }, { "trace", K_TRACE }, { "graph", K_GRAPH },
    { "2nd", K_2ND }, { "mode", K_MODE }, { "del", K_DEL },
    { "alpha", K_ALPHA }, { "xt", K_XT }, { "stat", K_STAT },
    { "math", K_MATH }, { "apps", K_APPS }, { "prgm", K_PRGM }, { "vars", K_VARS }, { "clear", K_CLEAR },
    { "x^-1", K_RECIP }, { "sin", K_SIN }, { "cos", K_COS }, { "tan", K_TAN },
    { "x^2", K_SQUARE }, { "log", K_LOG }, { "ln", K_LN }, { "sto", K_STO },
    { "neg", K_NEG }, { "enter", K_ENTER }, { "on", K_ON },
    { "up", K_UP }, { "down", K_DOWN }, { "left", K_LEFT }, { "right", K_RIGHT },
    /* the nspire's, so scripts can try them and the page's nspire keypad has something to press */
    { "menu", K_MENU }, { "trig", K_TRIG }, { "esc", K_ESC }, { "doc", K_DOC }, { "home", K_HOME },
    { "cat", K_CAT }, { "template", K_TEMPLATE }, { "pi", K_PI }, { "ee", K_EE }, { "?!", K_MARKS },
    { "10^x", K_TENX }, { "e^x", K_EX }, { "return", K_RETURN }, { "click", K_CLICK },
    { "ctrl", K_CTRL }, { "shift", K_SHIFT },
};

keycode_t keys_from_char(char c) {
    if(c >= '0' && c <= '9')
        return (keycode_t)(K_0 + (c - '0'));
    if(c >= 'a' && c <= 'z')
        return (keycode_t)(K_LETTER_A + (c - 'a'));
    if(c >= 'A' && c <= 'Z')
        return (keycode_t)(K_CAPITAL_A + (c - 'A'));

    switch(c) {
    case '+': return K_ADD;
    case '-': return K_SUB;
    case '*': return K_MUL;
    case '/': return K_DIV;
    case '^': return K_POW;
    case '(': return K_LPAREN;
    case ')': return K_RPAREN;
    case ',': return K_COMMA;
    case '=': return K_EQUALS;  /* the CE has no such key, its page types = as [2nd][math] */
    case '.': return K_DOT;
    case '~': return K_NEG;
    case '<': return K_LESS;
    case '>': return K_GREATER;
    case '!': return K_BANG;
    case '%': return K_PERCENT;
    default:  return K_NONE;
    }
}

keycode_t keys_named(const char *name) {
    size_t i;

    for(i = 0; i < sizeof names / sizeof names[0]; i++)
        if(strcmp(name, names[i].name) == 0)
            return names[i].key;
    if(name[0] == '\0' || name[1] != '\0')
        return K_NONE;
    return keys_from_char(name[0]);
}

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

#include "screen.h"
#include "window_pc.h"

#define QUEUE 64
static keypress_t queue[QUEUE];
static int head, count;

EMSCRIPTEN_KEEPALIVE
void web_press(int key, int on, int second, int alpha) {
    keypress_t p;

    if(count == QUEUE)
        return;     /* someone's mashing, they won't miss it */
    p.key = (keycode_t)key;
#ifdef _TINSPIRE
    (void)alpha;    /* two modifiers there, the page sends them as on and second */
    p.held = (on ? M_CTRL : 0) | (second ? M_SHIFT : 0);
#else
    p.held = (on ? M_ON : 0) | (second ? M_2ND : 0) | (alpha ? M_ALPHA : 0);
#endif
    queue[(head + count++) % QUEUE] = p;
}

/* the mouse over the screen, x and y in its 320x240 (below 0 once it left). kind 0 down, 1 drag, 2 move with no button, shift is shift-click */
EMSCRIPTEN_KEEPALIVE
void web_point(int kind, int x, int y, int shift) {
    keypress_t *p;
    keycode_t key = kind == 0 ? K_POINT_DOWN : kind == 1 ? K_POINT_DRAG : K_POINT_MOVE;

    /* a drag right after a drag replaces it, same for moves, only where it ended counts (keys_pc.c does the same) */
    if(kind != 0 && count > 0 && queue[(head + count - 1) % QUEUE].key == key)
        count--;
    if(count == QUEUE)
        return;
    p = &queue[(head + count++) % QUEUE];
    p->key = key;
    p->held = shift ? M_ALPHA : 0;  /* the nspire's app reads M_ALPHA for this too */
    p->x = (short)x;
    p->y = (short)y;
}

EMSCRIPTEN_KEEPALIVE
int web_key(const char *name) {
    return keys_named(name);
}

void keys_begin(void) {}
void keys_end(void) {}
void keys_script(char *const *presses, int n) { (void)presses; (void)n; }

void (*keys_on_sleep)(bool asleep) = NULL;
void keys_sleep(void) {}    /* no backlight, the tab going away is the off button */

keypress_t keys_wait(void) {
    keypress_t p;

    /* asyncify unwinds back to the browser here and picks up again 10 ms later */
    while(count == 0)
        emscripten_sleep(10);
    p = queue[head];
    head = (head + 1) % QUEUE;
    count--;
    return p;
}

/* the engine never gives the browser a turn while it works so nothing new can be
   down. means no [on] cancel in here, fine at PC speed */
keypress_t keys_peek(void) {
    keypress_t p = { K_NONE, 0 };
    return p;
}

EM_JS(void, present, (const unsigned char *fb), {
    if(Module.present) Module.present(fb);
});

bool window_open(const char *title) { (void)title; return true; }
void window_present(void) { present(screen_pixels()); }
void window_close(void) {}
int window_show(const char *title) { (void)title; window_present(); return 0; }

/* the page turns palette indices into colours with this, the theme can change them */
EMSCRIPTEN_KEEPALIVE
unsigned long web_rgb(int color) {
    unsigned char c[3];

    screen_rgb((unsigned char)color, c);
    return (unsigned long)c[0] << 16 | (unsigned long)c[1] << 8 | c[2];
}

#endif
#else
typedef int web_unused; /* nothing here on the calculator */
#endif
