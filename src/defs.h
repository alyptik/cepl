/*
 * defs.h - data structure and macro definitions
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#ifndef DEFS_H
#define DEFS_H

#include <unistd.h>

/* global version and usage strings */
#define VERSION_STRING	("CEPL v4.3.0")
#define USAGE_STRING	("[-hptvw] [-c<compiler>] [-l<library>] [-I<include dir>] [-o<output.c>]\n\n\t" \
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
	";w[arnings]:\t\tToggle -w (warnings) flag")
/* `strmv() `concat constant */
#define CONCAT		(-1)
/* `malloc()` size ceiling */
#define MAX		(SIZE_MAX / 2 - 1)
/* page size for buffer count */
#define COUNT		(sysconf(_SC_PAGESIZE))

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
	char *body, *funcs, *total;
	struct str_list hist, lines;
	struct flag_list flags;
};

#endif
