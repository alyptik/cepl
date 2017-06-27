/*
 * compile.h - compile the generated source code
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE file for copyright and license details.
 */

#ifndef COMPILE_H
#define COMPILE_H 1

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#define COUNT sysconf(_SC_PAGESIZE)
extern char **environ;
int compile(char const *cc, char *src, char *const cc_args[], char *const exec_args[]);

/* silence linter */
long syscall(long number, ...);
int fexecve(int mem_fd, char *const argv[], char *const envp[]);

static inline void set_cloexec(int set_fd)
{
	if (fcntl(set_fd, F_SETFD, FD_CLOEXEC) == -1)
		warn("%s", "error during fnctl");
}

static inline void pipe_fd(int in_fd, int out_fd)
{
	/* read output in a loop */
	for (;;) {
		ssize_t buf_len;
		char buf[COUNT];
		if ((buf_len = read(in_fd, buf, COUNT)) == -1) {
			if (errno == EINTR || errno == EAGAIN) {
				continue;
			} else {
				warn("%s", "error reading from input fd");
				return;
			}
		}
		/* break on EOF */
		if (buf_len == 0)
			break;
		if (write(out_fd, buf, buf_len) == -1) {
			if (errno == EINTR || errno == EAGAIN) {
				continue;
			} else {
				warn("%s", "error writing to output fd");
				return;
			}
		}
	}
}

#endif
