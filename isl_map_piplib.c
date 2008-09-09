#include "isl_set.h"
#include "isl_map.h"
#include "isl_piplib.h"
#include "isl_map_piplib.h"
#include "isl_map_private.h"

static void copy_values_from(isl_int *dst, Entier *src, unsigned n)
{
	int i;

	for (i = 0; i < n; ++i)
		entier_assign(dst[i], src[i]);
}

static void add_value(isl_int *dst, Entier *src)
{
	mpz_add(*dst, *dst, *src);
}

static void copy_constraint_from(isl_int *dst, PipVector *src,
		unsigned nparam, unsigned n_in, unsigned n_out,
		unsigned extra, int *pos)
{
	int i;

	copy_values_from(dst, src->the_vector+src->nb_elements-1, 1);
	copy_values_from(dst+1, src->the_vector, nparam+n_in);
	isl_seq_clr(dst+1+nparam+n_in, n_out);
	isl_seq_clr(dst+1+nparam+n_in+n_out, extra);
	for (i = 0; i + n_in + nparam < src->nb_elements-1; ++i) {
		int p = pos[i];
		add_value(&dst[1+nparam+n_in+n_out+p],
			  &src->the_vector[n_in+nparam+i]);
	}
}

static int add_inequality(struct isl_ctx *ctx,
		   struct isl_basic_map *bmap, int *pos, PipVector *vec)
{
	int i = isl_basic_map_alloc_inequality(bmap);
	if (i < 0)
		return -1;
	copy_constraint_from(bmap->ineq[i], vec,
	    bmap->nparam, bmap->n_in, bmap->n_out, bmap->extra, pos);

	return i;
}

/* For a div d = floor(f/m), add the constraints
 *
 *		f - m d >= 0
 *		-(f-(n-1)) + m d >= 0
 *
 * Note that the second constraint is the negation of
 *
 *		f - m d >= n
 */
static int add_div_constraints(struct isl_ctx *ctx,
	struct isl_basic_map *bmap, int *pos, PipNewparm *p, unsigned div)
{
	int i, j;
	unsigned div_pos = 1 + bmap->nparam + bmap->n_in + bmap->n_out + div;
	unsigned total = bmap->nparam + bmap->n_in + bmap->n_out + bmap->extra;

	i = add_inequality(ctx, bmap, pos, p->vector);
	if (i < 0)
		return -1;
	copy_values_from(&bmap->ineq[i][div_pos], &p->deno, 1);
	isl_int_neg(bmap->ineq[i][div_pos], bmap->ineq[i][div_pos]);

	j = isl_basic_map_alloc_inequality(bmap);
	if (j < 0)
		return -1;
	isl_seq_neg(bmap->ineq[j], bmap->ineq[i], 1 + total);
	isl_int_add(bmap->ineq[j][0], bmap->ineq[j][0], bmap->ineq[j][div_pos]);
	isl_int_sub_ui(bmap->ineq[j][0], bmap->ineq[j][0], 1);
	return j;
}

static int add_equality(struct isl_ctx *ctx,
		   struct isl_basic_map *bmap, int *pos,
		   unsigned var, PipVector *vec)
{
	int i;

	isl_assert(ctx, var < bmap->n_out, return -1);

	i = isl_basic_map_alloc_equality(bmap);
	if (i < 0)
		return -1;
	copy_constraint_from(bmap->eq[i], vec,
	    bmap->nparam, bmap->n_in, bmap->n_out, bmap->extra, pos);
	isl_int_set_si(bmap->eq[i][1+bmap->nparam+bmap->n_in+var], -1);

	return i;
}

static int find_div(struct isl_ctx *ctx,
		   struct isl_basic_map *bmap, int *pos, PipNewparm *p)
{
	int i, j;

	i = isl_basic_map_alloc_div(bmap);
	if (i < 0)
		return -1;

	copy_constraint_from(bmap->div[i]+1, p->vector,
	    bmap->nparam, bmap->n_in, bmap->n_out, bmap->extra, pos);

