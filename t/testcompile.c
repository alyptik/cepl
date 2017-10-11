/*
 * t/testcompile.c - unit-test for compile.c
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#include "tap.h"
#include "../src/compile.h"
#include <sys/stat.h>

int main(void)
{
	int saved_fd = dup(STDERR_FILENO);
	char *argv[] = {"cepl", NULL};
	char *const src = "int main(void)\n{\nreturn 0;\n}";
	char *const cc_args[] = {
		"gcc", "-O0", "-pipe",
		"-std=c11", "-fverbose-asm",
		"-Wall", "-Wextra",
		"-pedantic-errors",
		"-S", "-xc", "/dev/stdin",
		"-o", "/dev/stdout",
		NULL
	};

	plan(3);

	close(STDERR_FILENO);
	lives_ok({pipe_fd(-1, -1);}, "test living through pipe_fd() call with invalid fds.");
	dies_ok({compile(NULL, NULL, argv);}, "die passing a NULL pointer to compile().");
	ok(compile(src, cc_args, argv) == 0, "succeed compiling program.");
	dup2(saved_fd, STDERR_FILENO);

	done_testing();
}
