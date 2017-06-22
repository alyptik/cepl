/*
 * compile.h - compile the generated source code
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE file for copyright and license details.
 */

#ifndef _COMPILE_H
#define _COMPILE_H

#include <err.h>
#include <errno.h>
#include <fcntl.h>

#define UNUSED __attribute__ ((unused))
extern char **environ;

int compile(char const *cc, char *src, char *const ccargs[], char *const execargs[]);

/* silence linter */
long syscall(long number, ...);
int fexecve(int fd, char *const argv[], char *const envp[]);

static inline void set_cloexec(int setfd)
{
	if (fcntl(setfd, F_SETFD, FD_CLOEXEC) == -1)
		warn("%s", "error during fnctl");
}

static inline int pipe_fd(int in_fd, int out_fd)
{
	int total = 0;
	/* read output in a loop */
	for (;;) {
		int buflen;
		/* 2 MB */
		int count = 1024 * 1024 * 2;
		char buf[count];
		if ((buflen = read(in_fd, buf, count)) == -1) {
			if (errno == EINTR || errno == EAGAIN) {
				continue;
			} else {
				warn("%s", "error reading input fd");
				return 0;
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
				warn("%s", "error writing to output fd");
				return 0;
			}
		}
	}
	return total;
}
#endif
