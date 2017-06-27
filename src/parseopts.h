/*
 * parseopts.h - option parsing
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE file for copyright and license details.
 */

#ifndef PARSEOPTS_H
#define PARSEOPTS_H 1

#include <stdio.h>

#define CEPL_VERSION "CEPL v0.1.7"
#define USAGE "Usage: %s [-hvw] [-l<library>] [-I<include dir>] [-o<sessionlog.c>]\n\n\t-h: Show help/usage information.\n\t-v: Show version information.\n\t-w: Compile with \"-pedantic-errors -Wall -Wextra\" flags.\n\t-l: Link against specified library (flag can be repeated).\n\t-I: Search directory for header files (flag can be repeated).\n\t-o: Name of the file to output source to."
#define COUNT sysconf(_SC_PAGESIZE)

char *const *parse_opts(int argc, char *argv[], char *optstring, FILE **ofile);
char **parse_libs(char *libs[]);

static inline int free_cc_argv(char **cc_argv)
{
	int i;
	if (!cc_argv || !cc_argv[0])
		return -1;
	for (i = 0; cc_argv[i]; i++)
		free(cc_argv[i]);
	free(cc_argv);
	return i;
}

#endif
