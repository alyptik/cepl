/*
 * parseopts.h - option parsing
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#ifndef PARSEOPTS_H
#define PARSEOPTS_H 1

#include <err.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define CEPL_VERSION "CEPL v1.3.0"
#define USAGE "[-hpvw] [-c<compiler>] [-l<library>] [-I<include dir>] [-o<output.c>]\n\n\t-h,--help:\t\tShow help/usage information.\n\t-p,--parse:\t\tAdd symbols from dynamic libraries to readline completion.\n\t-v,--version:\t\tShow version information.\n\t-w,--warnings:\t\tCompile with ”-pedantic-errors -Wall -Wextra” flags.\n\t-c,--compiler:\t\tSpecify alternate compiler.\n\t-l:\t\t\tLink against specified library (flag can be repeated).\n\t-I:\t\t\tSearch directory for header files (flag can be repeated).\n\t-o:\t\t\tName of the file to output source to.\n\nInput lines prefixed with a “;” are used to control internal state.\n\n\t;f[unction]:\t\tDefine a function (e.g. “;f void foo(void) { … }”)\n\t;h[elp]:\t\tShow help\n\t;i[nclude]:\t\tDefine an include (e.g. “;i #include <crypt.h>”)\n\t;m[acro]:\t\tDefine a macro (e.g. “;m #define ZERO(x) (x ^ x)”)\n\t;p[arse]:\t\tToggle -p (shared library parsing) flag\n\t;q[uit]:\t\tExit CEPL\n\t;r[eset]:\t\tReset CEPL to its initial program state\n\t:w[arnings]:\t\tToggle -w (warnings) flag"
/* perl script to parse symbols in shared libraries */
#define ELF_SCRIPT "./elfsyms"
/* pipe buffer size */
#define COUNT sysconf(_SC_PAGESIZE)

/* silence linter */
int getopt(int argc, char * const argv[], const char *optstring);
FILE *fdopen(int fd, const char *mode);
ssize_t getline(char **lineptr, size_t *n, FILE *stream);

char **parse_opts(int argc, char *argv[], char const optstring[], FILE **ofile);
char **parse_libs(char *libs[]);

/* what kind of statement last line was */
enum last_line {
	EMPTY = 0,
	NOT_IN_MAIN = 1,
	IN_MAIN = 2,
};

/* struct definition for NULL terminated string array */
struct str_list {
	int cnt;
	char **list;
};

/* struct definition for NULL terminated enum array */
struct flag_list {
	int cnt;
	enum last_line *list;
};

static inline int free_argv(char **argv)
{
	int count;
	if (!argv || !argv[0])
		return -1;
	for (count = 0; argv[count]; count++)
		free(argv[count]);
	free(argv);
	return count;
}

static inline void init_list(struct str_list *list_struct, char *initial_str)
{
	if ((list_struct->list = malloc((sizeof *list_struct->list) * ++list_struct->cnt)) == NULL)
		err(EXIT_FAILURE, "%s", "error during initial list_ptr malloc()");
	if ((*(list_struct->list + list_struct->cnt - 1) = malloc(strlen(initial_str) + 1)) == NULL)
		err(EXIT_FAILURE, "%s", "error during initial list_ptr[0] malloc()");
	memset(*(list_struct->list + list_struct->cnt - 1), 0, strlen(initial_str) + 1);
	memcpy(*(list_struct->list + list_struct->cnt - 1), initial_str, strlen(initial_str) + 1);
}

static inline void append_str(struct str_list *list_struct, char *str, size_t offset)
{
	char **temp;
	if ((temp = realloc(list_struct->list, (sizeof *list_struct->list) * ++list_struct->cnt)) == NULL)
		err(EXIT_FAILURE, "%s[%d] %s", "error during list_ptr", list_struct->cnt - 1, "malloc()");
	list_struct->list = temp;
	if (!str) {
		*(list_struct->list + list_struct->cnt - 1) = NULL;
	} else {
		if ((*(list_struct->list + list_struct->cnt - 1) = malloc(strlen(str) + offset + 1)) == NULL)
			err(EXIT_FAILURE, "%s[%d]", "error appending string to list_ptr", list_struct->cnt - 1);
		memset(*(list_struct->list + list_struct->cnt - 1), 0, strlen(str) + offset + 1);
		memcpy(*(list_struct->list + list_struct->cnt - 1) + offset, str, strlen(str) + 1);
	}
}

static inline void init_flag_list(struct flag_list *list_struct)
{
	if ((list_struct->list = malloc((sizeof *list_struct->list) * ++list_struct->cnt)) == NULL)
		err(EXIT_FAILURE, "%s", "error during initial flag_list malloc()");
	*(list_struct->list + list_struct->cnt - 1) = EMPTY;
}

static inline void append_flag(struct flag_list *list_struct, enum last_line flag)
{
	enum last_line *temp;
	if ((temp = realloc(list_struct->list, (sizeof *list_struct->list) * ++list_struct->cnt)) == NULL)
		err(EXIT_FAILURE, "%s %d %s", "error during flag_list (cnt = ", list_struct->cnt, ") realloc()");
	list_struct->list = temp;
	*(list_struct->list + list_struct->cnt - 1) = flag;
}

#endif
