/* Copyright 2016-2017 Tobias Grosser
 *
 * Use of this software is governed by the MIT license
 *
 * Written by Tobias Grosser, Weststrasse 47, CH-8003, Zurich
 */

#include <vector>
#include <string>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include <isl/options.h>
#include "interface/isl-noexceptions.h"

static void assert_impl(bool condition, const char *file, int line,
	const char *message)
{
	if (condition)
		return;

	fprintf(stderr, "Assertion failed in %s:%d %s\n", file, line, message);
	exit(EXIT_FAILURE);
}

static void assert_impl(isl::boolean condition, const char *file, int line,
	const char *message)
{
	assert_impl(bool(condition), file, line, message);
}

#define assert(exp) assert_impl(exp, __FILE__, __LINE__, #exp)

#include "isl_test_cpp-generic.cc"

/* Test that isl_bool values are returned correctly.
 *
 * We check in detail the following parts of the isl::boolean class:
 *  - The is_true, is_false, and is_error functions return true in case they
 *    are called on a true, false, or error instance of isl::boolean,
 *    respectively
 *  - Explicit conversion to 'bool'
 *  - Implicit conversion to 'bool'
 *  - The complement operator
 *  - Explicit construction from 'true' and 'false'
 *  - Explicit construction form isl_bool
 */
void test_return_bool(isl::ctx ctx)
{
	isl::set empty(ctx, "{ : false }");
	isl::set univ(ctx, "{ : }");
	isl::set null;

	isl::boolean b_true = empty.is_empty();
	isl::boolean b_false = univ.is_empty();
	isl::boolean b_error = null.is_empty();

	assert(b_true.is_true());
	assert(!b_true.is_false());
	assert(!b_true.is_error());

	assert(!b_false.is_true());
	assert(b_false.is_false());
	assert(!b_false.is_error());

	assert(!b_error.is_true());
	assert(!b_error.is_false());
	assert(b_error.is_error());

	assert(bool(b_true) == true);
	assert(bool(b_false) == false);

	assert(b_true);

	assert((!b_false).is_true());
	assert((!b_true).is_false());
	assert((!b_error).is_error());

	assert(isl::boolean(true).is_true());
	assert(!isl::boolean(true).is_false());
	assert(!isl::boolean(true).is_error());

	assert(isl::boolean(false).is_false());
	assert(!isl::boolean(false).is_true());
	assert(!isl::boolean(false).is_error());

	assert(isl::manage(isl_bool_true).is_true());
	assert(!isl::manage(isl_bool_true).is_false());
	assert(!isl::manage(isl_bool_true).is_error());

	assert(isl::manage(isl_bool_false).is_false());
	assert(!isl::manage(isl_bool_false).is_true());
	assert(!isl::manage(isl_bool_false).is_error());

	assert(isl::manage(isl_bool_error).is_error());
	assert(!isl::manage(isl_bool_error).is_true());
	assert(!isl::manage(isl_bool_error).is_false());
}

/* Test that return values are handled correctly.
 *
 * Test that isl C++ objects, integers, boolean values, and strings are
 * returned correctly.
 */
void test_return(isl::ctx ctx)
{
	test_return_obj(ctx);
	test_return_int(ctx);
	test_return_bool(ctx);
	test_return_string(ctx);
}

/* Test that foreach functions are modeled correctly.
 *
 * Verify that lambdas are correctly called as callback of a 'foreach'
 * function and that variables captured by the lambda work correctly. Also
 * check that the foreach function takes account of the return value of the
 * lambda and aborts in case isl::stat::error is returned and then returns
 * isl::stat::error itself.
 */
void test_foreach(isl::ctx ctx)
{
	isl::set s(ctx, "{ [0]; [1]; [2] }");

	std::vector<isl::basic_set> basic_sets;

	auto add_to_vector = [&] (isl::basic_set bs) {
		basic_sets.push_back(bs);
		return isl::stat::ok;
	};

	isl::stat ret1 = s.foreach_basic_set(add_to_vector);

	assert(ret1 == isl::stat::ok);
	assert(basic_sets.size() == 3);
	assert(isl::set(basic_sets[0]).is_subset(s));
	assert(isl::set(basic_sets[1]).is_subset(s));
	assert(isl::set(basic_sets[2]).is_subset(s));
	assert(!basic_sets[0].is_equal(basic_sets[1]));

	auto fail = [&] (isl::basic_set bs) {
		return isl::stat::error;
	};

	isl::stat ret2 = s.foreach_basic_set(fail);

	assert(ret2 == isl::stat::error);
}

/* Test basic schedule tree functionality.
 *
 * In particular, create a simple schedule tree and
 * - perform some generic tests
 * - test map_descendant_bottom_up in the failing case
 * - test foreach_descendant_top_down
 * - test every_descendant
 */
static void test_schedule_tree(isl::ctx ctx)
{
	auto root = test_schedule_tree_generic(ctx);

	auto fail_map = [](isl::schedule_node node) {
		return isl::schedule_node();
	};
	assert(root.map_descendant_bottom_up(fail_map).is_null());

	int count = 0;
	auto inc_count = [&count](isl::schedule_node node) {
		count++;
		return isl::boolean(true);
	};
	root.foreach_descendant_top_down(inc_count);
	assert(count == 8);

	count = 0;
	auto inc_count_once = [&count](isl::schedule_node node) {
		count++;
		return isl::boolean(false);
	};
	root.foreach_descendant_top_down(inc_count_once);
	assert(count == 1);

	auto is_not_domain = [](isl::schedule_node node) {
		return !node.isa<isl::schedule_node_domain>();
	};
	assert(root.child(0).every_descendant(is_not_domain).is_true());
	assert(root.every_descendant(is_not_domain).is_false());

	auto fail = [](isl::schedule_node node) {
		return isl::boolean();
	};
	assert(root.every_descendant(fail).is_error());

	auto domain = root.as<isl::schedule_node_domain>().get_domain();
	auto filters = isl::union_set(ctx, "{}");
	auto collect_filters = [&filters](isl::schedule_node node) {
		if (auto filter = node.as<isl::schedule_node_filter>())
			filters = filters.unite(filter.get_filter());
		return isl::boolean(true);
	};
	root.every_descendant(collect_filters);
	assert(domain.is_equal(filters));
}

/* Test the isl C++ interface
 *
 * This includes:
 *  - The isl C <-> C++ pointer interface
 *  - Object construction
 *  - Different parameter types
 *  - Different return types
 *  - Foreach functions
 *  - Schedule trees
 *  - AST generation
 */
int main()
{
	isl_ctx *ctx = isl_ctx_alloc();

	isl_options_set_on_error(ctx, ISL_ON_ERROR_ABORT);

	test_pointer(ctx);
	test_constructors(ctx);
	test_parameters(ctx);
	test_return(ctx);
	test_foreach(ctx);
	test_schedule_tree(ctx);
	test_ast_build(ctx);

	isl_ctx_free(ctx);
}
