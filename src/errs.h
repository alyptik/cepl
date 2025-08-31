/*
 * errs.h - exception wrappers
 *
 * AUTHOR: Joey Pabalinas <joeypabalinas@gmail.com>
 * See LICENSE file for copyright and license details.
 */

#if !defined(ERRS_H)
#define ERRS_H 1

#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* warning macros */
# define WARN(fmt, ...)													\
	({														\
		fprintf(stderr, "%s:%s [%s:%u] " fmt "\n", strerror(errno), __func__, __FILE__, __LINE__, __VA_ARGS__);	\
	})
# define WARNX(fmt, ... )												\
	({														\
		fprintf(stderr, "%s [%s:%u] " fmt "\n", __func__, __FILE__, __LINE__, __VA_ARGS__);			\
	})
/* error macros */
# define ERR(fmt, ...)													\
	({														\
		fprintf(stderr, "%s:%s [%s:%u] " fmt "\n", strerror(errno), __func__, __FILE__, __LINE__, __VA_ARGS__);	\
		exit(EXIT_FAILURE);											\
	})
# define ERRX(fmt, ...)													\
	({														\
		fprintf(stderr, "%s [%s:%u] " fmt "\n", __func__, __FILE__, __LINE__, __VA_ARGS__);			\
		exit(EXIT_FAILURE);											\
	})

#endif /* !defined(ERRS_H) */
