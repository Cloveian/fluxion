#ifndef DERIVATIVE_H_
#define DERIVATIVE_H_

#include "identities.h"

#define ID_NUM_DERIV 25 /*Fluxion: 21 upstream, +abs, acot, asec, acsc*/
extern pcas_id_t id_derivative[ID_NUM_DERIV];

extern pcas_id_t id_deriv_power_rule;
extern pcas_id_t id_deriv_constant_rule;
extern pcas_id_t id_deriv_product_rule;

/*Takes the derivative of all derivative nodes*/
bool eval_derivative_nodes(pcas_ast_t *e);
/*Replaces the node with a deriv() node and calls eval_derivative nodes*/
void derivative(pcas_ast_t *e, pcas_ast_t *respect_to, pcas_ast_t *eval_at);
/*Fluxion: d/dx of e simplified, new. NULL once cancelled or out of memory*/
pcas_ast_t *slope_of(pcas_ast_t *e, pcas_ast_t *var);

#endif