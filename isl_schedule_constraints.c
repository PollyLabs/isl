/*
 * Copyright 2012      Ecole Normale Superieure
 * Copyright 2015-2016 Sven Verdoolaege
 *
 * Use of this software is governed by the MIT license
 *
 * Written by Sven Verdoolaege,
 * Ecole Normale Superieure, 45 rue d'Ulm, 75230 Paris, France
 */

#include <isl_schedule_constraints.h>
#include <isl/schedule.h>
#include <isl/space.h>
#include <isl/aff.h>
#include <isl/set.h>
#include <isl/map.h>
#include <isl/union_set.h>
#include <isl/union_map.h>
#include <isl/stream.h>

/* The constraints that need to be satisfied by a schedule on "domain".
 *
 * "context" specifies extra constraints on the parameters.
 *
 * "prefix" specifies an outer schedule within which the schedule
 * should be computed.  A zero-dimensional "prefix" means that
 * there is no such outer schedule.
 *
 * "validity" constraints map domain elements i to domain elements
 * that should be scheduled after i.  (Hard constraint)
 * "proximity" constraints map domain elements i to domains elements
 * that should be scheduled as early as possible after i (or before i).
 * (Soft constraint)
 *
 * "condition" and "conditional_validity" constraints map possibly "tagged"
 * domain elements i -> s to "tagged" domain elements j -> t.
 * The elements of the "conditional_validity" constraints, but without the
 * tags (i.e., the elements i -> j) are treated as validity constraints,
 * except that during the construction of a tilable band,
 * the elements of the "conditional_validity" constraints may be violated
 * provided that all adjacent elements of the "condition" constraints
 * are local within the band.
 * A dependence is local within a band if domain and range are mapped
 * to the same schedule point by the band.
 *
 * "intra" represents intra-statement consecutivity constraints.
 * Each element in this list maps domain elements to a product space,
 * where the two multi-affine expressions are linearly independent
 * of each other.  The scheduler will try to construct a schedule
 * with outer schedule rows that are linear combinations of
 * the outer expressions and inner schedule rows that are equal
 * to the inner expressions (up to linear combinations of outer
 * schedule rows).
 *
 * "inter" represents inter-statement consecutivity constraints.
 * Each element in this list is a product of a relation of
 * pairs of domain elements that should be made consecutive and
 * a pair of references to intra-statement consecutivity constraints.
 * The scheduler will try to schedule the pairs of domain elements
 * together as long as the outer parts of the intra-statement consecutivity
 * constraints have not been covered.  At the next level,
 * it will try to schedule them at a distance of one.
 */
struct isl_schedule_constraints {
	isl_union_set *domain;
	isl_set *context;

	isl_multi_union_pw_aff *prefix;

	isl_union_map *constraint[isl_edge_last_sc + 1];
	isl_multi_aff_list *intra;
	isl_map_list *inter;
};

__isl_give isl_schedule_constraints *isl_schedule_constraints_copy(
	__isl_keep isl_schedule_constraints *sc)
{
	isl_ctx *ctx;
	isl_schedule_constraints *sc_copy;
	enum isl_edge_type i;

	ctx = isl_union_set_get_ctx(sc->domain);
	sc_copy = isl_calloc_type(ctx, struct isl_schedule_constraints);
	if (!sc_copy)
		return NULL;

	sc_copy->domain = isl_union_set_copy(sc->domain);
	sc_copy->context = isl_set_copy(sc->context);
	sc_copy->intra = isl_multi_aff_list_copy(sc->intra);
	sc_copy->inter = isl_map_list_copy(sc->inter);
	sc_copy->prefix = isl_multi_union_pw_aff_copy(sc->prefix);
	if (!sc_copy->domain || !sc_copy->context || !sc_copy->intra ||
	    !sc_copy->inter || !sc_copy->prefix)
		return isl_schedule_constraints_free(sc_copy);

	for (i = isl_edge_first; i <= isl_edge_last_sc; ++i) {
		sc_copy->constraint[i] = isl_union_map_copy(sc->constraint[i]);
		if (!sc_copy->constraint[i])
			return isl_schedule_constraints_free(sc_copy);
	}

	return sc_copy;
}

/* Construct an empty (invalid) isl_schedule_constraints object.
 * The caller is responsible for setting the domain and initializing
 * all the other fields, e.g., by calling isl_schedule_constraints_init.
 */
