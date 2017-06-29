/*
 * t/testcompile.c - unit-test for compile.c
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE file for copyright and license details.
 */

#include "tap.h"
#include <limits.h>
#include "../src/compile.h"

int main(void)
{
	char *const src = "int main(void) { return 0; }\n";
	char *argv[] = {"cepl", NULL};
	char *const cc_args[] = {
		"gcc", "-O2", "-pipe", "-Wall", "-Wextra",
		"-pedantic-errors", "-std=c11", "-xc",
		"/dev/stdin", "-o", "/dev/stdout", NULL
	};

	plan(2);

	ok(compile("gcc", src, cc_args, argv) == 0, "compile test program.");
	lives_ok({pipe_fd(INT_MAX, INT_MAX);}, "test pipe_fd() with invalid fds.");

	done_testing();
}
