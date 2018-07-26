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
#include <termio.h>
#include <unistd.h>

/* macros */
#define DEFAULT(ARG, ALT)	((ARG) ? (ARG) : (ALT))
#define MIN(A, B)		(((A) < (B)) ? (A) : (B))
#define MAX(A, B)		(((A) > (B)) ? (A) : (B))
#define ARR_LEN(ARR)		((sizeof (ARR)) / (sizeof *(ARR)))
#define FOR_EACH_IN(LIST)	for (size_t i = 0; i < (LIST).cnt; i++)
#define DPRINTF(FMT, ...)	fprintf(stderr, "\033[92m" FMT "\033[00m", __VA_ARGS__)
/* `malloc()` wrapper */
#define xmalloc(TYPE, PTR, SZ, MSG) \
	do { \
		void *tmp = malloc(SZ); \
		if (!tmp) \
			ERR("%s", (MSG)); \
		*(TYPE **)PTR = tmp; \
	} while (0)
/* `calloc()` wrapper */
#define xcalloc(TYPE, PTR, NMEMB, SZ, MSG) \
	do { \
		void *tmp = calloc((NMEMB), (SZ)); \
		if (!tmp) \
			ERR("%s", (MSG)); \
		*(TYPE **)PTR = tmp; \
	} while (0)
/* `realloc()` wrapper */
#define xrealloc(TYPE, PTR, SZ, MSG) \
	do { \
		void *tmp[2] = {0, *(TYPE **)PTR}; \
		if (!(tmp[0] = realloc(tmp[1], SZ))) \
			ERR("%s", (MSG)); \
		*(TYPE **)PTR = tmp[0]; \
	} while (0)

/* global version and usage strings */
#define VERSION_STRING	"CEPL v6.2.1"
#define USAGE_STRING \
	"[-hptvw] [-(a|i)<asm.s>] [-c<compiler>] [-e<code>] " \
	"[-l<libs>] [-I<includes>] [-o<out.c>]\n\t" \
	"-a, --att\t\tName of the file to output AT&T-dialect assembler code to\n\t" \
	"-c, --cc\t\tSpecify alternate compiler\n\t" \
	"-e, --eval\t\tEvaluate the following argument as C code\n\t" \
	"-f, --file\t\tName of file to use as starting C code template\n\t" \
	"-h, --help\t\tShow help/usage information\n\t" \
	"-i, --intel\t\tName of the file to output Intel-dialect assembler code to\n\t" \
	"-o, --output\t\tName of the file to output C source code to\n\t" \
	"-p, --parse\t\tDisable addition of dynamic library symbols to readline completion\n\t" \
	"-t, --tracking\t\tToggle variable tracking\n\t" \
	"-v, --version\t\tShow version information\n\t" \
	"-w, --warnings\t\tCompile with \"-Wall -Wextra -pedantic\" flags\n\t" \
	"-l\t\t\tLink against specified library (flag can be repeated)\n\t" \
	"-I\t\t\tSearch directory for header files (flag can be repeated)\n" \
	"Lines prefixed with a \";\" are interpreted as commands ([] text is optional).\n\t" \
	";a[tt]\t\t\tToggle -a (output AT&T-dialect assembler code) flag\n\t" \
	";h[elp]\t\t\tShow help\n\t" \
	";i[ntel]\t\tToggle -a (output Intel-dialect assembler code) flag\n\t" \
	";m[acro]\t\tDefine a function (e.g. \";f void bork(void) { puts(\"wark\"); }\")\n\t" \
	";o[utput]\t\tToggle -o (output C source code) flag\n\t" \
	";p[arse]\t\tToggle -p (shared library parsing) flag\n\t" \
	";q[uit]\t\t\tExit CEPL\n\t" \
	";r[eset]\t\tReset CEPL to its initial program state\n\t" \
	";t[racking]\t\tToggle variable tracking\n\t" \
	";u[ndo]\t\t\tIncremental pop_history (can be repeated)\n\t" \
	";w[arnings]\t\tToggle -w (pedantic warnings) flag"
/* option default initializer */
#define STATE_FLAG_DEF_INIT \
	(struct state_flags){ \
		.asm_flag = false, .eval_flag = false, .exec_flag = false, \
		.in_flag = false, .out_flag = false, .parse_flag = true, \
		.track_flag = true, .warn_flag = false, .hist_flag = false, \
	}
#define	RED		"\\033[31m"
#define	GREEN		"\\033[32m"
#define	YELLOW		"\\033[33m"
#define	PURPLE		"\\033[35m"
#define	CYAN		"\\033[36m"
#define	RST		"\\033[00m"
/* page size for buffer count */
#define PAGE_SIZE	0x1000
/* max eval string length */
#define EVAL_LIMIT	PAGE_SIZE
/* max possible types */
#define TNUM		7
/* `strmv() `concat constant */
#define CONCAT		(-1)

/* enumerations */
enum src_flag {
	NOT_IN_MAIN, IN_MAIN, EMPTY,
};

/* input src state */
enum scan_state {
	IN_PRELUDE, IN_MIDDLE, IN_EPILOGUE,
};

/* asm dialect */
enum asm_type {
	NONE, ATT, INTEL,
};

