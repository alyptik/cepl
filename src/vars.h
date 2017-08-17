/*
 * vars.h - variable tracking
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#ifndef VARS_H
#define VARS_H 1

#include <regex.h>
#include <sys/types.h>

/* silence linter */
int regcomp(regex_t *preg, char const *regex, int cflags);
int regexec(regex_t const *preg, char const *string, size_t nmatch, regmatch_t pmatch[], int eflags);
size_t regerror(int errcode, regex_t const *preg, char *errbuf, size_t errbuf_size);
void regfree(regex_t *preg);

enum var_type {
	T_CHAR,
	T_STR,
	T_INT,
	T_DBL,
	T_PTR,
	T_ARR,
	T_STRUCT,
};

/*
 * struct var_meta {
 *         enum var_type type;
 *         char const *key;
 *         void *val;
 *         union {
 *                 long long int_val;
 *                 long double flt_val;
 *                 void *ptr_val;
 *         };
 * };
 */

struct var_list {
	int cnt;
	struct {
		enum var_type type;
		char const *key;
		union {
			long long int_val;
			long double flt_val;
			void *ptr_val;
		};
	} *list;
};

int print_vars(char *const src, char *const cc_args[], char *const exec_args[]);

#endif
