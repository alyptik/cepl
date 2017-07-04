/*
 * compile.c - compile the generated source code
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE file for copyright and license details.
 */

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <linux/memfd.h>
#include "compile.h"
#include "parseopts.h"

/* global linker arguments struct */
struct str_list ld_list = { 0, NULL };

/* fallback linker arg array */
static char *const ld_alt_list[] = {
	"gcc", "-xassembler", "/proc/self/fd/0",
	"-o/proc/self/fd/1", NULL
};

extern char **environ;

/* silence linter */
long syscall(long number, ...);
int fexecve(int mem_fd, char *const argv[], char *const envp[]);

int compile(char *const src, char *const cc_args[], char *const exec_args[])
{
	int mem_fd, status;
	int pipe_cc[2], pipe_ld[2], pipe_exec[2];

	/* create pipes */
	pipe(pipe_cc);
	pipe(pipe_ld);
	pipe(pipe_exec);
	/* set close-on-exec for pipe fds */
	set_cloexec(pipe_cc[0]);
	set_cloexec(pipe_cc[1]);
	set_cloexec(pipe_ld[0]);
	set_cloexec(pipe_ld[1]);
	set_cloexec(pipe_exec[0]);
	set_cloexec(pipe_exec[1]);

	/* fork compiler */
	switch (fork()) {
	/* error */
	case -1:
		close(pipe_cc[0]);
		close(pipe_cc[1]);
		close(pipe_ld[0]);
		close(pipe_ld[1]);
		close(pipe_exec[0]);
		close(pipe_exec[1]);
		err(EXIT_FAILURE, "%s", "error forking compiler");
		break;

	/* child */
	case 0:
		dup2(pipe_cc[0], 0);
		dup2(pipe_ld[1], 1);
		execvp(cc_args[0], cc_args);
		/* execvp() should never return */
		err(EXIT_FAILURE, "%s", "error forking compiler");
		break;

	/* parent */
	default:
		close(pipe_cc[0]);
		close(pipe_ld[1]);
		write(pipe_cc[1], src, strlen(src));
		close(pipe_cc[1]);
		wait(&status);
		if (status != 0) {
			warnx("%s", "compiler returned non-zero exit code");
			return status;
		}
	}

	/* fork linker */
	switch (fork()) {
	/* error */
	case -1:
		close(pipe_ld[0]);
		close(pipe_exec[0]);
		close(pipe_exec[1]);
		err(EXIT_FAILURE, "%s", "error forking linker");
		break;

	/* child */
	case 0:
		dup2(pipe_ld[0], 0);
		dup2(pipe_exec[1], 1);
		if (ld_list.list)
			execvp(ld_list.list[0], ld_list.list);
		/* fallback linker exec */
		execvp(ld_alt_list[0], ld_alt_list);
		/* execvp() should never return */
		err(EXIT_FAILURE, "%s", "error forking linker");
		break;

	/* parent */
	default:
		close(pipe_ld[0]);
		close(pipe_exec[1]);
		wait(&status);
		if (status != 0) {
			warnx("%s", "linker returned non-zero exit code");
			return status;
		}
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
		pipe_fd(pipe_exec[0], mem_fd);
		fexecve(mem_fd, exec_args, environ);
		/* fexecve() should never return */
		err(EXIT_FAILURE, "%s", "error forking executable");
		break;

	/* parent */
	default:
		close(pipe_exec[0]);
		wait(&status);
		/* don't overwrite non-zero exit status from compiler */
		if (status != 0) {
			warnx("%s", "executable returned non-zero exit code");
			return status;
		}
	}

	return 0;
}
