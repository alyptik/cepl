/*
 * compile.c - compile the generated source code
 *
 * AUTHOR: Joey Pabalinas <joeypabalinas@gmail.com>
 * See LICENSE.md file for copyright and license details.
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

/* fallback linker arg array */
static char *const ld_alt_list[] = {
	"gcc", "-pipe",
	"-O0", "-fPIC",
	"-xassembler", "-",
	"-lm", "-o/tmp/cepl_program",
	NULL
};

extern char **environ;

int compile(char const *restrict src, char *const cc_args[], char *const exec_args[], bool show_errors)
{
	int null_fd, mem_fd, status, prog_fd;
	int pipe_cc[2], pipe_ld[2], pipe_exec[2];
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
	if (pipe2(pipe_ld, O_CLOEXEC) == -1)
		ERR("%s", "error making pipe_ld pipe");
	if (pipe2(pipe_exec, O_CLOEXEC) == -1)
		ERR("%s", "error making pipe_exec pipe");

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
		ERR("%s", "error forking compiler");
		break;

	/* child */
	case 0:
		if (!show_errors)
			dup2(null_fd, STDERR_FILENO);
		dup2(pipe_cc[0], STDIN_FILENO);
		dup2(pipe_ld[1], STDOUT_FILENO);
		execvp(cc_args[0], cc_args);
		/* execvp() should never return */
		ERR("%s", "error forking compiler");
		break;

	/* parent */
	default:
		close(pipe_cc[0]);
		close(pipe_ld[1]);
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

	/* fork linker */
	switch (fork()) {
	/* error */
	case -1:
		close(pipe_ld[0]);
		close(pipe_exec[0]);
		close(pipe_exec[1]);
		ERR("%s", "error forking linker");
		break;

	/* child */
	case 0:
		if (!show_errors)
			dup2(null_fd, STDERR_FILENO);
		dup2(pipe_ld[0], STDIN_FILENO);
		dup2(pipe_exec[1], STDOUT_FILENO);
		if (ld_list.list)
			execvp(ld_list.list[0], ld_list.list);
		/* fallback linker exec */
		execvp(ld_alt_list[0], ld_alt_list);
		/* execvp() should never return */
		ERR("%s", "error forking linker");
		break;

	/* parent */
	default:
		close(pipe_ld[0]);
		close(pipe_exec[1]);
		wait(&status);
		/* convert 255 to -1 since WEXITSTATUS() only returns the low-order 8 bits */
		if (WIFEXITED(status) && WEXITSTATUS(status)) {
			if (show_errors)
				WARNX("%s", "linker returned non-zero exit code");
			return (WEXITSTATUS(status) != 0xff) ? WEXITSTATUS(status) : -1;
		}
	}

	/* fork executable */
	switch (fork()) {
	/* error */
	case -1:
		close(pipe_exec[0]);
		ERR("%s", "error forking executable");
		break;

	/* child */
	case 0:
		reset_handlers();
		/*
		 * if ((mem_fd = memfd_create("cepl", 0)) == -1)
		 *         ERR("%s", "error creating mem_fd");
		 * pipe_fd(pipe_exec[0], mem_fd);
		 * fexecve(mem_fd, exec_args, environ);
		 */
		execve("/tmp/cepl_program", exec_args, environ);
		/* fexecve() should never return */
		ERR("%s", "error forking executable");
		break;

	/* parent */
	default:
		close(pipe_exec[0]);
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
