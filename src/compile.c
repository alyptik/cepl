/*
 * compile.c - compile the generated source code
 *
 * AUTHOR: Joey Pabalinas <joeypabalinas@gmail.com>
 * See LICENSE file for copyright and license details.
 */

/* silence linter */
#undef _GNU_SOURCE
#define _GNU_SOURCE

#include "compile.h"
#include "parseopts.h"
#include <linux/memfd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>

/* global linker arguments struct */
struct str_list ld_list;

extern char **environ;

int compile(char const *restrict src, char *const cc_args[], char *const exec_args[], bool show_errors)
{
	int null_fd, status, prog_fd;
	int pipe_cc[2];
	size_t len = strlen(src);

	if (!src || !cc_args || !exec_args)
		ERRX("%s", "NULL pointer passed to compile()");
	if (!len)
		return 0;

	/* bit bucket */
	if ((null_fd = open("/dev/null", O_WRONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) == -1)
		ERR("%s", "open()");

	/* create pipes */
	if (pipe2(pipe_cc, O_CLOEXEC) == -1)
		ERR("%s", "error making pipe_cc pipe");

	/* fork compiler */
	switch (fork()) {
	/* error */
	case -1:
		close(pipe_cc[0]);
		close(pipe_cc[1]);
		ERR("%s", "error forking compiler");
		break;

	/* child */
	case 0:
		if (!show_errors)
			dup2(null_fd, STDERR_FILENO);
		dup2(pipe_cc[0], STDIN_FILENO);
		execvp(cc_args[0], cc_args);
		/* execvp() should never return */
		ERR("%s", "error forking compiler");
		break;

	/* parent */
	default:
		close(pipe_cc[0]);
		if (write(pipe_cc[1], src, len) == -1)
			ERR("%s", "error writing to pipe_cc[1]");
		close(pipe_cc[1]);
		wait(&status);
		/* convert 255 to -1 since WEXITSTATUS() only returns the low-order 8 bits */
		if (WIFEXITED(status) && WEXITSTATUS(status)) {
			if (show_errors)
				WARNX("%s", "compiler returned non-zero exit code");
			return (WEXITSTATUS(status) != 0xff) ? WEXITSTATUS(status) : -1;
		}
	}

	/* fork executable */
	switch (fork()) {
	/* error */
	case -1:
		ERR("%s", "error forking executable");
		break;

	/* child */
	case 0:
		reset_handlers();
		execve("/tmp/cepl_program", exec_args, environ);
		/* execve() should never return */
		ERR("%s", "error forking executable");
		break;

	/* parent */
	default:
		close(null_fd);
		wait(&status);
		if (unlink("/tmp/cepl_program") == -1)
			WARN("%s", "unable to remove /tmp/cepl_program");
		/* convert 255 to -1 since WEXITSTATUS() only returns the low-order 8 bits */
		if (WIFEXITED(status) && WEXITSTATUS(status)) {
			if (show_errors)
				WARNX("%s", "executable returned non-zero exit code");
			return (WEXITSTATUS(status) != 0xff) ? WEXITSTATUS(status) : -1;
		}
	}

	/* program returned success */
	return 0;
}
