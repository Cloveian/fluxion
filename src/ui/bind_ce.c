#ifndef _TINSPIRE

/* the CE's keys, what they print on them. the PC and the web keypad stand in for these */

#include "bind.h"
#include "editor/editor.h"

#define CHAR(k, m, c)   { k, m, { A_CHAR, c, NULL } }
#define FUNC(k, m, s)   { k, m, { A_FUNC, 0, s } }
#define DO(k, m, op, n) { k, m, { op, n, NULL } }
#define LETTER(c)       ((keycode_t)(K_LETTER_A + (c) - 'a'))

static const char neg_one[] = { ED_NEG, '1', '\0' }, euler[] = { CH_EULER, '\0' };

static const bind_t binds[] = {
    /* plain */
    CHAR(K_DOT, 0, '.'),        CHAR(K_NEG, 0, ED_NEG),
    CHAR(K_ADD, 0, ED_PLUS),    CHAR(K_SUB, 0, ED_SUB),
    CHAR(K_MUL, 0, ED_TIMES),   CHAR(K_DIV, 0, ED_DIVIDE),
    CHAR(K_LPAREN, 0, '('),     CHAR(K_RPAREN, 0, ')'),
    CHAR(K_COMMA, 0, ','),      CHAR(K_XT, 0, 'x'),
    CHAR(K_STO, 0, CH_STO),
    DO(K_POW, 0, A_TEMPLATE, ED_POW),
    { K_SQUARE, 0, { A_POWER, 0, "2" } },
    { K_RECIP, 0, { A_POWER, 0, neg_one } },
    FUNC(K_SIN, 0, "sin"), FUNC(K_COS, 0, "cos"), FUNC(K_TAN, 0, "tan"),
    FUNC(K_LN, 0, "ln"),   FUNC(K_LOG, 0, "log"),
    DO(K_LEFT, 0, A_LEFT, 0), DO(K_RIGHT, 0, A_RIGHT, 0), DO(K_UP, 0, A_UP, 0), DO(K_DOWN, 0, A_DOWN, 0),
    DO(K_CLEAR, 0, A_CLEAR, 0), DO(K_DEL, 0, A_DELETE, 0), DO(K_ENTER, 0, A_ENTER, 0),
    DO(K_YEQU, 0, A_SOFT, 0), DO(K_WINDOW, 0, A_SOFT, 1), DO(K_ZOOM, 0, A_SOFT, 2),
    DO(K_TRACE, 0, A_SOFT, 3), DO(K_GRAPH, 0, A_SOFT, 4),
    DO(K_MATH, 0, A_POPUP, POPUP_MATH),
    DO(K_VARS, 0, A_VARS, 0), DO(K_MODE, 0, A_SETTINGS, 0), DO(K_QUIT, 0, A_QUIT, 0),
    DO(K_2ND, 0, A_MOD, M_2ND), DO(K_ALPHA, 0, A_MOD, M_ALPHA), DO(K_ON, 0, A_MOD, M_ON),
    /* the PC's keyboard has these, the CE doesn't */
    CHAR(K_LESS, 0, '<'), CHAR(K_GREATER, 0, '>'), CHAR(K_BANG, 0, '!'), CHAR(K_PERCENT, 0, '%'),
    DO(K_HOME, 0, A_LINE_START, 0), DO(K_END, 0, A_LINE_END, 0),
    /* no such keys on a CE, the PC's scripts try the nspire's menu and popup with them */
    DO(K_MENU, 0, A_MENU, 0), DO(K_TRIG, 0, A_POPUP, POPUP_TRIG),

    /* [2nd], what's printed above */
    DO(K_MODE, M_2ND, A_QUIT, 0),
    DO(K_LEFT, M_2ND, A_LINE_START, 0), DO(K_RIGHT, M_2ND, A_LINE_END, 0),
    DO(K_UP, M_2ND, A_BRIGHTNESS, -1), DO(K_DOWN, M_2ND, A_BRIGHTNESS, 1),
    DO(K_0, M_2ND, A_CATALOG, 0),
    DO(K_DEL, M_2ND, A_DELETE, 0),
    DO(K_ENTER, M_2ND, A_NEXT_FORM, 0),
    DO(K_CLEAR, M_2ND, A_CLEAR_LINE, 0),
    CHAR(K_POW, M_2ND, CH_PI),  CHAR(K_DIV, M_2ND, CH_EULER),
    CHAR(K_DOT, M_2ND, CH_IMAG), CHAR(K_COMMA, M_2ND, ED_SCI),
    CHAR(K_MATH, M_2ND, '='),   /* TI's [2nd][math] is TEST, = is first */
    DO(K_SQUARE, M_2ND, A_TEMPLATE, ED_SQRT),
    FUNC(K_SIN, M_2ND, "asin"), FUNC(K_COS, M_2ND, "acos"), FUNC(K_TAN, M_2ND, "atan"),
    { K_LN, M_2ND, { A_TYPE, ED_POW, euler } },
    { K_LOG, M_2ND, { A_TYPE, ED_POW, "10" } },

    /* [alpha], the green letters */
    DO(K_LEFT, M_ALPHA, A_SELECT_LEFT, 0), DO(K_RIGHT, M_ALPHA, A_SELECT_RIGHT, 0),
    DO(K_ENTER, M_ALPHA, A_DECIMAL, 0),
    DO(K_YEQU, M_ALPHA, A_TEMPLATE, ED_FRAC),   /* n/d where the TI has it */
    DO(K_XT, M_ALPHA, A_TEMPLATE, ED_FRAC),     /* and right there */
    CHAR(K_MATH, M_ALPHA, 'a'),  CHAR(K_APPS, M_ALPHA, 'b'),   CHAR(K_PRGM, M_ALPHA, 'c'),
    CHAR(K_RECIP, M_ALPHA, 'd'), CHAR(K_SIN, M_ALPHA, 'e'),    CHAR(K_COS, M_ALPHA, 'f'),
    CHAR(K_TAN, M_ALPHA, 'g'),   CHAR(K_POW, M_ALPHA, 'h'),    CHAR(K_SQUARE, M_ALPHA, 'i'),
    CHAR(K_COMMA, M_ALPHA, 'j'), CHAR(K_LPAREN, M_ALPHA, 'k'), CHAR(K_RPAREN, M_ALPHA, 'l'),
    CHAR(K_DIV, M_ALPHA, 'm'),   CHAR(K_LOG, M_ALPHA, 'n'),    CHAR(K_7, M_ALPHA, 'o'),
    CHAR(K_8, M_ALPHA, 'p'),     CHAR(K_9, M_ALPHA, 'q'),      CHAR(K_MUL, M_ALPHA, 'r'),
    CHAR(K_LN, M_ALPHA, 's'),    CHAR(K_4, M_ALPHA, 't'),      CHAR(K_5, M_ALPHA, 'u'),
    CHAR(K_6, M_ALPHA, 'v'),     CHAR(K_SUB, M_ALPHA, 'w'),    CHAR(K_STO, M_ALPHA, 'x'),
    CHAR(K_1, M_ALPHA, 'y'),     CHAR(K_2, M_ALPHA, 'z'),      CHAR(K_3, M_ALPHA, CH_THETA),

    /* [on] is ctrl. 1 2 3 cut copy paste, next to each other like X C V */
    DO(K_1, M_ON, A_CUT, 0), DO(K_2, M_ON, A_COPY, 0), DO(K_3, M_ON, A_PASTE, 0),
    DO(K_STO, M_ON, A_UNDO, 0),
    /* and on the PC's keyboard the usual ones. Ctrl + a letter */
    DO(LETTER('x'), M_ON, A_CUT, 0), DO(LETTER('c'), M_ON, A_COPY, 0), DO(LETTER('v'), M_ON, A_PASTE, 0),
    DO(LETTER('z'), M_ON, A_UNDO, 0), DO(LETTER('a'), M_ON, A_SELECT_ALL, 0),
    DO(K_CLEAR, M_ON, A_CLEAR_LINE, 1),     /* Ctrl+Backspace */
    DO(K_MODE, M_ON, A_CANCEL, 0),          /* plain [on] is the TI's own break key, dont */
    FUNC(K_SIN, M_ON, "csc"), FUNC(K_COS, M_ON, "sec"), FUNC(K_TAN, M_ON, "cot"),
    FUNC(K_SIN, M_ON | M_2ND, "acsc"), FUNC(K_COS, M_ON | M_2ND, "asec"), FUNC(K_TAN, M_ON | M_2ND, "acot"),

    DO(K_ON, M_2ND | M_ALPHA, A_SCREEN_OFF, 0),
};

