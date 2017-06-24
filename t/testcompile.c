/*
 * t/testcompile.c - unit-test for compile.c
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE file for copyright and license details.
 */

#include "tap.h"
#include "../src/compile.h"

int main (void)
{
	char *const argv[] = {"testcompile", NULL};
	char *src = "int main(void) { return 0; }";
	char *const cc_args[] = {"gcc", "-O2", "-pipe", "-Wall", "-Wextra", "-pedantic-errors", "-std=c11", "-xc", "/dev/stdin", "-o", "/dev/stdout", NULL};

	plan(1);

	ok(compile("gcc", src, cc_args, argv) == 0, "compile test program.");

	done_testing();
}