	copy_values_from(bmap->div[i], &p->deno, 1);
	for (j = 0; j < i; ++j)
		if (isl_seq_eq(bmap->div[i], bmap->div[j],
				1+1+bmap->nparam+bmap->n_in+bmap->n_out+j)) {
			isl_basic_map_free_div(bmap, 1);
			return j;
		}

	if (add_div_constraints(ctx, bmap, pos, p, i) < 0)
		return -1;

	return i;
}

/* Count some properties of a quast
 * - maximal number of new parameters
 * - maximal depth
 * - total number of solutions
 * - total number of empty branches
 */
static void quast_count(PipQuast *q, int *maxnew, int depth, int *maxdepth,
		        int *sol, int *nosol)
{
	PipNewparm *p;

	for (p = q->newparm; p; p = p->next)
		if (p->rank > *maxnew)
			*maxnew = p->rank;
	if (q->condition) {
		if (++depth > *maxdepth)
			*maxdepth = depth;
		quast_count(q->next_else, maxnew, depth, maxdepth, sol, nosol);
		quast_count(q->next_then, maxnew, depth, maxdepth, sol, nosol);
	} else {
		if (q->list)
			++(*sol);
		else
			++(*nosol);
	}
}

/*
 * pos: array of length bmap->set.extra, mapping each of the existential
 *		variables PIP proposes to an existential variable in bmap
 * bmap: collects the currently active constraints
 * rest: collects the empty leaves of the quast (if not NULL)
 */
struct scan_data {
	struct isl_ctx 			*ctx;
	struct isl_basic_map 		*bmap;
	struct isl_set			**rest;
	int	   *pos;
};

/*
 * New existentially quantified variables are places after the existing ones.
 */
static struct isl_map *scan_quast_r(struct scan_data *data, PipQuast *q,
				    struct isl_map *map)
{
	PipNewparm *p;
	int error;
	struct isl_basic_map *bmap = data->bmap;
	unsigned old_n_div = bmap->n_div;

	if (!map)
		goto error;

	for (p = q->newparm; p; p = p->next) {
		int pos;
		unsigned pip_param = bmap->nparam + bmap->n_in;

		pos = find_div(data->ctx, bmap, data->pos, p);
		if (pos < 0)
			goto error;
		data->pos[p->rank - pip_param] = pos;
	}

	if (q->condition) {
		int j;
		int pos = add_inequality(data->ctx, bmap, data->pos,
					 q->condition);
		if (pos < 0)
			goto error;
		map = scan_quast_r(data, q->next_then, map);

		if (isl_inequality_negate(bmap, pos))
			goto error;
		map = scan_quast_r(data, q->next_else, map);

		if (isl_basic_map_free_inequality(bmap, 1))
			goto error;
	} else if (q->list) {
		PipList *l;
		int j;
		/* if bmap->n_out is zero, we are only interested in the domains
		 * where a solution exists and not in the actual solution
		 */
		for (j = 0, l = q->list; j < bmap->n_out && l; ++j, l = l->next)
			if (add_equality(data->ctx, bmap, data->pos, j,
						l->vector) < 0)
				goto error;
		map = isl_map_add(map, isl_basic_map_copy(bmap));
		if (isl_basic_map_free_equality(bmap, bmap->n_out))
			goto error;
	} else if (map->n && data->rest) {
		/* not interested in rest if no sol */
		struct isl_basic_set *bset;
		bset = isl_basic_set_from_basic_map(isl_basic_map_copy(bmap));
		bset = isl_basic_set_drop_vars(bset, bmap->n_in, bmap->n_out);
		if (!bset)
			goto error;
		*data->rest = isl_set_add(*data->rest, bset);
	}

	if (isl_basic_map_free_inequality(bmap, 2*(bmap->n_div - old_n_div)))
		goto error;
	if (isl_basic_map_free_div(bmap, bmap->n_div - old_n_div))
		goto error;
	return map;
error:
	isl_map_free(map);
	return NULL;
}

/*
 * Returns a map with "context" as domain and as range the first
 * "keep" variables in the quast lists.
 */