static __isl_give isl_schedule_constraints *isl_schedule_constraints_alloc(
	isl_ctx *ctx)
{
	return isl_calloc_type(ctx, struct isl_schedule_constraints);
}

/* Initialize all the fields of "sc", except domain, which is assumed
 * to have been set by the caller.
 */
static __isl_give isl_schedule_constraints *isl_schedule_constraints_init(
	__isl_take isl_schedule_constraints *sc)
{
	isl_ctx *ctx;
	isl_space *space;
	isl_union_map *empty;
	enum isl_edge_type i;

	if (!sc)
		return NULL;
	if (!sc->domain)
		return isl_schedule_constraints_free(sc);
	space = isl_union_set_get_space(sc->domain);
	if (!sc->context)
		sc->context = isl_set_universe(isl_space_copy(space));
	if (!sc->prefix) {
		isl_space *space_prefix;
		space_prefix = isl_space_set_from_params(isl_space_copy(space));
		sc->prefix = isl_multi_union_pw_aff_zero(space_prefix);
	}
	empty = isl_union_map_empty(space);
	for (i = isl_edge_first; i <= isl_edge_last_sc; ++i) {
		if (sc->constraint[i])
			continue;
		sc->constraint[i] = isl_union_map_copy(empty);
		if (!sc->constraint[i])
			sc->domain = isl_union_set_free(sc->domain);
	}
	isl_union_map_free(empty);
	ctx = isl_schedule_constraints_get_ctx(sc);
	if (!sc->intra)
		sc->intra = isl_multi_aff_list_alloc(ctx, 0);
	if (!sc->inter)
		sc->inter = isl_map_list_alloc(ctx, 0);

	if (!sc->domain || !sc->context || !sc->intra || !sc->inter ||
	    !sc->prefix)
		return isl_schedule_constraints_free(sc);

	return sc;
}

/* Construct an isl_schedule_constraints object for computing a schedule
 * on "domain".  The initial object does not impose any constraints.
 */
__isl_give isl_schedule_constraints *isl_schedule_constraints_on_domain(
	__isl_take isl_union_set *domain)
{
	isl_ctx *ctx;
	isl_schedule_constraints *sc;

	if (!domain)
		return NULL;

	ctx = isl_union_set_get_ctx(domain);
	sc = isl_schedule_constraints_alloc(ctx);
	if (!sc)
		goto error;

	sc->domain = domain;
	return isl_schedule_constraints_init(sc);
error:
	isl_union_set_free(domain);
	return NULL;
}

/* Replace the domain of "sc" by "domain".
 */
static __isl_give isl_schedule_constraints *isl_schedule_constraints_set_domain(
	__isl_take isl_schedule_constraints *sc,
	__isl_take isl_union_set *domain)
{
	if (!sc || !domain)
		goto error;

	isl_union_set_free(sc->domain);
	sc->domain = domain;

	return sc;
error:
	isl_schedule_constraints_free(sc);
	isl_union_set_free(domain);
	return NULL;
}

/* Intersect the domain of "sc" with "domain".
 */
__isl_give isl_schedule_constraints *isl_schedule_constraints_intersect_domain(
	__isl_take isl_schedule_constraints *sc,
	__isl_take isl_union_set *domain)
{
	isl_union_set *sc_domain;

	sc_domain = isl_schedule_constraints_get_domain(sc);
	sc_domain = isl_union_set_intersect(sc_domain, domain);
	return isl_schedule_constraints_set_domain(sc, sc_domain);
}

/* Replace the context of "sc" by "context".
 */
__isl_give isl_schedule_constraints *isl_schedule_constraints_set_context(
	__isl_take isl_schedule_constraints *sc, __isl_take isl_set *context)
{
	if (!sc || !context)
		goto error;

	isl_set_free(sc->context);
	sc->context = context;

	return sc;
error:
	isl_schedule_constraints_free(sc);
	isl_set_free(context);
	return NULL;
}

/* Replace the constraints of type "type" in "sc" by "c".
 */
static __isl_give isl_schedule_constraints *isl_schedule_constraints_set(
	__isl_take isl_schedule_constraints *sc, enum isl_edge_type type,
	__isl_take isl_union_map *c)
{
	if (!sc || !c)
		goto error;

	isl_union_map_free(sc->constraint[type]);
	sc->constraint[type] = c;

	return sc;
error:
	isl_schedule_constraints_free(sc);
	isl_union_map_free(c);
	return NULL;
}

