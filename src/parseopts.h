/*
 * parseopts.h - option parsing
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE file for copyright and license details.
 */

#ifndef PARSEOPTS_H
#define PARSEOPTS_H 1

#include <stdio.h>

#define CEPL_VERSION "CEPL v0.1.8"
#define USAGE "Usage: %s [-hvwp] [-l<library>] [-I<include dir>] [-o<sessionlog.c>]\n\n\t-h: Show help/usage information.\n\t-v: Show version information.\n\t-w: Compile with \"-pedantic-errors -Wall -Wextra\" flags.\n\t-p: Add symbols from dynamic libraries to readline completion.\n\t-l: Link against specified library (flag can be repeated).\n\t-I: Search directory for header files (flag can be repeated).\n\t-o: Name of the file to output source to."
#define COUNT sysconf(_SC_PAGESIZE)

char *const *parse_opts(int argc, char *argv[], char *optstring, FILE **ofile);
char **parse_libs(char *libs[]);

static inline int free_argv(char **argv)
{
	int i;
	if (!argv || !argv[0])
		return -1;
	for (i = 0; argv[i]; i++)
		free(argv[i]);
	free(argv);
	return i;
}

#endif
