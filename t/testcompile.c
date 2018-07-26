/*
 * t/testcompile.c - unit-test for compile.c
 *
 * AUTHOR: Joey Pabalinas <joeypabalinas@gmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#include "tap.h"
#include "../src/compile.h"

int main(void)
{
	char *argv[] = {"cepl", NULL};
	char *const src = "int main(void)\n{\nreturn 0;\n}";
	char *const cc_args[] = {
		"gcc",
		"-O0", "-pipe",
		"-fPIC", "-std=c11",
		"-Wall", "-Wextra",
		"-pedantic-errors",
		"-S", "-xc", "/dev/stdin",
		"-lm", "-o", "/dev/stdout",
		NULL
	};

	plan(3);

	lives_ok({pipe_fd(-1, -1);}, "test living through pipe_fd() call with invalid fds.");
	dies_ok({compile(NULL, NULL, argv, true);}, "die passing a NULL pointer to compile().");
	ok(compile(src, cc_args, argv, true) == 0, "succeed compiling program.");

	done_testing();
}
