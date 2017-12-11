/*
 * defs.h - data structure and macro definitions
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#ifndef DEFS_H
#define DEFS_H 1

#include "errs.h"
#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

/* macros */
#define DEFAULT(ARG, ALT)	((ARG) ? (ARG) : (ALT))
#define ARRLEN(ARR)		((sizeof (ARR)) / (sizeof (ARR)[0]))
#define HPRINT(VAL)		printf("[%#x] ", (VAL))

/* global version and usage strings */
#define VERSION_STRING	"CEPL v5.0.5"
#define USAGE_STRING	"[-hptvw] [(-a|-i)“<asm.s>”] [-c“<compiler>”] [-e“<code>”] " \
	"[-l“<libs>”] [-I“<includes>”] [-o“<out.c>”]\n\t" \
	"-a,--att:\t\tName of the file to output AT&T-dialect assembler code to\n\t" \
	"-c,--cc:\t\tSpecify alternate compiler\n\t" \
	"-e,--eval:\t\tEvaluate the following argument as C code\n\t" \
	"-h,--help:\t\tShow help/usage information\n\t" \
	"-i,--intel:\t\tName of the file to output Intel-dialect assembler code to\n\t" \
	"-o,--output:\t\tName of the file to output C source code to\n\t" \
	"-p,--parse:\t\tDisable addition of dynamic library symbols to readline completion\n\t" \
	"-t,--tracking:\t\tToggle variable tracking\n\t" \
	"-v,--version:\t\tShow version information\n\t" \
	"-w,--warnings:\t\tCompile with ”-pedantic -Wall -Wextra” flags\n\t" \
	"-l:\t\t\tLink against specified library (flag can be repeated)\n\t" \
	"-I:\t\t\tSearch directory for header files (flag can be repeated)\n" \
	"Input lines prefixed with a “;” are used to control internal state\n\t" \
	";a[tt]:\t\t\tToggle -a (output AT&T-dialect assembler code) flag\n\t" \
	";f[unction]:\t\tDefine a function (e.g. “;f void bork(void) { puts(\"wark\"); }”)\n\t" \
	";h[elp]:\t\tShow help\n\t" \
	";i[ntel]:\t\tToggle -a (output Intel-dialect assembler code) flag\n\t" \
	";m[acro]:\t\tDefine a macro (e.g. “;m #define SWAP2(X) ((((X) >> 8) & 0xff) | (((X) & 0xff) << 8))”)\n\t" \
	";o[utput]:\t\tToggle -o (output C source code) flag\n\t" \
	";p[arse]:\t\tToggle -p (shared library parsing) flag\n\t" \
	";q[uit]:\t\tExit CEPL\n\t" \
	";r[eset]:\t\tReset CEPL to its initial program state\n\t" \
	";t[racking]:\t\tToggle variable tracking\n\t" \
	";u[ndo]:\t\tIncremental pop_history (can be repeated)\n\t" \
	";w[arnings]:\t\tToggle -w (pedantic warnings) flag"
#define	RED		"\\033[31m"
#define	GREEN		"\\033[32m"
#define	YELLOW		"\\033[33m"
#define	RST		"\\033[00m"
/* page size for buffer count */
#define PAGE_SIZE	sysconf(_SC_PAGESIZE)
/* max possible types */
#define TNUM		7
/* max eval string length */
#define EVAL_LIMIT	4096
/* `strmv() `concat constant */
#define CONCAT		(-1)
/* `malloc()` size ceiling */
#define ARRAY_MAX	(SIZE_MAX / 2 - 1)

/* source file includes template */
static char const prelude[] =
	"#ifndef _BSD_SOURCE\n"
	"#  define _BSD_SOURCE\n"
	"#endif\n"
	"#ifndef _DEFAULT_SOURCE\n"
	"#  define _DEFAULT_SOURCE\n"
	"#endif\n"
	"#ifndef _GNU_SOURCE\n"
	"#  define _GNU_SOURCE\n"
	"#endif\n"
	"#ifndef _POSIX_C_SOURCE\n"
	"#  define _POSIX_C_SOURCE 200809L\n"
	"#endif\n"
	"#ifndef _SVID_SOURCE\n"
	"#  define _SVID_SOURCE\n"
	"#endif\n"
	"#ifndef _XOPEN_SOURCE\n"
	"#  define _XOPEN_SOURCE 700\n"
	"#endif\n\n"
	"#ifdef __INTEL_COMPILER\n"
	"#  define _Float128 float_t\n"
	"#else\n"
	"#  include <complex.h>\n"
	"#endif\n\n"
	"#include <assert.h>\n"
	"#include <ctype.h>\n"
	"#include <error.h>\n"
	"#include <errno.h>\n"
	"#include <float.h>\n"
	"#include <fcntl.h>\n"
	"#include <limits.h>\n"
	"#include <locale.h>\n"
	"#include <math.h>\n"
	"#include <pthread.h>\n"
	"#include <regex.h>\n"
	"#include <setjmp.h>\n"
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
	"#include <sys/mman.h>\n"
	"#include <sys/types.h>\n"
	"#include <sys/socket.h>\n"
	"#include <sys/syscall.h>\n"
	"#include <sys/wait.h>\n"
	"#include <time.h>\n"
	"#include <uchar.h>\n"
	"#include <wchar.h>\n"
	"#include <wctype.h>\n"
	"#include <unistd.h>\n\n"
	"extern char **environ;\n\n"
	"#line 1\n";