/* Replace the validity constraints of "sc" by "validity".
 */
__isl_give isl_schedule_constraints *isl_schedule_constraints_set_validity(
	__isl_take isl_schedule_constraints *sc,
	__isl_take isl_union_map *validity)
{
	return isl_schedule_constraints_set(sc, isl_edge_validity, validity);
}

/* Replace the coincidence constraints of "sc" by "coincidence".
 */
__isl_give isl_schedule_constraints *isl_schedule_constraints_set_coincidence(
	__isl_take isl_schedule_constraints *sc,
	__isl_take isl_union_map *coincidence)
{
	return isl_schedule_constraints_set(sc, isl_edge_coincidence,
						coincidence);
}

/* Replace the proximity constraints of "sc" by "proximity".
 */
__isl_give isl_schedule_constraints *isl_schedule_constraints_set_proximity(
	__isl_take isl_schedule_constraints *sc,
	__isl_take isl_union_map *proximity)
{
	return isl_schedule_constraints_set(sc, isl_edge_proximity, proximity);
}

/* Replace the conditional validity constraints of "sc" by "condition"
 * and "validity".
 */
__isl_give isl_schedule_constraints *
isl_schedule_constraints_set_conditional_validity(
	__isl_take isl_schedule_constraints *sc,
	__isl_take isl_union_map *condition,
	__isl_take isl_union_map *validity)
{
	sc = isl_schedule_constraints_set(sc, isl_edge_condition, condition);
	sc = isl_schedule_constraints_set(sc, isl_edge_conditional_validity,
						validity);
	return sc;
}

/* Replace the intra-statement consecutivity constraints of "sc" by "intra".
 */
__isl_give isl_schedule_constraints *
isl_schedule_constraints_set_intra_consecutivity(
	__isl_take isl_schedule_constraints *sc,
	__isl_take isl_multi_aff_list *intra)
{
	if (!sc || !intra)
		goto error;

	isl_multi_aff_list_free(sc->intra);
	sc->intra = intra;

	return sc;
error:
	isl_schedule_constraints_free(sc);
	isl_multi_aff_list_free(intra);
	return NULL;
}

/* Replace the inter-statement consecutivity constraints of "sc" by "inter".
 */
__isl_give isl_schedule_constraints *
isl_schedule_constraints_set_inter_consecutivity(
	__isl_take isl_schedule_constraints *sc, __isl_take isl_map_list *inter)
{
	if (!sc || !inter)
		goto error;

	isl_map_list_free(sc->inter);
	sc->inter = inter;

	return sc;
error:
	isl_schedule_constraints_free(sc);
	isl_map_list_free(inter);
	return NULL;
}

/* Replace the schedule prefix of "sc" by "prefix".
 */
__isl_give isl_schedule_constraints *isl_schedule_constraints_set_prefix(
	__isl_take isl_schedule_constraints *sc,
	__isl_take isl_multi_union_pw_aff *prefix)
{
	if (!sc || !prefix)
		goto error;

	isl_multi_union_pw_aff_free(sc->prefix);
	sc->prefix = prefix;

	return sc;
error:
	isl_schedule_constraints_free(sc);
	isl_multi_union_pw_aff_free(prefix);
	return NULL;
}

__isl_null isl_schedule_constraints *isl_schedule_constraints_free(
	__isl_take isl_schedule_constraints *sc)
{
	enum isl_edge_type i;

	if (!sc)
		return NULL;

	isl_union_set_free(sc->domain);
	isl_set_free(sc->context);
	isl_multi_aff_list_free(sc->intra);
	isl_map_list_free(sc->inter);
	isl_multi_union_pw_aff_free(sc->prefix);
	for (i = isl_edge_first; i <= isl_edge_last_sc; ++i)
		isl_union_map_free(sc->constraint[i]);

	free(sc);

	return NULL;
}

isl_ctx *isl_schedule_constraints_get_ctx(
	__isl_keep isl_schedule_constraints *sc)
{
	return sc ? isl_union_set_get_ctx(sc->domain) : NULL;
}

/* Return the domain of "sc".
 */
__isl_give isl_union_set *isl_schedule_constraints_get_domain(
	__isl_keep isl_schedule_constraints *sc)
{
	if (!sc)
		return NULL;

	return isl_union_set_copy(sc->domain);
}

