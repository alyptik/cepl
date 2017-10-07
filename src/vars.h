/*
 * vars.h - variable tracking
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#ifndef VARS_H
#define VARS_H

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
enum var_type extract_type(char const *ln, char const *id);
size_t extract_id(char const *ln, char **id, size_t *offset);
int find_vars(char const *ln, struct str_list *ilist, struct type_list *tlist);
int print_vars(struct var_list *vlist, char const *src, char *const cc_args[], char *const exec_args[]);

static inline void init_vlist(struct var_list *restrict vlist)
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

static inline void append_var(struct var_list *restrict vlist, char const *restrict key, enum var_type type)
{
	if (type == T_ERR || !key)
		return;
	void *tmp;
	vlist->cnt++;
	assert(vlist->max != 0);
	/* realloc if cnt reaches current size */
	if (vlist->cnt >= vlist->max) {
		/* check if size too large */
		if (vlist->cnt > MAX)
			ERRX("vlist->cnt > (SIZE_MAX / 2 - 1)");
		/* double until size is reached */
		while ((vlist->max *= 2) < vlist->cnt);
		if (!(tmp = realloc(vlist->list, sizeof *vlist->list * vlist->max)))
			ERRARR("type_list", vlist->cnt);
		vlist->list = tmp;
	}
	if (!(vlist->list[vlist->cnt - 1].key = calloc(1, strlen(key) + 1)))
		ERRARR("list_ptr", vlist->cnt - 1);
	strmv(0, vlist->list[vlist->cnt - 1].key, key);
	vlist->list[vlist->cnt - 1].type = type;
}

static inline void gen_vlist(struct var_list *restrict vl, struct str_list *restrict il, struct type_list *restrict tl)
{
	/* sanity checks */
	if (!vl || !il || !il->list || !tl || !tl->list)
		ERRX("NULL pointer passed to gen_var_list()");
	/* nothing to do */
	if (!il->cnt || !tl->cnt)
		return;
	assert(il->cnt >= tl->cnt);
	/* don't add duplicate keys to vl */
	for (size_t i = 0; i < il->cnt; i++) {
		bool uniq = true;
		for (ptrdiff_t j = vl->cnt - 1; j >= 0; j--) {
			if (tl->list[i] != vl->list[j].type || strcmp(il->list[i], vl->list[j].key))
				continue;
			/* break early if type or id match */
			uniq = false;
			break;
		}
		/* no matches found */
		if (uniq)
			append_var(vl, il->list[i], tl->list[i]);
	}
}

#endif
