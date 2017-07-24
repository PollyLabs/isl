#ifndef ISL_SCHEDULE_CONSTRAINTS_H
#define ISL_SCHEDULE_CONSTRAINTS_H

#include <isl/schedule.h>

/* isl_edge_last_sc is the last type that is represented
 * as an isl_union_map in isl_schedule_constraints.
 * isl_edge_last_table is the last type that has an edge table
 * in isl_sched_graph.
 */
enum isl_edge_type {
	isl_edge_validity = 0,
	isl_edge_first = isl_edge_validity,
	isl_edge_coincidence,
	isl_edge_condition,
	isl_edge_conditional_validity,
	isl_edge_proximity,
	isl_edge_last_sc = isl_edge_proximity,
	isl_edge_consecutivity,
	isl_edge_last_table = isl_edge_consecutivity,
	isl_edge_local
};

__isl_give isl_schedule_constraints *
isl_schedule_constraints_align_params(__isl_take isl_schedule_constraints *sc);

__isl_give isl_union_map *isl_schedule_constraints_get(
	__isl_keep isl_schedule_constraints *sc, enum isl_edge_type type);
__isl_give isl_schedule_constraints *isl_schedule_constraints_add(
	__isl_take isl_schedule_constraints *sc, enum isl_edge_type type,
	__isl_take isl_union_map *c);

int isl_schedule_constraints_n_basic_map(
	__isl_keep isl_schedule_constraints *sc);
int isl_schedule_constraints_n_inter_consecutivity_map(
	__isl_keep isl_schedule_constraints *sc);
int isl_schedule_constraints_n_map(__isl_keep isl_schedule_constraints *sc);

#endif
