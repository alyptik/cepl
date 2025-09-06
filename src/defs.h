/*
 * defs.h - data structure and macro definitions
 *
 * AUTHOR: Joey Pabalinas <joeypabalinas@gmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#if !defined(DEFS_H)
#define DEFS_H 1

/* silence linter */
#if !defined(_GNU_SOURCE)
#	define _GNU_SOURCE
#endif

#include "errs.h"
#include <ctype.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <termios.h>
#include <unistd.h>

/* macros */
#define arr_len(arr)		((sizeof (arr)) / (sizeof *(arr)))
#define printe(fmt, ...)	fprintf(stderr, "\033[92m" fmt "\033[00m", __VA_ARGS__)

/* `malloc()` wrapper */
#define xmalloc(ptr, sz, msg)					\
	({							\
		void *__tmp = malloc((sz));			\
		if (!__tmp)					\
			ERR("%s", (msg));			\
		*(ptr) = __tmp;					\
	})
/* `calloc()` wrapper */
#define xcalloc(ptr, nmemb, sz, msg)				\
	({							\
		void *__tmp = calloc((nmemb), (sz));		\
		if (!__tmp)					\
			ERR("%s", (msg));			\
		*(ptr) = __tmp;					\
	})
/* `realloc()` wrapper */
#define xrealloc(ptr, sz, msg)					\
	({							\
		void *__tmp[2] = {0, *(ptr)};			\
		if (!(__tmp[0] = realloc(__tmp[1], (sz))))	\
			ERR("%s", (msg));			\
		*(ptr) = __tmp[0];				\
	})

/* global version and usage strings */
#define VERSION_STRING	"cepl-26.0.0"
#define USAGE_STRING \
	"[-hpvw] [-c<compiler>] [-e<code to evaluate>] [-l<library>] " \
	"[-I<include directory>] [-L<library directory>] [-s<standard>] " \
	"[-o<out.c>]\n\t" \
	"-c, --compiler\t\tSpecify alternate compiler\n\t" \
	"-e, --eval\t\tEvaluate the following argument as C/C++ code\n\t" \
	"-h, --help\t\tShow help/usage information\n\t" \
	"-o, --output\t\tName of the file to output C/C++ source code to\n\t" \
	"-p, --parse\t\tDisable addition of dynamic library symbols to readline completion\n\t" \
	"-s, --std\t\tSpecify which C/C++ standard to use\n\t" \
	"-v, --version\t\tShow version information\n\t" \
	"-w, --warnings\t\tCompile with \"-Wall -Wextra -pedantic\" flags\n\t" \
	"-l\t\t\tLink against specified library (flag can be repeated)\n\t" \
	"-I\t\t\tSearch directory for header files (flag can be repeated)\n\t" \
	"-L\t\t\tSearch directory for libraries (flag can be repeated)\n" \
	"Lines prefixed with a \";\" are interpreted as commands ([] text is optional).\n\t" \
	";f[unction]\t\tLine is defined outside of main() (e.g. ;f #define SWAP2(X) ((((X) >> 8) & 0xff) | (((X) & 0xff) << 8)))\n\t" \
	";h[elp]\t\t\tShow help\n\t" \
	";m[an]\t\t\tShow manpage for argument (e.g. ;m strpbrk\n\t" \
	";q[uit]\t\t\tExit CEPL\n\t" \
	";r[eset]\t\tReset CEPL to its initial program state\n\t" \
	";u[ndo]\t\t\tIncremental pop_history (can be repeated)" \

/* state flags */
#define CXX_FLAG		0x01u
#define EVAL_FLAG		0x02u
#define EXEC_FLAG		0x04u
#define HIST_FLAG		0x08u
#define INPUT_FLAG		0x10u
#define OUT_FLAG		0x20u
#define PARSE_FLAG		0x40u
#define STD_FLAG		0x80u
#define WARN_FLAG		0x100u

/* page size for buffer count */
#define PAGE_SIZE	0x1000u
/* max eval string length */
#define EVAL_LIMIT	(PAGE_SIZE * PAGE_SIZE)
/* `strmv() `concat constant */
#define CONCAT		(-1)

/* enumerations */
enum src_flag {
	NOT_IN_MAIN, IN_MAIN, EMPTY,
};

