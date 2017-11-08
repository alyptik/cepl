/*
 * vars.h - variable tracking
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#ifndef VARS_H
#define VARS_H 1

#include "compile.h"
#include "parseopts.h"
#include <linux/memfd.h>
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

/* prototypes */
enum var_type extract_type(char const *restrict ln, char const *restrict id);
size_t extract_id(char const *restrict ln, char **restrict id, size_t *restrict offset);
int find_vars(char const *restrict ln, STR_LIST *restrict ilist, TYPE_LIST *restrict tlist);
int print_vars(VAR_LIST *restrict vlist, char const *restrict src, char *const cc_args[], char *const exec_args[]);

static inline void init_vlist(VAR_LIST *restrict vlist)
{
	char init_str[] = "FOOBARTHISVALUEDOESNTMATTERTROLLOLOLOL";
	vlist->cnt = 0;
	vlist->max = 1;
	if (!(vlist->list = calloc(1, sizeof *vlist->list)))
		ERR("error during initial list_ptr calloc()");
	vlist->cnt++;
	if (!(vlist->list[vlist->cnt - 1].key = calloc(1, strlen(init_str) + 1)))
		ERR("error during initial list_ptr[0] calloc()");
	strmv(0, vlist->list[vlist->cnt - 1].key, init_str);
	vlist->list[vlist->cnt - 1].type = T_ERR;
}

static inline void append_var(VAR_LIST *restrict vlist, char const *restrict key, enum var_type type)
{
	if (type == T_ERR || !key)
		return;
	void *tmp;
	vlist->cnt++;
	assert(vlist->max != 0);
	/* realloc if cnt reaches current size */
	if (vlist->cnt >= vlist->max) {
		/* check if size too large */
		if (vlist->cnt > ARRAY_MAX)
			ERRX("vlist->cnt > (SIZE_MAX / 2 - 1)");
		vlist->max *= 2;
		if (!(tmp = realloc(vlist->list, sizeof *vlist->list * vlist->max)))
			ERRARR("type_list", vlist->cnt);
		vlist->list = tmp;
	}
	if (!(vlist->list[vlist->cnt - 1].key = calloc(1, strlen(key) + 1)))
		ERRARR("list_ptr", vlist->cnt - 1);
	strmv(0, vlist->list[vlist->cnt - 1].key, key);
	vlist->list[vlist->cnt - 1].type = type;
}

static inline void gen_vlist(VAR_LIST *restrict vlist, STR_LIST *restrict ilist, TYPE_LIST *restrict tlist)
{
	/* sanity checks */
	if (!vlist || !ilist || !ilist->list || !tlist || !tlist->list)
		ERRX("NULL pointer passed to gen_var_list()");
	/* nothing to do */
	if (!ilist->cnt || !tlist->cnt)
		return;
	assert(ilist->cnt >= tlist->cnt);
	/* don't add duplicate keys to vlist */
	for (size_t i = 0; i < ilist->cnt; i++) {
		bool uniq = true;
		for (ptrdiff_t j = vlist->cnt - 1; j >= 0; j--) {
			if (tlist->list[i] != vlist->list[j].type || strcmp(ilist->list[i], vlist->list[j].key))
				continue;
			/* break early if type or id match */
			uniq = false;
			break;
		}
		/* no matches found */
		if (uniq)
			append_var(vlist, ilist->list[i], tlist->list[i]);
	}
}

#endif