static const key_label_t labels[] = {
    { K_0, "[0]" }, { K_1, "[1]" }, { K_2, "[2]" }, { K_3, "[3]" }, { K_4, "[4]" },
    { K_5, "[5]" }, { K_6, "[6]" }, { K_7, "[7]" }, { K_8, "[8]" }, { K_9, "[9]" },
    { K_DOT, "[.]" }, { K_NEG, "[(-)]" }, { K_ADD, "[+]" }, { K_SUB, "[-]" },
    { K_MUL, "[x]" }, { K_DIV, "[/]" }, { K_LPAREN, "[(]" }, { K_RPAREN, "[)]" },
    { K_COMMA, "[,]" }, { K_XT, "[x,t,theta,n]" }, { K_STO, "[sto->]" },
    { K_POW, "[^]" }, { K_SQUARE, "[x^2]" }, { K_RECIP, "[x^-1]" },
    { K_SIN, "[sin]" }, { K_COS, "[cos]" }, { K_TAN, "[tan]" }, { K_LN, "[ln]" }, { K_LOG, "[log]" },
    { K_MATH, "[math]" }, { K_APPS, "[apps]" }, { K_PRGM, "[prgm]" }, { K_YEQU, "[y=]" },
    { K_ENTER, "[enter]" },
};

const keypad_t keypad = {
    binds, sizeof binds / sizeof binds[0],
    labels, sizeof labels / sizeof labels[0],
    { { M_ON, "[on]" }, { M_2ND, "[2nd]" }, { M_ALPHA, "[alpha]" } },
    "[0]-[9]", "[alpha] + key",
    { M_ON, M_ALPHA },  /* [2nd] + a key with nothing up there does nothing */
    M_2ND | M_ALPHA, M_ALPHA,
    { M_2ND, M_ALPHA, M_ON },
    "[on]+[mode]",
};

#else
typedef int bind_ce_unused; /* nothing here on the nspire */
#endif
