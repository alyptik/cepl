/*
 * defs.h - data structure and macro definitions
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#ifndef DEFS_H
#define DEFS_H

#include <stdlib.h>

/* use specific message */
#define ERR(X) err(EXIT_FAILURE, "%s %s %d", (X), "at line", __LINE__)
#define ERRX(X) errx(EXIT_FAILURE, "%s %s %d", (X), "at line", __LINE__)
#define WARN(X) warn("%s %s %d", (X), "at line", __LINE__)
#define WARNX(X) warnx("%s %s %d", (X), "at line", __LINE__)

/* general case */
#define ERRGEN(X) err(EXIT_FAILURE, "%s %s %s %d", "error during", (X), "at line", __LINE__)
#define ERRXGEN(X) errx(EXIT_FAILURE, "%s %s %s %d", "error during", (X), "at line", __LINE__)
#define WARNGEN(X) warn("%s %s %s %d", "error during", (X), "at line", __LINE__)
#define WARNXGEN(X) warnx("%s %s %s %d", "error during", (X), "at line", __LINE__)

/* array case */
#define ERRARR(X, Y) err(EXIT_FAILURE, "%s %s[%zu] %s %d", "error allocating", (X), (Y), "at line", __LINE__)
#define ERRXARR(X, Y) err(EXIT_FAILURE, "%s %s[%zu] %s %d", "error allocating", (X), (Y), "at line", __LINE__)
#define WARNARR(X, Y) err("%s %s[%zu] %s %d", "error allocating", (X), (Y), "at line", __LINE__)
#define WARNXARR(X, Y) err("%s %s[%zu] %s %d", "error allocating", (X), (Y), "at line", __LINE__)

/* flag constants for type of source buffer */
enum src_flag {
	EMPTY = 0,
	NOT_IN_MAIN = 1,
	IN_MAIN = 2,
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
	char *funcs;
	char *body;
	char *final;
	struct str_list lines;
	struct str_list hist;
	struct flag_list flags;
};


#endif
