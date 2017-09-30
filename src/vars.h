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

static inline int var_cmp(void const *a, void const *b)
{
	char *a_key = ((struct {char *key; enum var_type type;} *)a)->key;
	char *b_key = ((struct {char *key; enum var_type type;} *)b)->key;
	enum var_type a_type = ((struct {char *key; enum var_type type;} *)a)->type;
	enum var_type b_type = ((struct {char *key; enum var_type type;} *)b)->type;
	int kres = strcmp(a_key, b_key);
	int tres = (a_type < b_type) - (a_type > b_type);
	/* full match */
	if (!kres && !tres)
		return 0;
	/* give key compare higher weight */
	if (kres)
		return kres;
	/* fallback to type compare */
	return tres;
}

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
	/* initialize list */
	append_var(vlist, ilist->list[0], tlist[0][0]);
	/* don't add duplicate keys to vlist */
	for (size_t i = 1; i < ilist->cnt; i++) {
		struct {char *key; enum var_type type;} vtmp = {ilist->list[i], tlist[0][i]};
		if (!bsearch(&vtmp, &vlist->list, vlist->cnt, sizeof *vlist->list, &var_cmp)) {
			append_var(vlist, ilist->list[i], tlist[0][i]);
			if (vlist->cnt > 1)
				qsort(&vlist->list, vlist->cnt, sizeof *vlist->list, &var_cmp);
		}
	}
}

#endif
