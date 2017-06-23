/*
 * compile.c - compile the generated source code
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE file for copyright and license details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <linux/memfd.h>
#include "compile.h"

int compile(char const *cc, char *src, char *const cc_args[], char *const exec_args[])
{
	int mem_fd, pipe_cc[2], pipe_exec[2];
	int status;
	int ret = 0;

	/* create pipes */
	pipe(pipe_cc);
	pipe(pipe_exec);
	/* set close-on-exec for pipe fds */
	set_cloexec(pipe_cc[0]);
	set_cloexec(pipe_cc[1]);
	set_cloexec(pipe_exec[0]);
	set_cloexec(pipe_exec[1]);

	/* fork compiler */
	switch (fork()) {
	/* error */
	case -1:
		close(pipe_cc[0]);
		close(pipe_cc[1]);
		close(pipe_exec[0]);
		close(pipe_exec[1]);
		err(EXIT_FAILURE, "%s", "error forking compiler");
		break;

	/* child */
	case 0:
		dup2(pipe_cc[0], 0);
		dup2(pipe_exec[1], 1);
		execvp(cc, cc_args);
		/* execvp() should never return */
		err(EXIT_FAILURE, "%s", "error forking compiler");
		break;

	/* parent */
	default:
		close(pipe_cc[0]);
		write(pipe_cc[1], src, strlen(src));
		close(pipe_cc[1]);
		close(pipe_exec[1]);
	}

	wait(&status);
	if (status != 0) {
		warn("%s", "compiler returned non-zero exit code");
		ret = status;
	}

	/* fork executable */
	switch (fork()) {
	/* error */
	case -1:
		close(pipe_exec[0]);
		err(EXIT_FAILURE, "%s", "error forking executable");
		break;

	/* child */
	case 0:
		if ((mem_fd = syscall(SYS_memfd_create, "cepl", MFD_CLOEXEC)) == -1)
			err(EXIT_FAILURE, "%s", "error creating mem_fd");
		if (pipe_fd(pipe_exec[0], mem_fd) == 0)
			err(EXIT_FAILURE, "%s", "zero bytes written by pipe_fd()");
		fexecve(mem_fd, exec_args, environ);
		/* fexecve() should never return */
		err(EXIT_FAILURE, "%s", "error forking executable");
		break;

	/* parent */
	default:
		close(pipe_exec[0]);
	}

	wait(&status);
	/* don't overwrite non-zero exit status from compiler */
	if (status != 0 && ret == 0) {
		warn("%s", "executable returned non-zero exit code");
		ret = status;
	}

	/* TODO: make return value more useful */
	return ret;
}
