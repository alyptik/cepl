/*
 * t/testcompile.c - unit-test for compile.c
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE file for copyright and license details.
 */

#include <limits.h>
#include "tap.h"
#include "../src/compile.h"

int main(void)
{
	char *const src = "int main(void) { return 0; }";
	char *argv[] = {"cepl", NULL};
	char *const cc_args[] = {
		"clang", "-O2", "-pipe", "-Wall", "-Wextra",
		"-pedantic-errors", "-std=c11", "-S", "-xc",
		"/proc/self/fd/0", "-o", "/proc/self/fd/1", NULL
	};

	plan(2);

	ok(compile(src, cc_args, argv) == 0, "fail compiling program.");
	lives_ok({pipe_fd(INT_MAX, INT_MAX);}, "test pipe_fd() with invalid fds.");

	done_testing();
}