/* Return the context of "sc".
 */
__isl_give isl_set *isl_schedule_constraints_get_context(
	__isl_keep isl_schedule_constraints *sc)
{
	if (!sc)
		return NULL;

	return isl_set_copy(sc->context);
}

/* Return the constraints of type "type" in "sc".
 */
__isl_give isl_union_map *isl_schedule_constraints_get(
	__isl_keep isl_schedule_constraints *sc, enum isl_edge_type type)
{
	if (!sc)
		return NULL;

	return isl_union_map_copy(sc->constraint[type]);
}

/* Return the validity constraints of "sc".
 */
__isl_give isl_union_map *isl_schedule_constraints_get_validity(
	__isl_keep isl_schedule_constraints *sc)
{
	return isl_schedule_constraints_get(sc, isl_edge_validity);
}

/* Return the coincidence constraints of "sc".
 */
__isl_give isl_union_map *isl_schedule_constraints_get_coincidence(
	__isl_keep isl_schedule_constraints *sc)
{
	return isl_schedule_constraints_get(sc, isl_edge_coincidence);
}

/* Return the proximity constraints of "sc".
 */
__isl_give isl_union_map *isl_schedule_constraints_get_proximity(
	__isl_keep isl_schedule_constraints *sc)
{
	return isl_schedule_constraints_get(sc, isl_edge_proximity);
}

/* Return the conditional validity constraints of "sc".
 */
__isl_give isl_union_map *isl_schedule_constraints_get_conditional_validity(
	__isl_keep isl_schedule_constraints *sc)
{
	return isl_schedule_constraints_get(sc, isl_edge_conditional_validity);
}

/* Return the conditions for the conditional validity constraints of "sc".
 */
__isl_give isl_union_map *
isl_schedule_constraints_get_conditional_validity_condition(
	__isl_keep isl_schedule_constraints *sc)
{
	return isl_schedule_constraints_get(sc, isl_edge_condition);
}

/* Return the intra-statement consecutivity constraints of "sc".
 */
__isl_give isl_multi_aff_list *isl_schedule_constraints_get_intra_consecutivity(
	__isl_keep isl_schedule_constraints *sc)
{
	if (!sc)
		return NULL;

	return isl_multi_aff_list_copy(sc->intra);
}

/* Return the inter-statement consecutivity constraints of "sc".
 */
__isl_give isl_map_list *isl_schedule_constraints_get_inter_consecutivity(
	__isl_keep isl_schedule_constraints *sc)
{
	if (!sc)
		return NULL;

	return isl_map_list_copy(sc->inter);
}

/* Return the schedule prefix of "sc".
 */
__isl_give isl_multi_union_pw_aff *isl_schedule_constraints_get_prefix(
	__isl_keep isl_schedule_constraints *sc)
{
	if (!sc)
		return NULL;

	return isl_multi_union_pw_aff_copy(sc->prefix);
}

/* Add "c" to the constraints of type "type" in "sc".
 */
__isl_give isl_schedule_constraints *isl_schedule_constraints_add(
	__isl_take isl_schedule_constraints *sc, enum isl_edge_type type,
	__isl_take isl_union_map *c)
{
	if (!sc || !c)
		goto error;

	c = isl_union_map_union(sc->constraint[type], c);
	sc->constraint[type] = c;
	if (!c)
		return isl_schedule_constraints_free(sc);

	return sc;
error:
	isl_schedule_constraints_free(sc);
	isl_union_map_free(c);
	return NULL;
}

/* Can a schedule constraint of type "type" be tagged?
 */
static int may_be_tagged(enum isl_edge_type type)
{
	if (type == isl_edge_condition || type == isl_edge_conditional_validity)
		return 1;
	return 0;
}

/* Apply "umap" to the domains of the wrapped relations
 * inside the domain and range of "c".
 *
 * That is, for each map of the form
 *
 *	[D -> S] -> [E -> T]
 *
 * in "c", apply "umap" to D and E.
 *
 * D is exposed by currying the relation to
 *
 *	D -> [S -> [E -> T]]
 *
 * E is exposed by doing the same to the inverse of "c".
 */
