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
size_t strnlen(const char *s, size_t maxlen);

int compile(char *const src, char *const cc_args[], char *const exec_args[])
{
	if (!src || !cc_args || !exec_args)
		errx(EXIT_FAILURE, "%s", "NULL pointer passed to compile()");

	int mem_fd, status;
	int pipe_cc[2], pipe_ld[2], pipe_exec[2];
	char src_buffer[strnlen(src, COUNT) + 1];

	if (sizeof src_buffer < 2)
		errx(EXIT_FAILURE, "%s", "empty source string passed to compile()");

	/* add trailing '\n' */
	memcpy(src_buffer, src, sizeof src_buffer);
	src_buffer[sizeof src_buffer - 1] = '\n';
	/* create pipes */
	pipe(pipe_cc);
	pipe(pipe_ld);
	pipe(pipe_exec);
	/* set close-on-exec for pipe fds */
	set_cloexec(pipe_cc);
	set_cloexec(pipe_ld);
	set_cloexec(pipe_exec);

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
		write(pipe_cc[1], src_buffer, sizeof src_buffer);
		close(pipe_cc[1]);
		wait(&status);
		if (WIFEXITED(status) && WEXITSTATUS(status)) {
			warnx("%s", "compiler returned non-zero exit code");
			return WEXITSTATUS(status);
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
		if (WIFEXITED(status) && WEXITSTATUS(status)) {
			warnx("%s", "linker returned non-zero exit code");
			return WEXITSTATUS(status);
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
		if (WIFEXITED(status) && WEXITSTATUS(status)) {
			warnx("%s", "executable returned non-zero exit code");
			return WEXITSTATUS(status);
		}
	}

	return 0;
}