static struct isl_map *isl_map_from_quast(struct isl_ctx *ctx, PipQuast *q,
		unsigned keep,
		struct isl_basic_set *context,
		struct isl_set **rest)
{
	int		pip_param;
	int		nexist;
	int		max_depth;
	int		n_sol, n_nosol;
	struct scan_data	data;
	struct isl_map		*map;
	unsigned		nparam;

	data.ctx = ctx;
	data.rest = rest;
	data.bmap = NULL;
	data.pos = NULL;

	if (!context)
		goto error;

	nparam = context->nparam;
	pip_param = nparam + context->dim;

	max_depth = 0;
	n_sol = 0;
	n_nosol = 0;
	nexist = pip_param-1;
	quast_count(q, &nexist, 0, &max_depth, &n_sol, &n_nosol);
	nexist -= pip_param-1;

	if (rest) {
		*rest = isl_set_alloc(data.ctx, nparam, context->dim, n_nosol,
					ISL_MAP_DISJOINT);
		if (!*rest)
			goto error;
	}
	map = isl_map_alloc(data.ctx, nparam, context->dim, keep, n_sol,
				ISL_MAP_DISJOINT);
	if (!map)
		goto error;

	data.bmap = isl_basic_map_from_basic_set(context,
				context->dim, 0);
	data.bmap = isl_basic_map_extend(data.bmap,
	    nparam, data.bmap->n_in, keep, nexist, keep, max_depth+2*nexist);
	if (!data.bmap)
		goto error;

	if (data.bmap->extra) {
		int i;
		data.pos = isl_alloc_array(ctx, int, data.bmap->extra);
		if (!data.pos)
			goto error;
		for (i = 0; i < data.bmap->n_div; ++i)
			data.pos[i] = i;
	}

	map = scan_quast_r(&data, q, map);
	map = isl_map_finalize(map);
	if (!map)
		goto error;
	if (rest) {
		*rest = isl_set_finalize(*rest);
		if (!*rest)
			goto error;
	}
	isl_basic_map_free(data.bmap);
	if (data.pos)
		free(data.pos);
	return map;
error:
	if (data.pos)
		free(data.pos);
	isl_basic_map_free(data.bmap);
	isl_map_free(map);
	if (rest) {
		isl_set_free(*rest);
		*rest = NULL;
	}
	return NULL;
}

static void copy_values_to(Entier *dst, isl_int *src, unsigned n)
{
	int i;

	for (i = 0; i < n; ++i)
		entier_assign(dst[i], src[i]);
}

static void copy_constraint_to(Entier *dst, isl_int *src,
		unsigned pip_param, unsigned pip_var,
		unsigned extra_front, unsigned extra_back)
{
	copy_values_to(dst+1+extra_front+pip_var+pip_param+extra_back, src, 1);
	copy_values_to(dst+1+extra_front+pip_var, src+1, pip_param);
	copy_values_to(dst+1+extra_front, src+1+pip_param, pip_var);
}

PipMatrix *isl_basic_map_to_pip(struct isl_basic_map *bmap, unsigned pip_param,
			 unsigned extra_front, unsigned extra_back)
{
	int i;
	unsigned nrow;
	unsigned ncol;
	PipMatrix *M;
	unsigned off;
	unsigned pip_var = bmap->nparam + bmap->n_in + bmap->n_out
				+ bmap->n_div - pip_param;
	unsigned pip_dim = pip_var - bmap->n_div;

	nrow = extra_front + bmap->n_eq + bmap->n_ineq;
	ncol = 1 + extra_front + pip_var + pip_param + extra_back + 1;
	M = pip_matrix_alloc(nrow, ncol);
	if (!M)
		return NULL;

	off = extra_front;
	for (i = 0; i < bmap->n_eq; ++i) {
		entier_set_si(M->p[off+i][0], 0);
		copy_constraint_to(M->p[off+i], bmap->eq[i],
				   pip_param, pip_var, extra_front, extra_back);
	}
	off += bmap->n_eq;
	for (i = 0; i < bmap->n_ineq; ++i) {
		entier_set_si(M->p[off+i][0], 1);
		copy_constraint_to(M->p[off+i], bmap->ineq[i],
				   pip_param, pip_var, extra_front, extra_back);
	}
	return M;
}

