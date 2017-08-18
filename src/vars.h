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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <linux/memfd.h>
#include "compile.h"

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
	int cnt;
	struct {
		size_t size;
		size_t nmemb;
		char const *key;
		enum var_type type;
		union {
			long long int_val;
			unsigned long long uint_val;
			double dbl_val;
			long double ldbl_val;
			void *ptr_val;
		};
	} *list;
};

/* silence linter */
int regcomp(regex_t *preg, char const *regex, int cflags);
int regexec(regex_t const *preg, char const *string, size_t nmatch, regmatch_t pmatch[], int eflags);
size_t regerror(int errcode, regex_t const *preg, char *errbuf, size_t errbuf_size);
void regfree(regex_t *preg);
void *mmap(void *addr, size_t len, int prot, int flags, int fildes, off_t off);

/* prototypes */
enum var_type extract_type(char const *line, char const *id);
size_t extract_id(char const *line, char **id, size_t *offset);
int find_vars(struct var_list *list, char const *src, char *const cc_args[], char *const exec_args[]);

static inline void init_var_list(struct var_list *list_struct)
{
	if ((list_struct->list = malloc((sizeof *list_struct->list) * ++list_struct->cnt)) == NULL)
		err(EXIT_FAILURE, "%s", "error during initial var_list malloc()");
	list_struct->list = NULL;
}

#endif
