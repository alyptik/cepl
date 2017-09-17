/*
 * parseopts.h - option parsing
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#ifndef PARSEOPTS_H
#define PARSEOPTS_H

#include <fcntl.h>
#include <gelf.h>
#include <libelf.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "defs.h"

/* global version and usage strings */
#define VERSION_STRING "CEPL v4.2.0"
#define USAGE_STRING "[-hptvw] [-c<compiler>] [-l<library>] [-I<include dir>] [-o<output.c>]\n\n\t" \
	"-h,--help:\t\tShow help/usage information.\n\t" \
	"-p,--parse:\t\tDisable addition of dynamic library symbols to readline completion.\n\t" \
	"-t,--tracking:\t\tToggle variable tracking.\n\t" \
	"-v,--version:\t\tShow version information.\n\t" \
	"-w,--warnings:\t\tCompile with ”-pedantic-errors -Wall -Wextra” flags.\n\t" \
	"-c,--compiler:\t\tSpecify alternate compiler.\n\t" \
	"-l:\t\t\tLink against specified library (flag can be repeated).\n\t" \
	"-I:\t\t\tSearch directory for header files (flag can be repeated).\n\t" \
	"-o:\t\t\tName of the file to output source to.\n\n" \
	"Input lines prefixed with a “;” are used to control internal state.\n\n\t" \
	";f[unction]:\t\tDefine a function (e.g. “;f void foo(void) { … }”)\n\t" \
	";h[elp]:\t\tShow help\n\t" \
	";i[nclude]:\t\tDefine an include (e.g. “;i #include <crypt.h>”)\n\t" \
	";m[acro]:\t\tDefine a macro (e.g. “;m #define ZERO(x) (x ^ x)”)\n\t" \
	";o[utput]:\t\tToggle -o (output file) flag\n\t" \
	";p[arse]:\t\tToggle -p (shared library parsing) flag\n\t" \
	";q[uit]:\t\tExit CEPL\n\t" \
	";r[eset]:\t\tReset CEPL to its initial program state\n\t" \
	";t[racking]:\t\tToggle variable tracking.\n\t" \
	";u[ndo]:\t\tIncremental pop_history (can be repeated)\n\t" \
	";w[arnings]:\t\tToggle -w (warnings) flag"
/* set pipe buffer byte count to page size */
#define COUNT sysconf(_SC_PAGESIZE)

/* silence linter */
int getopt(int argc, char *const argv[], char const *optstring);
FILE *fdopen(int fd, char const *mode);
ssize_t getline(char **lineptr, size_t *n, FILE *stream);

/* prototypes */
char **parse_opts(int argc, char *argv[], char const optstring[], FILE volatile **ofile);
void read_syms(struct str_list *tokens, char const *elf_file);
void parse_libs(struct str_list *symbols, char *libs[]);

static inline size_t free_argv(char **argv)
{
	size_t count;
	if (!argv || !(argv[0]))
		return -1;
	for (count = 0; argv[count]; count++)
		free(argv[count]);
	free(argv);
	return count;
}

static inline ptrdiff_t free_str_list(struct str_list *plist)
{
	ptrdiff_t null_cnt = 0;
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
		/* double until size is reached */
		while ((list_struct->max *= 2) < list_struct->cnt);
		if (!(tmp = realloc(list_struct->list, sizeof *list_struct->list * list_struct->max)))
			ERRARR("flag_list", list_struct->cnt);
		list_struct->list = tmp;
	}
	list_struct->list[list_struct->cnt - 1] = flag;
}

#endif
