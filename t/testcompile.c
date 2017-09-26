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

	/* redirect stdout to /dev/null */
	int saved_fd, null_fd;
	if (!(null_fd = open("/dev/null", O_WRONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)))
		ERR("/dev/null open()");
	saved_fd = dup(STDERR_FILENO);
	dup2(null_fd, STDOUT_FILENO);
	dup2(null_fd, STDERR_FILENO);
	lives_ok({pipe_fd(-1, -1);}, "test living through pipe_fd() call with invalid fds.");
	dies_ok({compile(NULL, NULL, argv);}, "die passing a NULL pointer to compile().");
	ok(compile(src, cc_args, argv) == 0, "succeed compiling program.");
	dup2(saved_fd, STDERR_FILENO);
	close(null_fd);

	done_testing();
}
