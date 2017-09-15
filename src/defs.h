/*
 * defs.h - data structure and macro definitions
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#ifndef DEFS_H
#define DEFS_H

#include <error.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* error macros */
#define ERR(X)		do { \
				fprintf(stderr, "\n%s: \"%s\" line %d\n", strerror(errno), (X), __LINE__); \
				exit(1); \
			} while (0)
#define ERRX(X)		do { \
				fprintf(stderr, "\n\"%s\" line %d\n", (X), __LINE__); \
				exit(1); \
			} while (0)
#define WARN(X)		do { \
				fprintf(stderr, "\n%s: \"%s\" line %d\n", strerror(errno), (X), __LINE__); \
			} while (0)
#define WARNX(X)	do { \
				fprintf(stderr, "\n\"%s\" line %d\n", (X), __LINE__); \
			} while (0)
/* array error macros */
#define ERRARR(X, Y)	do { \
				fprintf(stderr, "\n%s: \"%s[%zu]\" line %d\n", strerror(errno), (X), (Y), __LINE__); \
				exit(1); \
			} while (0)
#define ERRXARR(X, Y)	do { \
				fprintf(stderr, "\n\"%s[%zu]\" line %d\n", (X), (Y), __LINE__); \
				exit(1); \
			} while (0)
#define WARNARR(X, Y)	do { \
				fprintf(stderr, "\n%s: \"%s[%zu]\" line %d\n", strerror(errno), (X), (Y), __LINE__); \
			} while (0)
#define WARNXARR(X, Y)	do { \
				fprintf(stderr, "\n\"%s[%zu]\" line %d\n", (X), (Y), __LINE__); \
			} while (0)

/* flag constants for type of source buffer */
enum src_flag {
	EMPTY = 0,
	NOT_IN_MAIN = 1,
	IN_MAIN = 2,
};
/* possible types of tracked variable */
enum var_type {
	T_ERR = 0,
	T_CHR = 1,
	T_STR = 2,
	T_INT = 3,
	T_UINT = 4,
	T_DBL = 5,
	T_LDBL = 6,
	T_PTR = 7,
	T_OTHER = 8,
};

/* struct definition for NULL-terminated string dynamic array */
struct str_list {
	size_t cnt, max;
	char **list;
};
/* struct definition for flag dynamic array */
struct flag_list {
	size_t cnt, max;
	enum src_flag *list;
};
/* struct definition for var-tracking dynamic array */
struct var_list {
	size_t cnt, max;
	struct {
		char *key;
		enum var_type type;
	} *list;
};
/* struct definition for generated program sources */
struct prog_src {
	char *body, *funcs, *total;
	struct str_list hist, lines;
	struct flag_list flags;
};


#endif
