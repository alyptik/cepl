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
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>

/* prototypes */
enum var_type extract_type(char const *restrict ln, char const *restrict id);
size_t extract_id(char const *restrict ln,
		char **restrict id,
		size_t *restrict offset);
int find_vars(struct program *restrict prog, char const *restrict code);
int print_vars(struct program *restrict prog, char *const *cc_args, char *const *exec_args);

static inline void init_var_list(struct var_list *restrict vlist)
{
	char init_str[] = "FOOBARTHISVALUEDOESNTMATTERTROLLOLOLOL";
	vlist->cnt = 0;
	vlist->max = 1;
	xcalloc(char, &vlist->list, 1, sizeof *vlist->list, "init_vlist()");
	vlist->cnt++;
	xcalloc(char, &vlist->list[vlist->cnt - 1].id, 1, strlen(init_str) + 1, "init_vlist");
	strmv(0, vlist->list[vlist->cnt - 1].id, init_str);
	vlist->list[vlist->cnt - 1].type_spec = T_ERR;
}

static inline void append_var(struct var_list *restrict vlist,
		 char const *restrict id,
		 enum var_type type_spec)
{
	if (type_spec == T_ERR || !id)
		return;
	/* realloc if cnt reaches current size */
	if (++vlist->cnt >= vlist->max) {
		vlist->max *= 2;
		xrealloc(char, &vlist->list, sizeof *vlist->list * vlist->max, "append_var()");
	}
	xcalloc(char, &vlist->list[vlist->cnt - 1].id, 1, strlen(id) + 1, "append_var()");
	strmv(0, vlist->list[vlist->cnt - 1].id, id);
	vlist->list[vlist->cnt - 1].type_spec = type_spec;
}

static inline void gen_var_list(struct program *restrict prog)
{
	/* sanity checks */
	if (!vlist || !ilist || !ilist->list || !tlist || !tlist->list)
		ERRX("%s", "NULL pointer passed to gen_var_list()");
	/* nothing to do */
	if (!ilist->cnt || !tlist->cnt)
		return;
	/* don't add duplicate keys to vlist */
	for (size_t i = 0; i < ilist->cnt; i++) {
		bool uniq = true;
		for (ptrdiff_t j = vlist->cnt - 1; j >= 0; j--) {
			if (tlist->list[i] != vlist->list[j].type_spec || strcmp(ilist->list[i], vlist->list[j].id))
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