static __isl_give isl_union_map *apply_factor_domain(
	__isl_take isl_union_map *c, __isl_keep isl_union_map *umap)
{
	c = isl_union_map_curry(c);
	c = isl_union_map_apply_domain(c, isl_union_map_copy(umap));
	c = isl_union_map_uncurry(c);

	c = isl_union_map_reverse(c);
	c = isl_union_map_curry(c);
	c = isl_union_map_apply_domain(c, isl_union_map_copy(umap));
	c = isl_union_map_uncurry(c);
	c = isl_union_map_reverse(c);

	return c;
}

/* Apply "umap" to domain and range of "c".
 * If "tag" is set, then "c" may contain tags and then "umap"
 * needs to be applied to the domains of the wrapped relations
 * inside the domain and range of "c".
 */
static __isl_give isl_union_map *apply(__isl_take isl_union_map *c,
	__isl_keep isl_union_map *umap, int tag)
{
	isl_union_map *t;

	if (tag)
		t = isl_union_map_copy(c);
	c = isl_union_map_apply_domain(c, isl_union_map_copy(umap));
	c = isl_union_map_apply_range(c, isl_union_map_copy(umap));
	if (!tag)
		return c;
	t = apply_factor_domain(t, umap);
	c = isl_union_map_union(c, t);
	return c;
}

/* Apply "umap" to the domain of the schedule constraints "sc".
 *
 * The two sides of the various schedule constraints are adjusted
 * accordingly.
 *
 * Intra-statement consecutivity constraints and the schedule prefix
 * are removed because they cannot be transformed by "umap".
 * Inter-statement consecutivity constraints are removed
 * because the referenced intra-statement consecutivity constraints
 * are removed.
 */
__isl_give isl_schedule_constraints *isl_schedule_constraints_apply(
	__isl_take isl_schedule_constraints *sc,
	__isl_take isl_union_map *umap)
{
	int n;
	enum isl_edge_type i;

	if (!sc || !umap)
		goto error;

	for (i = isl_edge_first; i <= isl_edge_last_sc; ++i) {
		int tag = may_be_tagged(i);

		sc->constraint[i] = apply(sc->constraint[i], umap, tag);
		if (!sc->constraint[i])
			goto error;
	}
	sc->domain = isl_union_set_apply(sc->domain, umap);
	sc->intra = isl_multi_aff_list_clear(sc->intra);
	sc->inter = isl_map_list_clear(sc->inter);
	n = isl_multi_union_pw_aff_dim(sc->prefix, isl_dim_set);
	sc->prefix = isl_multi_union_pw_aff_drop_dims(sc->prefix,
							isl_dim_set, 0, n);
	if (!sc->domain || !sc->intra || !sc->inter || !sc->prefix)
		return isl_schedule_constraints_free(sc);

	return sc;
error:
	isl_schedule_constraints_free(sc);
	isl_union_map_free(umap);
	return NULL;
}

/* An enumeration of the various keys that may appear in a YAML mapping
 * of an isl_schedule_constraints object.
 * The keys for the edge types are assumed to have the same values
 * as the edge types in isl_edge_type.
 */
enum isl_sc_key {
	isl_sc_key_error = -1,
	isl_sc_key_validity = isl_edge_validity,
	isl_sc_key_coincidence = isl_edge_coincidence,
	isl_sc_key_condition = isl_edge_condition,
	isl_sc_key_conditional_validity = isl_edge_conditional_validity,
	isl_sc_key_proximity = isl_edge_proximity,
	isl_sc_key_domain,
	isl_sc_key_context,
	isl_sc_key_intra,
	isl_sc_key_inter,
	isl_sc_key_prefix,
	isl_sc_key_end
};

/* Textual representations of the YAML keys for an isl_schedule_constraints
 * object.
 */
static char *key_str[] = {
	[isl_sc_key_validity] = "validity",
	[isl_sc_key_coincidence] = "coincidence",
	[isl_sc_key_condition] = "condition",
	[isl_sc_key_conditional_validity] = "conditional_validity",
	[isl_sc_key_proximity] = "proximity",
	[isl_sc_key_domain] = "domain",
	[isl_sc_key_context] = "context",
	[isl_sc_key_intra] = "intra_consecutivity",
	[isl_sc_key_inter] = "inter_consecutivity",
	[isl_sc_key_prefix] = "prefix",
};

/* Print a key, value pair for the edge of type "type" in "sc" to "p".
 *
 * If the edge relation is empty, then it is not printed since
 * an empty relation is the default value.
 */
