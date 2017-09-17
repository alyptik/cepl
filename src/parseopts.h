/*
 * parseopts.h - option parsing
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#ifndef PARSEOPTS_H
#define PARSEOPTS_H

#include "defs.h"
#include "errs.h"
#include <fcntl.h>
#include <gelf.h>
#include <libelf.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/* prototypes */
char **parse_opts(int argc, char *argv[], char const optstring[], FILE volatile **ofile);
void read_syms(struct str_list *tokens, char const *elf_file);
void parse_libs(struct str_list *symbols, char *libs[]);

/* recursive free */
static inline ptrdiff_t free_argv(char ***argv)
{
	size_t cnt;
	if (!argv || !*argv || !(*argv)[0])
		return -1;
	for (cnt = 0; (*argv)[cnt]; cnt++)
		free((*argv)[cnt]);
	free(*argv);
	*argv = NULL;
	return cnt;
}

/* emulate `strcat()` if `off < 0`, else copy `src` to `dest` at offset `off` */
static inline void strmv(ptrdiff_t off, char *restrict dest, char const *restrict src) {
	/* sanity checks */
	if (!dest || !src)
		ERRX("NULL pointer passed to strmv()");
	ptrdiff_t src_sz;
	char *src_ptr = strchr(src, 0);
	char *dest_ptr = strchr(dest, 0);
	if (!src_ptr || !dest_ptr)
		ERRX("strmv() string not null-terminated");
	if (off >= 0)
		dest_ptr = dest + off;
	src_sz = src_ptr - src;
	memcpy(dest_ptr, src, (size_t)src_sz + 1);
}

static inline ptrdiff_t free_str_list(struct str_list *plist)
{
	size_t null_cnt = 0;
	/* return -1 if passed NULL pointers */
	if (!plist || !plist->list)
		return -1;
	for (size_t i = 0; i < plist->cnt; i++) {
		/* if NULL increment counter and skip */
		if (!plist->list[i]) {
			null_cnt++;
			continue;
		}
		free(plist->list[i]);
		plist->list[i] = NULL;
	}
	free(plist->list);
	plist->list = NULL;
	plist->cnt = 0;
	plist->max = 1;
	return null_cnt;
}

static inline void init_list(struct str_list *list_struct, char *init_str)
{
	list_struct->cnt = 0;
	list_struct->max = 1;
	if (!(list_struct->list = calloc(1, sizeof *list_struct->list)))
		ERR("error during initial list_ptr calloc()");
	/* exit early if NULL */
	if (!init_str)
		return;
	list_struct->cnt++;
	if (!(list_struct->list[list_struct->cnt - 1] = calloc(1, strlen(init_str) + 1)))
		ERR("error during initial list_ptr[0] calloc()");
	memcpy(list_struct->list[list_struct->cnt - 1], init_str, strlen(init_str) + 1);
}

static inline void append_str(struct str_list *list_struct, char *str, size_t padding)
{
	char **tmp;
	list_struct->cnt++;
	/* realloc if cnt reaches current size */
	if (list_struct->cnt >= list_struct->max) {
		/* check if size too large */
		if (list_struct->cnt > MAX)
			ERRX("list_struct->cnt > (SIZE_MAX / 2 - 1)");
		/* double until size is reached */
		while ((list_struct->max *= 2) < list_struct->cnt);
		if (!(tmp = realloc(list_struct->list, sizeof *list_struct->list * list_struct->max)))
			ERRARR("list_ptr", list_struct->cnt - 1);
		list_struct->list = tmp;
	}
	if (!str) {
		list_struct->list[list_struct->cnt - 1] = NULL;
	} else {
		if (!(list_struct->list[list_struct->cnt - 1] = calloc(1, strlen(str) + padding + 1)))
			ERRARR("list_ptr", list_struct->cnt - 1);
		memcpy(list_struct->list[list_struct->cnt - 1] + padding, str, strlen(str) + 1);
	}
}

static inline void init_flag_list(struct flag_list *list_struct)
{
	list_struct->cnt = 0;
	list_struct->max = 1;
	if (!(list_struct->list = calloc(1, sizeof *list_struct->list)))
		ERR("error during initial flag_list calloc()");
	list_struct->cnt++;
	list_struct->list[list_struct->cnt - 1] = EMPTY;
}

static inline void append_flag(struct flag_list *list_struct, enum src_flag flag)
{
	enum src_flag *tmp;
	list_struct->cnt++;
	/* realloc if cnt reaches current size */
	if (list_struct->cnt >= list_struct->max) {
		/* check if size too large */
		if (list_struct->cnt > MAX)
			ERRX("list_struct->cnt > (SIZE_MAX / 2 - 1)");
		/* double until size is reached */
		while ((list_struct->max *= 2) < list_struct->cnt);
		if (!(tmp = realloc(list_struct->list, sizeof *list_struct->list * list_struct->max)))
			ERRARR("flag_list", list_struct->cnt);
		list_struct->list = tmp;
	}
	list_struct->list[list_struct->cnt - 1] = flag;
}

#endif
