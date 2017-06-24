/*
 * compile.h - compile the generated source code
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE file for copyright and license details.
 */

#ifndef COMPILE_H
#define COMPILE_H

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

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

static inline int pipe_fd(int in_fd, int out_fd)
{
	int total = 0;
	/* read output in a loop */
	for (;;) {
		int buf_len;
		size_t count = sysconf(_SC_PAGESIZE);
		char buf[count];
		if ((buf_len = read(in_fd, buf, count)) == -1) {
			if (errno == EINTR || errno == EAGAIN) {
				continue;
			} else {
				warn("%s", "error reading from input fd");
				return 0;
			}
		}
		total += buf_len;
		/* break on EOF */
		if (buf_len == 0)
			break;
		if (write(out_fd, buf, buf_len) == -1) {
			if (errno == EINTR || errno == EAGAIN) {
				continue;
			} else {
				warn("%s", "error writing to output fd");
				return 0;
			}
		}
	}
	return total;
}

#endif