static __isl_give isl_printer *print_constraint(__isl_take isl_printer *p,
	__isl_keep isl_schedule_constraints *sc, enum isl_edge_type type)
{
	isl_bool empty;

	empty = isl_union_map_plain_is_empty(sc->constraint[type]);
	if (empty < 0)
		return isl_printer_free(p);
	if (empty)
		return p;

	p = isl_printer_print_str(p, key_str[type]);
	p = isl_printer_yaml_next(p);
	p = isl_printer_print_union_map(p, sc->constraint[type]);
	p = isl_printer_yaml_next(p);

	return p;
}

/* Print a key, value pair for the intra-statement consecutivity constraints.
 *
 * If the list of intra-statement consecutivity constraints is empty, then
 * the list is not printed since an empty list is the default value.
 */
static __isl_give isl_printer *print_intra(__isl_take isl_printer *p,
	__isl_keep isl_schedule_constraints *sc)
{
	if (isl_multi_aff_list_n_multi_aff(sc->intra) == 0)
		return p;

	p = isl_printer_print_str(p, key_str[isl_sc_key_intra]);
	p = isl_printer_yaml_next(p);
	p = isl_printer_print_multi_aff_list(p, sc->intra);
	p = isl_printer_yaml_next(p);

	return p;
}

/* Print a key, value pair for the inter-statement consecutivity constraints.
 *
 * If the list of inter-statement consecutivity constraints is empty, then
 * the list is not printed since an empty list is the default value.
 */
static __isl_give isl_printer *print_inter(__isl_take isl_printer *p,
	__isl_keep isl_schedule_constraints *sc)
{
	if (isl_map_list_n_map(sc->inter) == 0)
		return p;

	p = isl_printer_print_str(p, key_str[isl_sc_key_inter]);
	p = isl_printer_yaml_next(p);
	p = isl_printer_print_map_list(p, sc->inter);
	p = isl_printer_yaml_next(p);

	return p;
}

/* Print a key, value pair for the schedule prefix.
 *
 * If the schedule prefix is zero-dimensional, then
 * it is not printed since a zero-dimensional prefix is the default.
 */
static __isl_give isl_printer *print_prefix(__isl_take isl_printer *p,
	__isl_keep isl_schedule_constraints *sc)
{
	if (isl_multi_union_pw_aff_dim(sc->prefix, isl_dim_set) == 0)
		return p;

	p = isl_printer_print_str(p, key_str[isl_sc_key_prefix]);
	p = isl_printer_yaml_next(p);
	p = isl_printer_print_multi_union_pw_aff(p, sc->prefix);
	p = isl_printer_yaml_next(p);

	return p;
}

/* Print "sc" to "p"
 *
 * In particular, print the isl_schedule_constraints object as a YAML document.
 * Fields with values that are (obviously) equal to their default values
 * are not printed.
 */
__isl_give isl_printer *isl_printer_print_schedule_constraints(
	__isl_take isl_printer *p, __isl_keep isl_schedule_constraints *sc)
{
	isl_bool universe;

	if (!sc)
		return isl_printer_free(p);

	p = isl_printer_yaml_start_mapping(p);
	p = isl_printer_print_str(p, key_str[isl_sc_key_domain]);
	p = isl_printer_yaml_next(p);
	p = isl_printer_print_union_set(p, sc->domain);
	p = isl_printer_yaml_next(p);
	universe = isl_set_plain_is_universe(sc->context);
	if (universe < 0)
		return isl_printer_free(p);
	if (!universe) {
		p = isl_printer_print_str(p, key_str[isl_sc_key_context]);
		p = isl_printer_yaml_next(p);
		p = isl_printer_print_set(p, sc->context);
		p = isl_printer_yaml_next(p);
	}
	p = print_constraint(p, sc, isl_edge_validity);
	p = print_constraint(p, sc, isl_edge_proximity);
	p = print_constraint(p, sc, isl_edge_coincidence);
	p = print_constraint(p, sc, isl_edge_condition);
	p = print_constraint(p, sc, isl_edge_conditional_validity);
	p = print_intra(p, sc);
	p = print_inter(p, sc);
	p = print_prefix(p, sc);
	p = isl_printer_yaml_end_mapping(p);

	return p;
}

#undef BASE
#define BASE schedule_constraints
#include <print_templ_yaml.c>

