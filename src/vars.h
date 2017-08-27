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
#include <sys/wait.h>
#include <linux/memfd.h>
#include "compile.h"
#include "parseopts.h"

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
		char *key;
		enum var_type type;
		void *data;
		union {
			long long int_val;
			unsigned long long uint_val;
			double dbl_val;
			long double ldbl_val;
			void *ptr_val;
		};
	} *list;
};

/* prototypes */
enum var_type extract_type(char const *line, char const *id);
size_t extract_id(char const *line, char **id, size_t *offset);
int find_vars(char const *line, struct str_list *id_list, enum var_type **type_list);
int print_vars(char const *src, char *const cc_args[], char *const exec_args[], struct var_list *list);

static inline void init_var_list(struct var_list *list_struct)
{
	if ((list_struct->list = malloc(sizeof *list_struct->list)) == NULL)
		ERR("error during initial var_list malloc()");
	list_struct->cnt = 0;
}

static inline void append_var(struct var_list *list_struct, char const *key, enum var_type type)
{
	void *tmp;
	/* if (!list_struct || size < 1 || nmemb < 1 || !key || !val) */
	if (!list_struct || !key)
		ERRX("invalid arguments passed to append_var()");
	if ((tmp = realloc(list_struct->list, (sizeof *list_struct->list) * ++list_struct->cnt)) == NULL)
		ERRARR("var_list", list_struct->cnt);
	list_struct->list = tmp;
	if ((list_struct->list[list_struct->cnt - 1].key = malloc(strlen(key) + 1)) == NULL)
		ERRGEN("append_var()");
	memset(list_struct->list[list_struct->cnt - 1].key, 0, strlen(key) + 1);
	memcpy(list_struct->list[list_struct->cnt - 1].key, key, strlen(key) + 1);
	list_struct->list[list_struct->cnt - 1].type = type;
	list_struct->list[list_struct->cnt - 1].data = NULL;

	/* switch (type) { */
	/* case T_CHR: */
		/* fallthrough */
	/* case T_INT: */
	/*         list_struct->list[list_struct->cnt - 1].int_val = *((long long *)*val); */
	/*         break; */
	/* case T_UINT: */
	/*         list_struct->list[list_struct->cnt - 1].uint_val = *((unsigned long long *)*val); */
	/*         break; */
	/* case T_DBL: */
	/*         list_struct->list[list_struct->cnt - 1].dbl_val = *((double *)*val); */
	/*         break; */
	/* case T_LDBL: */
	/*         list_struct->list[list_struct->cnt - 1].ldbl_val = *((long double *)*val); */
	/*         break; */
	/* case T_STR: */
		/* fallthrough */
	/* case T_PTR: */
	/*         list_struct->list[list_struct->cnt - 1].ptr_val = (void *)*val; */
	/*         break; */
	/* case T_OTHER: */
		/* fallthrough */
	/* for the default case only track the address of the object */
	/* default: */
	/*         list_struct->list[list_struct->cnt - 1].ptr_val = val; */
	/* } */
}

static inline void gen_var_list(struct var_list *list_struct, struct str_list *id_list, enum var_type **type_list)
{
	/* sanity checks */
	if (!list_struct || !list_struct->list || !id_list || !id_list->list || !type_list)
		WARNX("NULL pointer passed to gen_var_list()");
	for (ssize_t i = 0; i < id_list->cnt; i++)
		append_var(list_struct, id_list->list[i], *type_list[i]);
}

#endif
