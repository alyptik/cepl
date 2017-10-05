/*
 * errs.h - exception wrappers
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#ifndef ERRS_H
#define ERRS_H

#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


/* warning macros */
#define WARN(X)		fprintf(stderr, "%s: \"%s\" %s:%u\n", strerror(errno), (X), __FILE__, __LINE__)
#define WARNX(X)	fprintf(stderr, "\"%s\" %s:%u\n", (X), __FILE__, __LINE__)
#define WARNARR(X, Y)	fprintf(stderr, "%s: \"%s[%zu]\" %s:%u\n", strerror(errno), (X), (Y), __FILE__, __LINE__)
#define WARNXARR(X, Y)	fprintf(stderr, "\"%s[%zu]\" %s:%u\n", (X), (Y), __FILE__, __LINE__)

/* error macros */
#define ERR(X)		do { \
				fprintf(stderr, "%s: \"%s\" %s:%u\n", \
						strerror(errno), (X), __FILE__, __LINE__); \
				exit(EXIT_FAILURE); \
			} while (0)
#define ERRX(X)		do { \
				fprintf(stderr, "\"%s\" %s:%u\n", \
						(X), __FILE__, __LINE__); \
				exit(EXIT_FAILURE); \
			} while (0)
#define ERRARR(X, Y)	do { \
				fprintf(stderr, "%s: \"%s[%zu]\" %s:%u\n", \
						strerror(errno), (X), (Y), __FILE__, __LINE__); \
				exit(EXIT_FAILURE); \
			} while (0)
#define ERRXARR(X, Y)	do { \
				fprintf(stderr, "\"%s[%zu]\" %s:%u\n", \
						(X), (Y), __FILE__, __LINE__); \
				exit(EXIT_FAILURE); \
			} while (0)

#endif
