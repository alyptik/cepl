/*
 * errs.h - exception wrappers
 *
 * AUTHOR: Joey Pabalinas <joeypabalinas@gmail.com>
 * See LICENSE.md file for copyright and license details.
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
# define WARN(fmt, ...) \
	do { \
		fprintf(stderr, "%s:%s [%s:%u] " fmt "\n", strerror(errno), __func__, __FILE__, __LINE__, __VA_ARGS__); \
	} while (0)
# define WARNX(fmt, ... ) \
	do { \
		fprintf(stderr, "%s [%s:%u] " fmt "\n", __func__, __FILE__, __LINE__, __VA_ARGS__); \
	} while (0)
/* error macros */
# define ERR(fmt, ...) \
	do { \
		fprintf(stderr, "%s:%s [%s:%u] " fmt "\n", strerror(errno), __func__, __FILE__, __LINE__, __VA_ARGS__); \
		exit(EXIT_FAILURE); \
	} while (0)
# define ERRX(fmt, ...) \
	do { \
		fprintf(stderr, "%s [%s:%u] " fmt "\n", __func__, __FILE__, __LINE__, __VA_ARGS__); \
		exit(EXIT_FAILURE); \
	} while (0)

#endif /* !defined(ERRS_H) */
