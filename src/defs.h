/*
 * defs.h - data structure definitions
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#ifndef DEFS_H
#define DEFS_H

#include <stdlib.h>

/* flag constants for type of source buffer */
enum src_flag {
	EMPTY = 0,
	NOT_IN_MAIN = 1,
	IN_MAIN = 2,
};

/* struct definition for NULL terminated string array */
struct str_list {
	size_t cnt;
	char **list;
};

/* struct definition for flag array */
struct flag_list {
	size_t cnt;
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

struct var_list {
	size_t cnt;
	struct {
		char *key;
		enum var_type type;
	} *list;
};


#endif
