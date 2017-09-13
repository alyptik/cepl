/*
 * t/testcompile.c - unit-test for compile.c
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#include "tap.h"
#include "../src/compile.h"

int main(void)
{
	char *const src = "int main(void)\n{\nreturn 0;\n}";
	char *argv[] = {"cepl", NULL};
	char *const cc_args[] = {
		"gcc",
		"-O0", "-pipe",
		"-Wall", "-Wextra",
		"-pedantic-errors",
		"-std=c11", "-S", "-xc",
		"/proc/self/fd/0",
		"-o", "/proc/self/fd/1", NULL
	};

	plan(3);

	lives_ok({pipe_fd(-1, -1);}, "test living through pipe_fd() call with invalid fds.");
	dies_ok({compile(NULL, NULL, argv);}, "die passing a NULL pointer to compile().");
	ok(compile(src, cc_args, argv) == 0, "succeed compiling program.");

	done_testing();
}
