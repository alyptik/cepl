/*
 * defs.h - data structure and macro definitions
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#ifndef DEFS_H
#define DEFS_H

#include "errs.h"
#include <limits.h>
#include <stdint.h>
#include <unistd.h>

/* global version and usage strings */
#define VERSION_STRING	"CEPL v4.5.3"
#define USAGE_STRING	"[-hptvw] [-c<compiler>] [-l<library>] [-I<include dir>] [-o<output.c>]\n\n\t" \
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
/* color escapes */
#define	RED	"\\033[31m"
#define	GREEN	"\\033[32m"
#define	YELLOW	"\\033[33m"
#define	RST	"\\033[00m"
/* page size for buffer count */
#define COUNT		sysconf(_SC_PAGESIZE)
/* `malloc()` size ceiling */
#define MAX		(SIZE_MAX / 2 - 1)
/* `strmv() `concat constant */
#define CONCAT		(-1)

/* source file includes template */
static char const prelude[] = "#define _BSD_SOURCE\n"
	"#define _DEFAULT_SOURCE\n"
	"#define _GNU_SOURCE\n"
	"#define _POSIX_C_SOURCE 200809L\n"
	"#define _SVID_SOURCE\n"
	"#define _XOPEN_SOURCE 700\n\n"
	"#include <assert.h>\n"
	"#include <ctype.h>\n"
	"#include <error.h>\n"
	"#include <errno.h>\n"
	"#include <fcntl.h>\n"
	"#include <limits.h>\n"
	"#include <math.h>\n"
	"#include <regex.h>\n"
	"#include <signal.h>\n"
	"#include <stdalign.h>\n"
	"#include <stdarg.h>\n"
	"#include <stdbool.h>\n"
	"#include <stddef.h>\n"
	"#include <stdint.h>\n"
	"#include <stdio.h>\n"
	"#include <stdlib.h>\n"
	"#include <stdnoreturn.h>\n"
	"#include <string.h>\n"
	"#include <strings.h>\n"
	"#include <time.h>\n"
	"#include <uchar.h>\n"
	"#include <wchar.h>\n"
	"#include <unistd.h>\n"
	"#include <linux/memfd.h>\n"
	"#include <sys/mman.h>\n"
	"#include <sys/types.h>\n"
	"#include <sys/syscall.h>\n"
	"#include <sys/wait.h>\n\n"
	"extern char **environ;\n"
	"#line 1\n";
/* compiler pre-program */
static char const prog_start[] = "\n\nint main(int argc, char *argv[]) "
	"{(void)argc; (void)argv;\n"
	"\n";
/* pre-program shown to user */
static char const prog_start_user[] = "\n\nint main(int argc, char *argv[])\n"
	"{\n";
static char const prog_end[] = "\n\treturn 0;\n}\n";

/* enumerations */
enum src_flag {
	NOT_IN_MAIN, IN_MAIN, EMPTY,
};
/* possible types of tracked variable */
enum var_type {
	T_ERR, T_CHR, T_STR,
	T_INT, T_UINT, T_DBL,
	T_PTR, T_OTHER,
};

/* struct definition for NULL-terminated string dynamic array */
struct str_list {
	size_t cnt, max;
	char **list;
};
/* struct definition for flag dynamic array */
struct flag_list {
	size_t cnt, max;
	enum src_flag *list;
};
/* struct definition for var-tracking dynamic array */
struct var_list {
	size_t cnt, max;
	struct {
		char *key;
		enum var_type type;
	} *list;
};
/* struct definition for generated program sources */
struct prog_src {
	size_t b_sz, f_sz, t_sz;
	size_t b_max, f_max, t_max;
	char *b, *f, *total;
	struct str_list hist, lines;
	struct flag_list flags;
};

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
	char *dest_ptr = NULL, *src_ptr = memchr(src, '\0', COUNT);
	if (off >= 0) {
		dest_ptr = dest + off;
	} else {
		dest_ptr = memchr(dest, '\0', COUNT);
	}
	if (!src_ptr || !dest_ptr)
		ERRX("strmv() string not null-terminated");
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
