/*
 * compile.h - compile the generated source code
 *
 * AUTHOR: Joey Pabalinas <joeypabalinas@gmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#if !defined(COMPILE_H)
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
	/* splice data in a loop */
	for (;;) {
		ssize_t ret;
		if ((ret = splice(in_fd, NULL, out_fd, NULL, PAGE_SIZE, SPLICE_F_MOVE)) < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			WARN("%s", "error reading from input fd");
			break;
		}
		/* break on EOF */
		if (!ret)
			break;
	}
}

#endif /* !defined(COMPILE_H) */
