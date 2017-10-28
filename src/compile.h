/*
 * compile.h - compile the generated source code
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#ifndef COMPILE_H
#define COMPILE_H 1

#include "defs.h"
#include "errs.h"
#include <fcntl.h>

/* prototypes */
int compile(char const *restrict src, char *const cc_args[], char *const exec_args[]);

static inline void set_cloexec(int set_fd[static 2])
{
	if (fcntl(set_fd[0], F_SETFD, FD_CLOEXEC) == -1)
		WARN("fnctl()");
	if (fcntl(set_fd[1], F_SETFD, FD_CLOEXEC) == -1)
		WARN("fnctl()");
}

static inline void pipe_fd(int in_fd, int out_fd)
{
	/* pipe data in a loop */
	for (;;) {
		ptrdiff_t buf_len;
		char buf[COUNT];
		if ((buf_len = read(in_fd, buf, COUNT)) == -1) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			WARN("error reading from input fd");
			break;
		}
		/* break on EOF */
		if (buf_len == 0)
			break;
		if (write(out_fd, buf, buf_len) == -1) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			WARN("error writing to output fd");
			break;
		}
	}
}

#endif
