/*
 * compile.h - compile the generated source code
 *
 * AUTHOR: Joey Pabalinas <joeypabalinas@gmail.com>
 * See LICENSE file for copyright and license details.
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

#endif /* !defined(COMPILE_H) */
