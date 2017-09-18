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
enum var_type extract_type(char const *line, char const *id);
size_t extract_id(char const *line, char **id, size_t *offset);
int find_vars(char const *line, struct str_list *ilist, enum var_type **tlist);
int print_vars(struct var_list *vlist, char const *src, char *const cc_args[], char *const exec_args[]);

static inline void init_vlist(struct var_list *vlist)
{
	vlist->cnt = 0;
	vlist->max = 1;
	if (!(vlist->list = malloc(sizeof *vlist->list)))
		ERR("error during initial var_list malloc()");
}

static inline void append_var(struct var_list *vlist, char const *key, enum var_type type)
{
	void *tmp;
	if (!vlist || !vlist->list || !key)
		ERRX("invalid arguments passed to append_var()");
	vlist->cnt++;
	/* realloc if cnt reaches current size */
	if (vlist->cnt >= vlist->max) {
		/* check if size too large */
		if (vlist->cnt > MAX)
			ERRX("vlist->cnt > (SIZE_MAX / 2 - 1)");
		/* double until size is reached */
		while ((vlist->max *= 2) < vlist->cnt);
		if (!(tmp = realloc(vlist->list, sizeof *vlist->list * vlist->max))) {
			free(vlist->list);
			ERRARR("var_list", vlist->cnt);
		}
		vlist->list = tmp;
	}
	if (!(vlist->list[vlist->cnt - 1].key = calloc(1, strlen(key) + 1)))
		ERR("append_var()");
	memcpy(vlist->list[vlist->cnt - 1].key, key, strlen(key) + 1);
	vlist->list[vlist->cnt - 1].type = type;
}

static inline void gen_vlist(struct var_list *vlist, struct str_list *ilist, enum var_type **tlist)
{
	/* sanity checks */
	if (!vlist || !vlist->list || !ilist || !ilist->list || !tlist)
		ERRX("NULL pointer passed to gen_var_list()");
	/* don't add duplicate keys to vlist */
	for (size_t i = 0; i < ilist->cnt; i++) {
		bool uniq = true;
		for (ptrdiff_t j = vlist->cnt - 1; j >= 0; j--) {
			if (!strcmp(ilist->list[i], vlist->list[j].key) && (*tlist)[i] == vlist->list[j].type) {
				uniq = false;
				break;
			}
		}
		if (uniq)
			append_var(vlist, ilist->list[i], (*tlist)[i]);
	}
}

#endif
