/*
 * t/testvars.c - unit-test for vars.c
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#include "tap.h"
#include "../src/vars.h"

int main(void)
{
	char *const src = "int main(void) { int i = 0; return i; }";
	char *argv[] = {"cepl", NULL};
	char *const cc_args[] = {
		"gcc", "-O2", "-pipe", "-Wall", "-Wextra",
		"-pedantic-errors", "-std=c11", "-S", "-xc",
		"/proc/self/fd/0", "-o", "/proc/self/fd/1", NULL
	};
	struct str_list strs = {0, NULL};
	struct var_list vars = {0, NULL};
	enum var_type *types = NULL;

	plan(3);

	ok(find_vars(src, &strs, &types), "succeed finding variable values.");
	ok(extract_type("unsigned long long foo = 5", "foo") == T_UINT, "succeed extracting type.");
	ok(extract_type("struct bar baz[] = 5", "baz") == T_PTR, "succeed extracting pointer type from array.");

	done_testing();

	free(types);
}
