#ifndef FLUXION_CLIPBOARD_H
#define FLUXION_CLIPBOARD_H

/* the system clipboard as text. only the PC window has one, elsewhere these do nothing */

/* NULL does nothing */
void clipboard_put(const char *text);

/* a copy of what's on it (caller frees), NULL when no text or no clipboard */
char *clipboard_get(void);

#endif
