/*
 * errs.h - exception wrappers
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#ifndef ERRS_H
#define ERRS_H

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* error macros */
#define ERR(X)		do { \
	fprintf(stderr, "\n%s: \"%s\" on line %d\n", strerror(errno), (X), __LINE__); \
	exit(EXIT_FAILURE); \
			} while (0)
#define ERRX(X)		do { \
	fprintf(stderr, "\n\"%s\" on line %d\n", (X), __LINE__); \
	exit(EXIT_FAILURE); \
			} while (0)
#define ERRARR(X, Y)	do { \
	fprintf(stderr, "\n%s: \"%s[%zu]\" on line %d\n", strerror(errno), (X), (Y), __LINE__); \
	exit(EXIT_FAILURE); \
			} while (0)
#define ERRXARR(X, Y)	do { \
	fprintf(stderr, "\n\"%s[%zu]\" on line %d\n", (X), (Y), __LINE__); \
	exit(EXIT_FAILURE); \
			} while (0)

/* warning macros */
#define WARN(X)		do { \
	fprintf(stderr, "\n%s: \"%s\" on line %d\n", strerror(errno), (X), __LINE__); \
			} while (0)
#define WARNX(X)	do { \
	fprintf(stderr, "\n\"%s\" on line %d\n", (X), __LINE__); \
			} while (0)
#define WARNARR(X, Y)	do { \
	fprintf(stderr, "\n%s: \"%s[%zu]\" on line %d\n", strerror(errno), (X), (Y), __LINE__); \
			} while (0)
#define WARNXARR(X, Y)	do { \
	fprintf(stderr, "\n\"%s[%zu]\" on line %d\n", (X), (Y), __LINE__); \
		} while (0)

#endif