#undef KEY
#define KEY enum isl_sc_key
#undef KEY_ERROR
#define KEY_ERROR isl_sc_key_error
#undef KEY_END
#define KEY_END isl_sc_key_end
#include "extract_key.c"

#undef BASE
#define BASE set
#include "read_in_string_templ.c"

#undef BASE
#define BASE union_set
#include "read_in_string_templ.c"

#undef BASE
#define BASE union_map
#include "read_in_string_templ.c"

#undef BASE
#define BASE multi_aff_list
#include "read_in_string_templ.c"

#undef BASE
#define BASE map_list
#include "read_in_string_templ.c"

#undef BASE
#define BASE multi_union_pw_aff
#include "read_in_string_templ.c"

/* Read an isl_schedule_constraints object from "s".
 *
 * Start off with an empty (invalid) isl_schedule_constraints object and
 * then fill up the fields based on the input.
 * The input needs to contain at least a description of the domain.
 * The other fields are set to defaults by isl_schedule_constraints_init
 * if they are not specified in the input.
 */
__isl_give isl_schedule_constraints *isl_stream_read_schedule_constraints(
	isl_stream *s)
{
	isl_ctx *ctx;
	isl_schedule_constraints *sc;
	int more;
	int domain_set = 0;

	if (isl_stream_yaml_read_start_mapping(s))
		return NULL;

	ctx = isl_stream_get_ctx(s);
	sc = isl_schedule_constraints_alloc(ctx);
	while ((more = isl_stream_yaml_next(s)) > 0) {
		enum isl_sc_key key;
		isl_set *context;
		isl_union_set *domain;
		isl_union_map *constraints;
		isl_multi_aff_list *intra;
		isl_map_list *inter;
		isl_multi_union_pw_aff *prefix;

		key = get_key(s);
		if (isl_stream_yaml_next(s) < 0)
			return isl_schedule_constraints_free(sc);
		switch (key) {
		case isl_sc_key_end:
		case isl_sc_key_error:
			return isl_schedule_constraints_free(sc);
		case isl_sc_key_domain:
			domain_set = 1;
			domain = read_union_set(s);
			sc = isl_schedule_constraints_set_domain(sc, domain);
			if (!sc)
				return NULL;
			break;
		case isl_sc_key_context:
			context = read_set(s);
			sc = isl_schedule_constraints_set_context(sc, context);
			if (!sc)
				return NULL;
			break;
		case isl_sc_key_intra:
			intra = read_multi_aff_list(s);
			sc = isl_schedule_constraints_set_intra_consecutivity(
								    sc, intra);
			if (!sc)
				return NULL;
			break;
		case isl_sc_key_inter:
			inter = read_map_list(s);
			sc = isl_schedule_constraints_set_inter_consecutivity(
								    sc, inter);
			if (!sc)
				return NULL;
			break;
		case isl_sc_key_prefix:
			prefix = read_multi_union_pw_aff(s);
			sc = isl_schedule_constraints_set_prefix(sc, prefix);
			if (!sc)
				return NULL;
			break;
		case isl_sc_key_validity:
		case isl_sc_key_coincidence:
		case isl_sc_key_condition:
		case isl_sc_key_conditional_validity:
		case isl_sc_key_proximity:
			constraints = read_union_map(s);
			sc = isl_schedule_constraints_set(sc, key, constraints);
			if (!sc)
				return NULL;
			break;
		}
	}
	if (more < 0)
		return isl_schedule_constraints_free(sc);

	if (isl_stream_yaml_read_end_mapping(s) < 0) {
		isl_stream_error(s, NULL, "unexpected extra elements");
		return isl_schedule_constraints_free(sc);
	}

	if (!domain_set) {
		isl_stream_error(s, NULL, "no domain specified");
		return isl_schedule_constraints_free(sc);
	}

	return isl_schedule_constraints_init(sc);
}

/* Read an isl_schedule_constraints object from the file "input".
 */
__isl_give isl_schedule_constraints *isl_schedule_constraints_read_from_file(
	isl_ctx *ctx, FILE *input)
{
	struct isl_stream *s;
	isl_schedule_constraints *sc;

	s = isl_stream_new_file(ctx, input);
	if (!s)
		return NULL;
	sc = isl_stream_read_schedule_constraints(s);
	isl_stream_free(s);

	return sc;
}

