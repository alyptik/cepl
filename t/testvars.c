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
		"int main(void)\n{\n\t"
		"int a = 1; int b = 456; double res = a + (double)b / 1000;\n";

	plan(6);

	init_list(&ids, NULL);
	init_tlist(&types);
	ok(find_vars(src, &ids, &types) == 3, "succeed finding three variable values.");
	ok(extract_type(src, "a") == T_INT, "succeed extracting integral type from `a`.");
	ok(extract_type(src, "b") == T_INT, "succeed extracting integral type from `b`.");
	ok(extract_type(src, "res") == T_DBL, "succeed extracting floating type from `res`.");
	ok(extract_type("unsigned long long foo = 5", "foo") == T_UINT, "succeed extracting unsigned int type.");
	ok(extract_type("struct bar baz[] = 5", "baz") == T_PTR, "succeed extracting pointer type from array.");

	free_str_list(&ids);
	if (types.list)
	free(types.list);
	done_testing();
}