static struct isl_map *extremum_on(
		struct isl_basic_map *bmap, struct isl_basic_set *dom,
		struct isl_set **empty, int max)
{
	PipOptions	*options;
	PipQuast	*sol;
	struct isl_map	*map;
	struct isl_ctx  *ctx;
	PipMatrix *domain = NULL, *context = NULL;

	if (!bmap || !dom)
		goto error;

	ctx = bmap->ctx;
	isl_assert(ctx, bmap->nparam == dom->nparam, goto error);
	isl_assert(ctx, bmap->n_in == dom->dim, goto error);

	domain = isl_basic_map_to_pip(bmap, bmap->nparam + bmap->n_in,
					0, dom->n_div);
	if (!domain)
		goto error;
	context = isl_basic_map_to_pip((struct isl_basic_map *)dom, 0, 0, 0);
	if (!context)
		goto error;

	options = pip_options_init();
	options->Simplify = 1;
	options->Maximize = max;
	options->Urs_unknowns = -1;
	options->Urs_parms = -1;
	sol = pip_solve(domain, context, -1, options);

	if (sol) {
		struct isl_basic_set *copy;
		copy = isl_basic_set_copy(dom);
		map = isl_map_from_quast(ctx, sol, bmap->n_out, copy, empty);
	} else {
		map = isl_map_empty(ctx, bmap->nparam, bmap->n_in, bmap->n_out);
		if (empty)
			*empty = NULL;
	}
	if (!map)
		goto error;
	if (map->n == 0 && empty) {
		isl_set_free(*empty);
		*empty = isl_set_from_basic_set(dom);
	} else
		isl_basic_set_free(dom);
	isl_basic_map_free(bmap);

	pip_quast_free(sol);
	pip_options_free(options);
	pip_matrix_free(domain);
	pip_matrix_free(context);

	return map;
error:
	if (domain)
		pip_matrix_free(domain);
	if (context)
		pip_matrix_free(context);
	isl_basic_map_free(bmap);
	isl_basic_set_free(dom);
	return NULL;
}

struct isl_map *isl_pip_basic_map_lexmax(
		struct isl_basic_map *bmap, struct isl_basic_set *dom,
		struct isl_set **empty)
{
	return extremum_on(bmap, dom, empty, 1);
}

struct isl_map *isl_pip_basic_map_lexmin(
		struct isl_basic_map *bmap, struct isl_basic_set *dom,
		struct isl_set **empty)
{
	return extremum_on(bmap, dom, empty, 0);
}

struct isl_map *isl_pip_basic_map_compute_divs(struct isl_basic_map *bmap)
{
	PipMatrix *domain = NULL, *context = NULL;
	PipOptions	*options;
	PipQuast	*sol;
	struct isl_ctx  *ctx;
	struct isl_map	*map;
	struct isl_set	*set;
	struct isl_basic_set	*dom;
	unsigned	 n_in;
	unsigned	 n_out;

	if (!bmap)
		goto error;

	ctx = bmap->ctx;
	n_in = bmap->n_in;
	n_out = bmap->n_out;

	domain = isl_basic_map_to_pip(bmap, bmap->nparam + n_in + n_out, 0, 0);
	if (!domain)
		goto error;
	context = pip_matrix_alloc(0, bmap->nparam + n_in + n_out + 2);
	if (!context)
		goto error;

	options = pip_options_init();
	options->Simplify = 1;
	options->Urs_unknowns = -1;
	options->Urs_parms = -1;
	sol = pip_solve(domain, context, -1, options);

	dom = isl_basic_set_alloc(ctx, bmap->nparam, n_in + n_out, 0, 0, 0);
	map = isl_map_from_quast(ctx, sol, 0, dom, NULL);

	pip_quast_free(sol);
	pip_options_free(options);
	pip_matrix_free(domain);
	pip_matrix_free(context);

	isl_basic_map_free(bmap);

	set = isl_map_domain(map);

	return isl_map_from_set(set, n_in, n_out);
error:
	if (domain)
		pip_matrix_free(domain);
	if (context)
		pip_matrix_free(context);
	isl_basic_set_free(dom);
	isl_basic_map_free(bmap);
	return NULL;
}
