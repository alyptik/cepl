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

extern char **environ;

int compile(char const *src, char *const cc_args[], bool show_errors)
{
	int null_fd, status;
	int pipe_cc[2];
	size_t len = strlen(src);
	char *exec_args[] = {"/tmp/cepl_program", NULL};

	if (!src || !cc_args)
		ERRX("NULL pointer passed to compile()");
	if (!len)
		return 0;

	/* bit bucket */
	if ((null_fd = open("/dev/null", O_WRONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) == -1)
		ERR("open()");

	/* create pipe */
	if (pipe2(pipe_cc, O_CLOEXEC) == -1)
		ERR("error making pipe_cc pipe");

	/* fork compiler */
	switch (fork()) {
	/* error */
	case -1:
		close(pipe_cc[0]);
		close(pipe_cc[1]);
		ERR("error forking compiler");
		break;

	/* child */
	case 0:
		if (!show_errors)
			dup2(null_fd, STDERR_FILENO);
		dup2(pipe_cc[0], STDIN_FILENO);
		execvp(cc_args[0], cc_args);
		/* execvp() should never return */
		ERR("error forking compiler");
		break;

	/* parent */
	default:
		close(pipe_cc[0]);
		if (write(pipe_cc[1], src, len) == -1)
			ERR("error writing to pipe_cc[1]");
		close(pipe_cc[1]);
		wait(&status);
		/* convert 255 to -1 since WEXITSTATUS() only returns the low-order 8 bits */
		if (WIFEXITED(status) && WEXITSTATUS(status)) {
			if (show_errors)
				WARNX("compiler returned non-zero exit code");
			return (WEXITSTATUS(status) != 0xff) ? WEXITSTATUS(status) : -1;
		}
	}

	/* fork executable */
	switch (fork()) {
	/* error */
	case -1:
		ERR("error forking executable");
		break;

	/* child */
	case 0:
		reset_handlers();
		execve("/tmp/cepl_program", exec_args, environ);
		/* execve() should never return */
		ERR("error forking executable");
		break;

	/* parent */
	default:
		close(null_fd);
		wait(&status);
		if (unlink("/tmp/cepl_program") == -1)
			WARN("unable to remove /tmp/cepl_program");
		/* convert 255 to -1 since WEXITSTATUS() only returns the low-order 8 bits */
		if (WIFEXITED(status) && WEXITSTATUS(status)) {
			if (show_errors)
				WARNX("executable returned non-zero exit code");
			return (WEXITSTATUS(status) != 0xff) ? WEXITSTATUS(status) : -1;
		}
	}

	/* program returned success */
	return 0;
}
