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
size_t extract_id(char const *restrict ln, char **restrict id, size_t *restrict off);
int find_vars(struct program *restrict prog, char const *restrict code);
int print_vars(struct program *restrict prog, char *const *restrict cc_args, char **exec_args);

static inline void init_var_list(struct var_list *restrict var_list)
{
	char init_str[] = "FOOBARTHISVALUEDOESNTMATTERTROLLOLOLOL";
	var_list->cnt = 0;
	var_list->max = 1;
	xcalloc(char, &var_list->list, 1, sizeof *var_list->list, "init_prog->var_list()");
	var_list->cnt++;
	xcalloc(char, &var_list->list[var_list->cnt - 1].id, 1, strlen(init_str) + 1, "init_prog->var_list");
	strmv(0, var_list->list[var_list->cnt - 1].id, init_str);
	var_list->list[var_list->cnt - 1].type_spec = T_ERR;
}

static inline void append_var(struct var_list *restrict var_list, char const *restrict id, enum var_type type_spec)
{
	if (type_spec == T_ERR || !id)
		return;
	/* realloc if cnt reaches current size */
	if (++var_list->cnt >= var_list->max) {
		var_list->max *= 2;
		xrealloc(char, &var_list->list, sizeof *var_list->list * var_list->max, "append_var()");
	}
	xcalloc(char, &var_list->list[var_list->cnt - 1].id, 1, strlen(id) + 1, "append_var()");
	strmv(0, var_list->list[var_list->cnt - 1].id, id);
	var_list->list[var_list->cnt - 1].type_spec = type_spec;
}

static inline void gen_var_list(struct program *restrict prog)
{
	/* sanity checks */
	if (!prog || !prog->id_list.list || !prog->type_list.list)
		ERRX("%s", "NULL pointer passed to gen_var_list()");
	/* nothing to do */
	if (!prog->id_list.cnt || !prog->type_list.cnt)
		return;
	/* don't add duplicate keys to prog->var_list */
	for (size_t i = 0; i < prog->id_list.cnt; i++) {
		bool uniq = true;
		for (ptrdiff_t j = prog->var_list.cnt - 1; j >= 0; j--) {
			if (prog->type_list.list[i] != prog->var_list.list[j].type_spec
					|| strcmp(prog->id_list.list[i], prog->var_list.list[j].id))
				continue;
			/* break early if type or id match */
			uniq = false;
			break;
		}
		/* no matches found */
		if (uniq)
			append_var(&prog->var_list, prog->id_list.list[i], prog->type_list.list[i]);
	}
}

#endif
