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
	char *const src = "int main(void)\n{\nint i = 0;\n";

	plan(4);

	init_list(&ids, NULL);
	init_tlist(&types);
	ok(find_vars(src, &ids, &types) > 0, "succeed finding variable values.");
	ok(extract_type(src, "i") == T_INT, "succeed extracting int type.");
	ok(extract_type("unsigned long long foo = 5", "foo") == T_UINT, "succeed extracting unsigned int type.");
	ok(extract_type("struct bar baz[] = 5", "baz") == T_PTR, "succeed extracting pointer type from array.");

	free_str_list(&ids);
	if (types.list)
	free(types.list);
	done_testing();
}
