/*
 * compile.c - compile the generated source code
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE file for copyright and license details.
 */

#include <err.h>
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

#define UNUSED __attribute__ ((unused))
extern char **environ;
/* silence linter */
long syscall(long number, ...);
int fexecve(int fd, char *const argv[], char *const envp[]);

int compile(char const *cc, char *src, char *const args[])
{
	int fd, pipecc[2], pipeexec[2];
	char *buf;
	/* 2MB */
	int count = 1024 * 1024 * 2;
	char *const execargv[] = {NULL};

	/* create pipes */
	pipe(pipecc);
	pipe(pipeexec);

	/* fork compiler */
	switch (fork()) {
	/* error */
	case -1:
		close(pipecc[0]);
		close(pipecc[1]);
		close(pipeexec[0]);
		close(pipeexec[1]);
		err(EXIT_FAILURE, "%s", "error forking compiler");
		break;

	/* child */
	case 0:
		dup2(pipecc[0], 0);
		dup2(pipeexec[1], 1);
		close(pipecc[0]);
		close(pipecc[1]);
		close(pipeexec[0]);
		close(pipeexec[1]);
		execvp(cc, args);
		/* execvp() should never return */
		err(EXIT_FAILURE, "%s", "error forking compiler");
		break;

	/* parent */
	default:
		write(pipecc[1], src, strlen(src));
		fsync(pipecc[1]);
		close(pipecc[0]);
		close(pipecc[1]);
	}

	/* fork executable */
	switch (fork()) {
	/* error */
	case -1:
		close(pipeexec[0]);
		close(pipeexec[1]);
		err(EXIT_FAILURE, "%s", "error forking executable");
		break;

	/* child */
	case 0:
		if ((fd = syscall(SYS_memfd_create, "cepl", MFD_CLOEXEC)) == -1)
			err(EXIT_FAILURE, "%s", "error creating memfd");

		if ((buf = malloc(count)) == NULL)
			err(EXIT_FAILURE, "%s", "error allocating buffer");

		/* read output from compiler in a loop */
		for (;;) {
			int buflen;
			if ((buflen = read(pipeexec[0], buf, count)) == -1) {
				free(buf);
				buf = NULL;
				err(EXIT_FAILURE, "%s", "error reading stdout from compiler");
			}
			/* break on EOF */
			if (buflen == 0)
			break;

			if (write(fd, buf, buflen) == -1) {
				free(buf);
				buf = NULL;
				err(EXIT_FAILURE, "%s", "error writing to memfd");
			}
		}

		free(buf);
		buf = NULL;
		fexecve(fd, execargv, environ);
		/* fexecve() should never return */
		err(EXIT_FAILURE, "%s", "error forking executable");
		break;

	/* parent */
	default:
		close(pipeexec[0]);
		close(pipeexec[1]);
	}

	return 0;
}
