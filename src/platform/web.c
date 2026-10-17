#ifndef __TICE__

/* the browser build, for the showcase page. stands in for keys_pc.c and window_pc.c
   (no SDL in there), screen_pc.c and store_pc.c work as is. the page hands keys in
   with web_press() and gets the framebuffer back every window_present().
   the key names up top are the PC's too, keys_pc.c's scripts read them */

#include <string.h>

#include "keys.h"

static const struct { const char *name; key_t key; } names[] = {
    { "y=", K_YEQU }, { "window", K_WINDOW }, { "zoom", K_ZOOM }, { "trace", K_TRACE }, { "graph", K_GRAPH },
    { "2nd", K_2ND }, { "mode", K_MODE }, { "del", K_DEL },
    { "alpha", K_ALPHA }, { "xt", K_XT }, { "stat", K_STAT },
    { "math", K_MATH }, { "apps", K_APPS }, { "prgm", K_PRGM }, { "vars", K_VARS }, { "clear", K_CLEAR },
    { "x^-1", K_RECIP }, { "sin", K_SIN }, { "cos", K_COS }, { "tan", K_TAN },
    { "x^2", K_SQUARE }, { "log", K_LOG }, { "ln", K_LN }, { "sto", K_STO },
    { "neg", K_NEG }, { "enter", K_ENTER }, { "on", K_ON },
    { "up", K_UP }, { "down", K_DOWN }, { "left", K_LEFT }, { "right", K_RIGHT },
};

key_t keys_from_char(char c) {
    if(c >= '0' && c <= '9')
        return (key_t)(K_0 + (c - '0'));
    if(c >= 'a' && c <= 'z')
        return (key_t)(K_LETTER_A + (c - 'a'));
    if(c >= 'A' && c <= 'Z')
        return (key_t)(K_CAPITAL_A + (c - 'A'));

    switch(c) {
    case '+': return K_ADD;
    case '-': return K_SUB;
    case '*': return K_MUL;
    case '/': return K_DIV;
    case '^': return K_POW;
    case '(': return K_LPAREN;
    case ')': return K_RPAREN;
    case ',': return K_COMMA;
    case '.': return K_DOT;
    case '~': return K_NEG;
    default:  return K_NONE;
    }
}

key_t keys_named(const char *name) {
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
    p.key = (key_t)key;
    p.on = on;
    p.second = second;
    p.alpha = alpha;
    queue[(head + count++) % QUEUE] = p;
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
    keypress_t p = { K_NONE, false, false, false };
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