/* Read an isl_schedule_constraints object from the string "str".
 */
__isl_give isl_schedule_constraints *isl_schedule_constraints_read_from_str(
	isl_ctx *ctx, const char *str)
{
	struct isl_stream *s;
	isl_schedule_constraints *sc;

	s = isl_stream_new_str(ctx, str);
	if (!s)
		return NULL;
	sc = isl_stream_read_schedule_constraints(s);
	isl_stream_free(s);

	return sc;
}

/* Align the initial parameters of "space" to those of "map".
 */
static isl_stat space_align_params(__isl_take isl_map *map, void *user)
{
	isl_space **space = user;

	*space = isl_space_align_params(*space, isl_map_get_space(map));
	isl_map_free(map);

	if (!*space)
		return isl_stat_error;
	return isl_stat_ok;
}

/* Align the initial parameters of "map" to those of "space".
 */
static __isl_give isl_map *align_params(__isl_take isl_map *map, void *user)
{
	isl_space *space = user;

	return isl_map_align_params(map, isl_space_copy(space));
}

/* Align the parameters of the fields of "sc".
 * The intra-statement consecutivity constraints do not need to have
 * their parameters aligned because only the coefficients
 * of the statement instance identifiers are taken into account.
 */
__isl_give isl_schedule_constraints *
isl_schedule_constraints_align_params(__isl_take isl_schedule_constraints *sc)
{
	isl_space *space;
	enum isl_edge_type i;

	if (!sc)
		return NULL;

	space = isl_union_set_get_space(sc->domain);
	space = isl_space_align_params(space, isl_set_get_space(sc->context));
	for (i = isl_edge_first; i <= isl_edge_last_sc; ++i)
		space = isl_space_align_params(space,
				    isl_union_map_get_space(sc->constraint[i]));
	if (isl_map_list_foreach(sc->inter, &space_align_params, &space) < 0)
		space = isl_space_free(space);
	space = isl_space_align_params(space,
				isl_multi_union_pw_aff_get_space(sc->prefix));

	for (i = isl_edge_first; i <= isl_edge_last_sc; ++i) {
		sc->constraint[i] = isl_union_map_align_params(
				    sc->constraint[i], isl_space_copy(space));
		if (!sc->constraint[i])
			space = isl_space_free(space);
	}
	sc->inter = isl_map_list_map(sc->inter, &align_params, space);
	sc->prefix = isl_multi_union_pw_aff_align_params(sc->prefix,
							isl_space_copy(space));
	sc->context = isl_set_align_params(sc->context, isl_space_copy(space));
	sc->domain = isl_union_set_align_params(sc->domain, space);
	if (!sc->context || !sc->domain || !sc->inter || !sc->prefix)
		return isl_schedule_constraints_free(sc);

	return sc;
}

/* Add the number of basic maps in "map" to *n.
 */
static isl_stat add_n_basic_map(__isl_take isl_map *map, void *user)
{
	int *n = user;

	*n += isl_map_n_basic_map(map);
	isl_map_free(map);

	return isl_stat_ok;
}

/* Return the total number of isl_basic_maps in the constraints of "sc".
 * Return -1 on error.
 */
int isl_schedule_constraints_n_basic_map(
	__isl_keep isl_schedule_constraints *sc)
{
	enum isl_edge_type i;
	int n = 0;

	if (!sc)
		return -1;
	for (i = isl_edge_first; i <= isl_edge_last_sc; ++i)
		if (isl_union_map_foreach_map(sc->constraint[i],
						&add_n_basic_map, &n) < 0)
			return -1;

	return n;
}

/* Return the number of inter-statement consecutivity constraints.
 * Return -1 on error.
 */
int isl_schedule_constraints_n_inter_consecutivity_map(
	__isl_keep isl_schedule_constraints *sc)
{
	if (!sc)
		return -1;
	return isl_map_list_n_map(sc->inter);
}

/* Return the total number of isl_maps in the constraints of "sc".
 */
int isl_schedule_constraints_n_map(__isl_keep isl_schedule_constraints *sc)
{
	enum isl_edge_type i;
	int n = 0;

	for (i = isl_edge_first; i <= isl_edge_last_sc; ++i)
		n += isl_union_map_n_map(sc->constraint[i]);
	n += isl_schedule_constraints_n_inter_consecutivity_map(sc);

	return n;
}
