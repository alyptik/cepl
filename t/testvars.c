/*
 * t/testvars.c - unit-test for vars.c
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#include "tap.h"
#include "../src/vars.h"

/* global linker arguments struct */
struct str_list ld_list;

int main(void)
{
	struct str_list ids = {0};
	struct type_list types = {0};
	char *const src =
		"char wark[] = \"wark\", *ptr = wark;"
		"long foo = 1, bar = 456;"
		"short baz = 50; int *quix = &baz;"
		"double res = foo + (double)bar / 1000;";

	plan(10);

	/* initialize lists */
	init_list(&ids, NULL);
	init_tlist(&types);

	ok(find_vars(src, &ids, &types) == 6, "succeed finding six variable values.");
	ok(extract_type(src, "ptr") == T_STR, "succeed extracting string type from `ptr`.");
	ok(extract_type(src, "wark") == T_STR, "succeed extracting string type from `val`.");
	ok(extract_type(src, "foo") == T_INT, "succeed extracting integral type from `foo`.");
	ok(extract_type(src, "bar") == T_INT, "succeed extracting integral type from `bar`.");
	ok(extract_type(src, "baz") == T_INT, "succeed extracting integral type from `baz`.");
	ok(extract_type(src, "quix") == T_PTR, "succeed extracting pointer type from `quix`.");
	ok(extract_type(src, "res") == T_DBL, "succeed extracting floating type from `res`.");
	ok(extract_type("unsigned long long foo = 5", "foo") == T_UINT, "succeed extracting unsigned int type.");
	ok(extract_type("struct bar baz[] = 5", "baz") == T_PTR, "succeed extracting pointer type from array.");

	/* cleanup */
	free_str_list(&ids);
	free(types.list);

	done_testing();
}
