/*
 * compile.h - compile the generated source code
 *
 * AUTHOR: Joey Pabalinas <joeypabalinas@gmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#ifndef COMPILE_H
#define COMPILE_H 1

#include "defs.h"
#include "errs.h"
#include <fcntl.h>

/* prototypes */
int compile(char const *restrict src, char *const cc_args[], char *const exec_args[], bool show_errors);

static inline void set_cloexec(int set_fd[static 2])
{
	if (fcntl(set_fd[0], F_SETFD, FD_CLOEXEC) == -1)
		WARN("%s", "fnctl()");
	if (fcntl(set_fd[1], F_SETFD, FD_CLOEXEC) == -1)
		WARN("%s", "fnctl()");
}

static inline void pipe_fd(int in_fd, int out_fd)
{
	/* fallback buffer (not reentrant) */
	static char failover[PAGE_SIZE];
	char *primary = malloc(PAGE_SIZE);
	char *buf = primary;
	if (!buf)
		buf = failover;
	/* pipe data in a loop */
	for (;;) {
		ptrdiff_t ret;
		if ((ret = read(in_fd, buf, PAGE_SIZE)) < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			WARN("%s", "error reading from input fd");
			break;
		}
		/* break on EOF */
		if (!ret)
			break;
		if (write(out_fd, buf, ret) < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			WARN("%s", "error writing to output fd");
			break;
		}
	}
	free(primary);
}

#endif