/* compiler pre-program */
static char const prog_start[] =
	"\nint main(int argc, char *argv[]) "
	"{"
		"\n\t(void)argc, (void)argv;\n";
/* pre-program shown to user */
static char const prog_start_user[] =
	"\nint main(int argc, char *argv[])\n"
	"{\n";
static char const prog_end[] =
		"\n\treturn 0;"
	"\n}\n";

/* enumerations */
enum src_flag {
	NOT_IN_MAIN, IN_MAIN, EMPTY,
};

/* asm dialect */
enum asm_type {
	NONE, ATT, INTEL,
};

/* possible types of tracked variable */
enum var_type {
	T_ERR, T_CHR, T_STR,
	T_INT, T_UINT, T_DBL,
	T_PTR, T_OTHER,
};

/* struct definition for NULL-terminated string dynamic array */
typedef struct _str_list {
	size_t cnt, max;
	char **list;
} STR_LIST;

/* struct definition for flag dynamic array */
typedef struct _flag_list {
	size_t cnt, max;
	enum src_flag *list;
} FLAG_LIST;

/* struct definition for type dynamic array */
typedef struct _type_list {
	size_t cnt, max;
	enum var_type *list;
} TYPE_LIST;

/* struct definition for var-tracking array */
typedef struct _var_list {
	size_t cnt, max;
	struct {
		char *id;
		enum var_type type_spec;
	} *list;
} VAR_LIST;

/* struct definition for generated program sources */
typedef struct _prog_src {
	size_t b_sz, f_sz, t_sz;
	size_t b_max, f_max, t_max;
	char *b, *f, *total;
	STR_LIST hist, lines;
	FLAG_LIST flags;
} PROG_SRC;

/* `malloc()` wrapper */
static inline void xmalloc(void *restrict ptr, size_t sz, char const *msg)
{
	/* sanity check */
	if (!ptr)
		return;
	if (!(*(void **)ptr = malloc(sz)))
		ERR(msg ? msg : "(nil)");
}

/* `calloc()` wrapper */
static inline void xcalloc(void *restrict ptr, size_t nmemb, size_t sz, char const *msg)
{
	/* sanity check */
	if (!ptr)
		return;
	if (!(*(void **)ptr = calloc(nmemb, sz)))
		ERR(msg ? msg : "(nil)");
}

/* `realloc()` wrapper */
static inline void xrealloc(void *restrict ptr, size_t sz, char const *msg)
{
	void *tmp;
	/* sanity check */
	if (!ptr)
		return;
	if (!(tmp = realloc(*(void **)ptr, sz)))
		ERR(msg ? msg : "(nil)");
	*(void **)ptr = tmp;
}

/* `fread()` wrapper */
static inline size_t xfread(void *restrict ptr, size_t sz, size_t nmemb, FILE *restrict stream)
{
	size_t cnt;
	if ((cnt = fread(ptr, sz, nmemb, stream)) < nmemb)
		return 0;
	return cnt;
}

/* recursive free */
static inline ptrdiff_t free_argv(char ***restrict argv)
{
	ptrdiff_t cnt;
	if (!argv || !*argv)
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
	char *dest_ptr = NULL, *src_ptr = memchr(src, '\0', EVAL_LIMIT);
	if (off >= 0) {
		dest_ptr = dest + off;
	} else {
		dest_ptr = memchr(dest, '\0', EVAL_LIMIT);
	}
	if (!src_ptr || !dest_ptr)
		ERRX("strmv() string not null-terminated");
	src_sz = src_ptr - src;
	memcpy(dest_ptr, src, (size_t)src_sz + 1);
}

static inline ptrdiff_t free_str_list(STR_LIST *restrict plist)
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

