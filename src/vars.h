/*
 * vars.h - variable tracking
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#ifndef VARS_H
#define VARS_H 1

#include <err.h>
#include <errno.h>
#include <regex.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

/* silence linter */
int regcomp(regex_t *preg, char const *regex, int cflags);
int regexec(regex_t const *preg, char const *string, size_t nmatch, regmatch_t pmatch[], int eflags);
size_t regerror(int errcode, regex_t const *preg, char *errbuf, size_t errbuf_size);
void regfree(regex_t *preg);

enum var_type {
	T_CHR,
	T_STR,
	T_INT,
	T_UINT,
	T_DBL,
	T_UDBL,
	T_PTR,
	T_OTHER,
};

struct var_list {
	int cnt;
	struct {
		size_t sz;
		size_t nmemb;
		char const *key;
		enum var_type type;
		/* hack to allow flexible array member to be part of a union */
		union {
			long long int_val[1];
			unsigned long long uint_val[1];
			long double flt_val[1];
			void *ptr_val[1];
		};
	} *list;
};

enum var_type extract_type(char const *line, char const *id);
size_t extract_id(char const *line, char **id, size_t *offset);
int append_var(struct var_list *list, enum var_type type, char const *key);
int find_vars(struct var_list *list, char const *src, char *const cc_args[], char *const exec_args[]);

#endif
