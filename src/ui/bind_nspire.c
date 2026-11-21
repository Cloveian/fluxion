#ifdef _TINSPIRE

/* the nspire's keys, doing what they do in its own OS (the CX II guidebook's shortcut list) */

#include "bind.h"
#include "editor/editor.h"

#define CHAR(k, m, c)   { k, m, { A_CHAR, c, NULL } }
#define FUNC(k, m, s)   { k, m, { A_FUNC, 0, s } }
#define DO(k, m, op, n) { k, m, { op, n, NULL } }
#define LETTER(c)       (keycode_t)(K_LETTER_A + (c) - 'a')

static const char euler[] = { CH_EULER, '\0' };

static const bind_t binds[] = {
    CHAR(K_DOT, 0, '.'),        CHAR(K_NEG, 0, ED_NEG),
    CHAR(K_ADD, 0, ED_PLUS),    CHAR(K_SUB, 0, ED_SUB),
    CHAR(K_MUL, 0, ED_TIMES),   CHAR(K_DIV, 0, ED_DIVIDE),
    CHAR(K_LPAREN, 0, '('),     CHAR(K_RPAREN, 0, ')'),
    CHAR(K_COMMA, 0, ','),      CHAR(K_EQUALS, 0, '='),
    CHAR(K_EE, 0, ED_SCI),
    DO(K_POW, 0, A_TEMPLATE, ED_POW),
    { K_SQUARE, 0, { A_POWER, 0, "2" } },
    { K_EX, 0, { A_TYPE, ED_POW, euler } },
    { K_TENX, 0, { A_TYPE, ED_POW, "10" } },
    FUNC(K_SIN, 0, "sin"), FUNC(K_COS, 0, "cos"), FUNC(K_TAN, 0, "tan"),
    DO(K_TRIG, 0, A_POPUP, POPUP_TRIG),
    DO(K_PI, 0, A_POPUP, POPUP_CONSTANTS),
    DO(K_MARKS, 0, A_POPUP, POPUP_MATH),    /* the OS's has ? ! $ and such, mine has ! and the degree */
    DO(K_LEFT, 0, A_LEFT, 0), DO(K_RIGHT, 0, A_RIGHT, 0), DO(K_UP, 0, A_UP, 0), DO(K_DOWN, 0, A_DOWN, 0),
    DO(K_DEL, 0, A_BACKSPACE, 0), DO(K_ESC, 0, A_ESC, 0),
    DO(K_ENTER, 0, A_ENTER, 0), DO(K_RETURN, 0, A_ENTER, 0), DO(K_CLICK, 0, A_ENTER, 0),
    DO(K_MENU, 0, A_MENU, 0), DO(K_VARS, 0, A_VARS, 0), DO(K_CAT, 0, A_CATALOG, 0),
    DO(K_TEMPLATE, 0, A_SOFT, 3),   /* the Templates folder */
    DO(K_DOC, 0, A_SETTINGS, 0), DO(K_HOME, 0, A_QUIT, 0),
    DO(K_CTRL, 0, A_MOD, M_CTRL), DO(K_SHIFT, 0, A_MOD, M_SHIFT),

    DO(K_EQUALS, M_CTRL, A_POPUP, POPUP_MATH),
    DO(K_CAT, M_CTRL, A_POPUP, POPUP_MATH),
    DO(K_SQUARE, M_CTRL, A_TEMPLATE, ED_SQRT),
    DO(K_POW, M_CTRL, A_TEMPLATE, ED_NROOT),
    DO(K_DIV, M_CTRL, A_TEMPLATE, ED_FRAC),
    FUNC(K_EX, M_CTRL, "ln"), FUNC(K_TENX, M_CTRL, "log"),
    { K_NEG, M_CTRL, { A_TYPE, 0, "ans" } },
    CHAR(K_VARS, M_CTRL, CH_STO),
    DO(K_ENTER, M_CTRL, A_DECIMAL, 0), DO(K_RETURN, M_CTRL, A_DECIMAL, 0),   /* the OS's approx */
    DO(K_DEL, M_CTRL, A_CLEAR_LINE, 1),
    DO(K_ESC, M_CTRL, A_UNDO, 0), DO(LETTER('z'), M_CTRL, A_UNDO, 0),
    DO(LETTER('x'), M_CTRL, A_CUT, 0), DO(LETTER('c'), M_CTRL, A_COPY, 0), DO(LETTER('v'), M_CTRL, A_PASTE, 0),
    DO(K_7, M_CTRL, A_LINE_START, 0), DO(K_1, M_CTRL, A_LINE_END, 0),     /* top / end of page */
    DO(K_ADD, M_CTRL, A_BRIGHTNESS, -1), DO(K_SUB, M_CTRL, A_BRIGHTNESS, 1),
    DO(K_MENU, M_CTRL, A_MENU, 0),
    DO(K_HOME, M_CTRL, A_SCREEN_OFF, 0),

    DO(K_LEFT, M_SHIFT, A_SELECT_LEFT, 0), DO(K_RIGHT, M_SHIFT, A_SELECT_RIGHT, 0),
    { K_ADD, M_SHIFT, { A_ITEM, 0, "integral" } },
    { K_SUB, M_SHIFT, { A_ITEM, 0, "deriv" } },
};

const keypad_t keypad = {
    binds, sizeof binds / sizeof binds[0],
    { M_SHIFT, 0 },     /* ctrl + a key with nothing on it does nothing */
    M_SHIFT, 0,
    { M_CTRL, M_SHIFT, 0 },
    "esc",
};

#else
typedef int bind_nspire_unused; /* nothing here off the nspire */
#endif
