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
	char *const src = "int main(void) { int i = 1; return 0; }";
	char *argv[] = {"cepl", NULL};
	char *const cc_args[] = {
		"gcc", "-O2", "-pipe", "-Wall", "-Wextra",
		"-pedantic-errors", "-std=c11", "-S", "-xc",
		"/proc/self/fd/0", "-o", "/proc/self/fd/1", NULL
	};
	struct var_list vars = { 0, NULL };

	plan(2);

	ok(find_vars(&vars, src, cc_args, argv), "succeed finding variable values.");
	ok(extract_type("int foo = 5", "foo") == T_INT, "succeed finding variable values.");

	done_testing();
}