/* input src state */
enum scan_state {
	IN_PROLOGUE, IN_MIDDLE, IN_EPILOGUE,
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

/* struct definition for program source sections */
struct source_section {
	size_t size, max;
	char *buf;
};

/* struct definition for generated program sources */
struct source_code {
	struct source_section body, funcs, total;
	struct str_list hist, lines;
	struct flag_list flags;
};

/* standard io stream state state */
struct termios_state {
	bool modes_changed;
	struct termios save_modes[4];
};

/* monolithic program structure */
struct program {
	FILE *ofile;
	unsigned int state_flags;
	char *input_src[3], eval_arg[EVAL_LIMIT];
	char *cur_line, *hist_file;
	char *out_filename, *asm_filename;
	struct str_list cc_list;
	struct str_list lib_list, sym_list;
	struct str_list id_list;
	struct source_code src[2];
	struct termios_state tty_state;
};

/* reset signal handlers before fork */
static inline void reset_handlers(void)
{
	/* signals to trap */
	struct { int sig; char const *sig_name; } sigs[] = {
		{SIGHUP, "SIGHUP"}, {SIGINT, "SIGINT"},
		{SIGQUIT, "SIGQUIT"}, {SIGILL, "SIGILL"},
		{SIGABRT, "SIGABRT"}, {SIGFPE, "SIGFPE"},
		{SIGSEGV, "SIGSEGV"}, {SIGPIPE, "SIGPIPE"},
		{SIGALRM, "SIGALRM"}, {SIGTERM, "SIGTERM"},
		{SIGBUS, "SIGBUS"}, {SIGSYS, "SIGSYS"},
		{SIGVTALRM, "SIGVTALRM"}, {SIGXCPU, "SIGXCPU"},
		{SIGXFSZ, "SIGXFSZ"},
	};
	struct sigaction sa[arr_len(sigs)];
	for (size_t i = 0; i < arr_len(sigs); i++) {
		sa[i].sa_handler = SIG_DFL;
		sigemptyset(&sa[i].sa_mask);
		/* don't reset `SIGINT` handler */
		sa[i].sa_flags = SA_RESETHAND;
		if (sigaction(sigs[i].sig, &sa[i], NULL) == -1)
			ERR("%s %s", sigs[i].sig_name, "sigaction()");
	}
}

/* `fopen()` wrapper */
static inline void xfopen(FILE **file, char const *path, char const *fmode)
{
	if (!(*file = fopen(path, fmode)))
		ERR("xfopen()");
}

/* `fclose()` wrapper */
static inline void xfclose(FILE **out_file)
{
	if (!out_file || !*out_file)
		return;
	if (fclose(*out_file) == EOF)
		WARN("xfclose()");
}

/* recursive free */
static inline ptrdiff_t free_argv(char ***argv)
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
static inline void strmv(ptrdiff_t off, char *dest, char const *src) {
	/* sanity checks */
	if (!dest || !src)
		ERRX("NULL pointer passed to strmv()");
	ptrdiff_t src_sz;
	char *dest_ptr = dest;
	char const *src_end = src;
	/* find the end of the source string */
	for (size_t i = 0; i < EVAL_LIMIT && *src_end; i++, src_end++);
	/* find the end of the desitnation string if offset is negative */
	if (off < 0)
		for (size_t i = 0; i < EVAL_LIMIT && *dest_ptr; i++, dest_ptr++);
	else
		dest_ptr = dest + off;
	if (!src_end || !dest_ptr)
		ERRX("strmv() string not null-terminated");
	src_sz = src_end - src;
	if (src_sz < 0)
		ERRX("strmv() src_end - src < 0");
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

static inline void init_str_list(struct str_list *list_struct, char *init_str)
{
	list_struct->cnt = 0;
	list_struct->max = 1;
	xcalloc(&list_struct->list, 1, sizeof *list_struct->list, "list_ptr calloc()");
	if (!init_str)
		return;
	list_struct->cnt++;
	xcalloc(&list_struct->list[list_struct->cnt - 1], 1, strlen(init_str) + 1, "init_str_list()");
	strmv(0, list_struct->list[list_struct->cnt - 1], init_str);
}

static inline void append_str(struct str_list *list_struct, char const *string, size_t pad)
{
	/* sanity checks */
	if (!list_struct->list)
		ERRX("NULL list_struct->list passed to append_str()");
	/* realloc if cnt reaches current size */
	if (++list_struct->cnt >= list_struct->max) {
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

static inline void init_flag_list(struct flag_list *list_struct)
{
	list_struct->cnt = 0;
	list_struct->max = 1;
	xcalloc(&list_struct->list, 1, sizeof *list_struct->list, "init_flag_list()");
	list_struct->cnt++;
	list_struct->list[list_struct->cnt - 1] = EMPTY;
}

static inline void append_flag(struct flag_list *list_struct, enum src_flag flag)
{
	/* realloc if cnt reaches current size */
	if (++list_struct->cnt >= list_struct->max) {
		list_struct->max *= 2;
		xrealloc(&list_struct->list, sizeof *list_struct->list * list_struct->max, "append_flag()");
	}
	list_struct->list[list_struct->cnt - 1] = flag;
}

#endif /* !defined(DEFS_H) */