/* possible types of tracked variable */
enum var_type {
	T_ERR, T_CHR, T_STR,
	T_INT, T_UINT, T_FLT,
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

/* struct definition for type dynamic array */
struct type_list {
	size_t cnt, max;
	enum var_type *list;
};

/* struct definition for var-tracking array */
struct var_list {
	size_t cnt, max;
	struct {
		char *id;
		enum var_type type_spec;
	} *list;
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

/* struct definition for state flags */
struct state_flags {
	bool asm_flag, eval_flag;
	bool exec_flag, parse_flag;
	bool track_flag, warn_flag;
	bool in_flag, out_flag, hist_flag;
};

/* standard io stream state state */
struct termio_state {
	bool modes_changed;
	struct termio save_modes[4];
};

/* monolithic program structure */
struct program {
	FILE *ofile;
	int saved_fd;
	char *input_src[3], eval_arg[EVAL_LIMIT];
	char *cur_line, *hist_file;
	char *out_filename, *asm_filename;
	struct str_list cc_list, ld_list;
	struct str_list lib_list, sym_list;
	struct str_list id_list;
	struct type_list type_list;
	struct var_list var_list;
	struct source_code src[2];
	struct state_flags sflags;
	struct termio_state tty_state;
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
	struct sigaction sa[ARR_LEN(sigs)];
	for (size_t i = 0; i < ARR_LEN(sigs); i++) {
		sa[i].sa_handler = SIG_DFL;
		sigemptyset(&sa[i].sa_mask);
		/* don't reset `SIGINT` handler */
		sa[i].sa_flags = SA_RESETHAND;
		if (sigaction(sigs[i].sig, &sa[i], NULL) == -1)
			ERR("%s %s", sigs[i].sig_name, "sigaction()");
	}
}

/* `fclose()` wrapper */
static inline void xfclose(FILE **restrict out_file)
{
	if (!out_file || !*out_file)
		return;
	if (fclose(*out_file) == EOF)
		WARN("%s", "xfclose()");
}

/* `fopen()` wrapper */
static inline void xfopen(FILE **restrict file, char const *restrict path, char const *restrict fmode)
{
	if (!(*file = fopen(path, fmode)))
		ERR("%s", "xfopen()");
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
		ERRX("%s", "NULL pointer passed to strmv()");
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
		ERRX("%s", "strmv() string not null-terminated");
	src_sz = src_end - src;
	if (src_sz < 0)
		ERRX("%s", "strmv() src_end - src < 0");
	memcpy(dest_ptr, src, (size_t)src_sz + 1);
}

static inline ptrdiff_t free_str_list(struct str_list *restrict plist)
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

static inline void init_str_list(struct str_list *restrict list_struct, char *restrict init_str)
{
	list_struct->cnt = 0;
	list_struct->max = 1;
	xcalloc(char, &list_struct->list, 1, sizeof *list_struct->list, "list_ptr calloc()");
	if (!init_str)
		return;
	list_struct->cnt++;
	xcalloc(char, &list_struct->list[list_struct->cnt - 1], 1, strlen(init_str) + 1, "init_str_list()");
	strmv(0, list_struct->list[list_struct->cnt - 1], init_str);
}

static inline void append_str(struct str_list *restrict list_struct, char const *restrict string, size_t pad)
{
	/* sanity checks */
	if (!list_struct->list)
		ERRX("%s", "NULL list_struct->list passed to append_str()");
	/* realloc if cnt reaches current size */
	if (++list_struct->cnt >= list_struct->max) {
		list_struct->max *= 2;
		xrealloc(char, &list_struct->list, sizeof *list_struct->list * list_struct->max, "append_str()");
	}
	if (!string) {
		list_struct->list[list_struct->cnt - 1] = NULL;
		return;
	}
	xcalloc(char, &list_struct->list[list_struct->cnt - 1], 1, strlen(string) + pad + 1, "append_str()");
	strmv(pad, list_struct->list[list_struct->cnt - 1], string);
}

static inline void init_type_list(struct type_list *restrict list_struct)
{
	list_struct->cnt = 0;
	list_struct->max = 1;
	xcalloc(int, &list_struct->list, 1, sizeof *list_struct->list, "init_type_list()");
}

static inline void append_type(struct type_list *restrict list_struct, enum var_type type_spec)
{
	/* realloc if cnt reaches current size */
	if (++list_struct->cnt >= list_struct->max) {
		list_struct->max *= 2;
		xrealloc(int, &list_struct->list, sizeof *list_struct->list * list_struct->max, "append_type()");
	}
	list_struct->list[list_struct->cnt - 1] = type_spec;
}

static inline void init_flag_list(struct flag_list *restrict list_struct)
{
	list_struct->cnt = 0;
	list_struct->max = 1;
	xcalloc(int, &list_struct->list, 1, sizeof *list_struct->list, "init_flag_list()");
	list_struct->cnt++;
	list_struct->list[list_struct->cnt - 1] = EMPTY;
}

static inline void append_flag(struct flag_list *restrict list_struct, enum src_flag flag)
{
	/* realloc if cnt reaches current size */
	if (++list_struct->cnt >= list_struct->max) {
		list_struct->max *= 2;
		xrealloc(int, &list_struct->list, sizeof *list_struct->list * list_struct->max, "append_flag()");
	}
	list_struct->list[list_struct->cnt - 1] = flag;
}

static inline struct str_list strsplit(char const *restrict str)
{
	if (!str)
		return (struct str_list){0};

	struct str_list list_struct;
	bool str_lit = false, chr_lit = false;
	size_t memb_cnt = 0;
	char arr[strlen(str) + 1], *ptr = arr;

	strmv(0, ptr, str);
	init_str_list(&list_struct, NULL);

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
		case '\n': /* fallthrough */
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

	return list_struct;
}

#endif /* !defined(DEFS_H) */
