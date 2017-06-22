/*
 * compile.c - compile the generated source code
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE file for copyright and license details.
 */

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/memfd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include "compile.h"

int compile(char const *cc, char *src, char *const ccargs[], char *const execargs[])
{
	int fd, pipecc[2], pipeexec[2];
	/* flags for pipemain[0] */
	int flags;

	/* create pipes */
	pipe(pipecc);
	pipe(pipeexec);
	pipe(pipemain);
	if (fcntl(pipecc[0], F_SETFD, FD_CLOEXEC) == -1)
		err(EXIT_FAILURE, "%s", "error during fnctl");
	if (fcntl(pipecc[1], F_SETFD, FD_CLOEXEC) == -1)
		err(EXIT_FAILURE, "%s", "error during fnctl");
	if (fcntl(pipeexec[0], F_SETFD, FD_CLOEXEC) == -1)
		err(EXIT_FAILURE, "%s", "error during fnctl");
	if (fcntl(pipeexec[1], F_SETFD, FD_CLOEXEC) == -1)
		err(EXIT_FAILURE, "%s", "error during fnctl");
	if (fcntl(pipemain[0], F_SETFD, FD_CLOEXEC) == -1)
		err(EXIT_FAILURE, "%s", "error during fnctl");
	if (fcntl(pipemain[1], F_SETFD, FD_CLOEXEC) == -1)
		err(EXIT_FAILURE, "%s", "error during fnctl");
	flags = fcntl(pipemain[0], F_GETFL, 0);
	if (fcntl(pipemain[0], F_SETFL, flags | O_NONBLOCK) == -1)
		err(EXIT_FAILURE, "%s", "error during fnctl");
	flags = fcntl(pipemain[1], F_GETFL, 0);
	if (fcntl(pipemain[1], F_SETFL, flags | O_NONBLOCK) == -1)
		err(EXIT_FAILURE, "%s", "error during fnctl");

	/* fork compiler */
	switch (fork()) {
	/* error */
	case -1:
		err(EXIT_FAILURE, "%s", "error forking compiler");
		break;

	/* child */
	case 0:
		dup2(pipecc[0], 0);
		dup2(pipeexec[1], 1);
		close(pipecc[1]);
		close(pipeexec[0]);
		execvp(cc, ccargs);
		/* execvp() should never return */
		err(EXIT_FAILURE, "%s", "error forking compiler");
		break;

	/* parent */
	default:
		write(pipecc[1], src, strlen(src));
		close(pipecc[0]);
		close(pipecc[1]);
	}

	/* fork executable */
	switch (fork()) {
	/* error */
	case -1:
		err(EXIT_FAILURE, "%s", "error forking executable");
		break;

	/* child */
	case 0:
		execl("/tmp/cepl", "cepl", NULL);
		/* fexecve() should never return */
		err(EXIT_FAILURE, "%s", "execl returned");
		break;

/* TODO: fix memfd compile method */
		/*
		 * if ((buf = malloc(count)) == NULL)
		 *         err(EXIT_FAILURE, "%s", "error allocating buffer");
		 */

		/* read output from compiler in a loop */
		/*
		 * for (;;) {
		 *         int buflen;
		 *         if ((buflen = read(pipeexec[0], buf, count)) == -1) {
		 *                 free(buf);
		 *                 buf = NULL;
		 *                 err(EXIT_FAILURE, "%s", "error reading stdout from compiler");
		 *         }
		 *         [> break on EOF <]
		 *         if (buflen == 0)
		 *         break;
		 *         if (write(fd, buf, buflen) == -1) {
		 *                 free(buf);
		 *                 buf = NULL;
		 *                 err(EXIT_FAILURE, "%s", "error writing to memfd");
		 *         }
		 * }
		 */

		/*
		 * free(buf);
		 * buf = NULL;
		 */

		if ((fd = syscall(SYS_memfd_create, "cepl", MFD_CLOEXEC)) == -1)
			err(EXIT_FAILURE, "%s", "error creating memfd");

		pipe_fd(pipeexec[0], fd);
		close(pipeexec[0]);
		dup2(pipemain[1], STDOUT_FILENO);
		fexecve(fd, execargs, environ);
		/* fexecve() should never return */
		err(EXIT_FAILURE, "%s", "error forking executable");
		break;
/* TODO: fix memfd compile method */

	/* parent */
	default:
		close(pipeexec[0]);
		close(pipeexec[1]);
	}

	return pipemain[0];
}

inline int pipe_fd(int in_fd, int out_fd)
{
	int total = 0;
	/* read output in a loop */
	for (;;) {
		int buflen;
		/* 2 MB */
		size_t count = 1024 * 1024 * 2;
		char buf[count];
		if ((buflen = read(in_fd, buf, count)) == -1) {
			if (errno == EINTR || errno == EAGAIN) {
				continue;
			} else {
				err(EXIT_FAILURE, "%s", "error reading input fd");
			}
		}
		total += buflen;
		/* break on EOF */
		if (buflen == 0)
			break;
		if (write(out_fd, buf, buflen) == -1) {
			if (errno == EINTR || errno == EAGAIN) {
				continue;
			} else {
			err(EXIT_FAILURE, "%s", "error writing to output fd");
			}
		}
	}
	return total;
}