static inline void init_list(STR_LIST *restrict list_struct, char *restrict init_str)
{
	list_struct->cnt = 0;
	list_struct->max = 1;
	xcalloc(&list_struct->list, 1, sizeof *list_struct->list, "list_ptr calloc()");
	if (!init_str)
		return;
	list_struct->cnt++;
	xcalloc(&list_struct->list[list_struct->cnt - 1], 1, strlen(init_str) + 1, "init_list()");
	strmv(0, list_struct->list[list_struct->cnt - 1], init_str);
}

static inline void append_str(STR_LIST *restrict list_struct, char const *restrict string, size_t pad)
{
	/* sanity checks */
	if (++list_struct->cnt > ARRAY_MAX)
		ERRX("list_struct->cnt > (SIZE_MAX / 2 - 1)");
	if (!list_struct->list)
		ERRX("NULL list_struct->list passed to append_str()");
	/* realloc if cnt reaches current size */
	if (list_struct->cnt >= list_struct->max) {
		list_struct->max *= 2;
		xrealloc(&list_struct->list, sizeof *list_struct->list * list_struct->max, "append_str()");
	}
	if (!string) {
		list_struct->list[list_struct->cnt - 1] = NULL;
		return;
	}
	xcalloc(&list_struct->list[list_struct->cnt - 1], 1, strlen(string) + pad + 1, "append_str()");
	strmv(pad, list_struct->list[list_struct->cnt - 1], string);
}

static inline void init_tlist(TYPE_LIST *restrict list_struct)
{
	list_struct->cnt = 0;
	list_struct->max = 1;
	xcalloc(&list_struct->list, 1, sizeof *list_struct->list, "init_tlist()");
}

static inline void append_type(TYPE_LIST *restrict list_struct, enum var_type type_spec)
{
	/* check if size too large */
	if (++list_struct->cnt > ARRAY_MAX)
		ERRX("list_struct->cnt > (SIZE_MAX / 2 - 1)");
	/* realloc if cnt reaches current size */
	if (list_struct->cnt >= list_struct->max) {
		list_struct->max *= 2;
		xrealloc(&list_struct->list, sizeof *list_struct->list * list_struct->max, "append_type()");
	}
	list_struct->list[list_struct->cnt - 1] = type_spec;
}

static inline void init_flag_list(FLAG_LIST *restrict list_struct)
{
	list_struct->cnt = 0;
	list_struct->max = 1;
	xcalloc(&list_struct->list, 1, sizeof *list_struct->list, "init_flag_list()");
	list_struct->cnt++;
	list_struct->list[list_struct->cnt - 1] = EMPTY;
}

static inline void append_flag(FLAG_LIST *restrict list_struct, enum src_flag flag)
{
	/* check if size too large */
	if (++list_struct->cnt > ARRAY_MAX)
		ERRX("list_struct->cnt > (SIZE_MAX / 2 - 1)");
	/* realloc if cnt reaches current size */
	if (list_struct->cnt >= list_struct->max) {
		list_struct->max *= 2;
		xrealloc(&list_struct->list, sizeof *list_struct->list * list_struct->max, "append_flag()");
	}
	list_struct->list[list_struct->cnt - 1] = flag;
}

static inline STR_LIST strsplit(char const *restrict str)
{
	if (!str)
		return (STR_LIST){0};

	STR_LIST list_struct;
	bool str_lit = false, chr_lit = false;
	size_t memb_cnt = 0;
	char arr[strlen(str) + 1], *ptr = arr;

	strmv(0, ptr, str);
	init_list(&list_struct, NULL);

	for (; *ptr; ptr++) {
		switch (*ptr) {
		case '\\':
			ptr++;
			break;

		case '"':
			if (!chr_lit)
				str_lit ^= true;
			break;

		case '\'':
			if (!str_lit)
				str_lit ^= true;
			chr_lit ^= true;
			break;

		case '{':
			if (!str_lit && !chr_lit)
				memb_cnt++;
			break;

		case '}':
			if (!str_lit && !chr_lit && memb_cnt > 0)
				memb_cnt--;
			break;

		case ';':
			if (!str_lit && !chr_lit && !memb_cnt)
				*ptr = '\x1c';
			break;
		}
	}

	ptr = arr;
	for (char *tmp = strtok(ptr, "\x1c"); tmp; tmp = strtok(NULL, "\x1c")) {
		while (isspace(*tmp))
			tmp++;
		append_str(&list_struct, tmp, 0);
	}

#ifdef _DEBUG
	for (size_t i = 0; i < list_struct.cnt; i++)
		puts(list_struct.list[i]);
#endif

	return list_struct;
}

#endif
