#ifndef IDENTITIES_H_
#define IDENTITIES_H_

#include "../parser.h"

/*Neither the from or the to text can exceed 50 characters*/
#define ID_MAX_TEXT 50

typedef struct {
    /*Fluxion: pointers (were char[ID_MAX_TEXT]), so the text stays read-only. as
    arrays the tables were 16 KB of .data, and a Flash App with that much .data
    crashes on launch before main even runs. took way too long to find, like 3 hours gone*/
    const char *from_text, *to_text;
    pcas_ast_t *from, *to;
    /*Fluxion: functions (sin, ln, ...) that appear in from, see func_mask().
    read from from_text before loading, so unneeded identities never get parsed.*/
    unsigned long funcs;
    bool funcs_known;
} pcas_id_t;

#define ID_NUM_GENERAL 17
#define ID_NUM_TRIG_IDENTITIES 29
#define ID_NUM_TRIG_INV_CONSTANTS 25
#define ID_NUM_TRIG_CONSTANTS 47
#define ID_NUM_HYPERBOLIC 3
#define ID_NUM_COMPLEX 12

extern pcas_id_t id_general[ID_NUM_GENERAL];
extern pcas_id_t id_trig_identities[ID_NUM_TRIG_IDENTITIES];
extern pcas_id_t id_trig_inv_constants[ID_NUM_TRIG_INV_CONSTANTS];
extern pcas_id_t id_trig_constants[ID_NUM_TRIG_CONSTANTS];
extern pcas_id_t id_hyperbolic[ID_NUM_HYPERBOLIC];
extern pcas_id_t id_complex[ID_NUM_COMPLEX];

/*Fluxion: bit per function operator in e*/
unsigned long func_mask(pcas_ast_t *e);
/*Fluxion: same bits, read from identity text (str_table syntax) without parsing*/
unsigned long text_func_mask(const char *text);

bool id_Load(pcas_id_t *id);
void id_Unload(pcas_id_t *id);
bool id_Execute(pcas_ast_t *e, pcas_id_t *id, bool recursive);

bool id_ExecuteTable(pcas_ast_t *e, pcas_id_t *table, unsigned table_len, bool recursive);
void id_UnloadTable(pcas_id_t *table, unsigned table_len);
/*Calls unload table for all tables*/
void id_UnloadAll();

#endif
