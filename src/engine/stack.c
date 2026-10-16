#include "stack.h"
#include "error.h"

#include <stdlib.h>

void stack_Create(pcas_stack_t *s) {
    s->top = 0;
    s->_max = STACK_START;
    s->items = pcas_malloc(STACK_START * sizeof(void*));
}

void stack_Cleanup(pcas_stack_t *s) {
    free(s->items);
}

void stack_Push(pcas_stack_t *s, void *item) {
    if (s->top >= s->_max) {
        /*Fluxion: through a temp, so a failed grow keeps the old buffer instead
        of leaking it. the item gets dropped either way, oom unwinds anyway*/
        void **grown = pcas_realloc(s->items, s->_max * 2 * sizeof(void*));
        if(grown == NULL)
            return;
        s->items = grown;
        s->_max *= 2;
    }

    if(s->items == NULL) /*Fluxion: stack_Create ran out*/
        return;

    s->items[s->top++] = item;
}

void *stack_Pop(pcas_stack_t *s) {
    if (s->top <= 0) {
        s->top = 0;
        return NULL;
    }

    return s->items[--s->top];
}

void *stack_Peek(pcas_stack_t *s) {
    if (s->top <= 0) {
        s->top = 0;
        return NULL;
    }

    return s->items[s->top - 1];
}

void stack_Clear(pcas_stack_t *s) {
    s->top = 0;
}