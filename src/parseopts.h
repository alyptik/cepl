/*
 * parseopts.h - option parsing
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE file for copyright and license details.
 */

#ifndef PARSEOPTS_H
#define PARSEOPTS_H 1

#include <stdio.h>
#include <stdlib.h>

#define CEPL_VERSION "CEPL v0.2.4"
#define ELF_SCRIPT "./elfsyms"
#define COUNT sysconf(_SC_PAGESIZE)
#define USAGE "[-hvwp] [-l<library>] [-I<include dir>] [-o<output.c>]\n\n\t-h: Show help/usage information.\n\t-v: Show version information.\n\t-w: Compile with ”-pedantic-errors -Wall -Wextra” flags.\n\t-p: Add symbols from dynamic libraries to readline completion.\n\t-l: Link against specified library (flag can be repeated).\n\t-I: Search directory for header files (flag can be repeated).\n\t-o: Name of the file to output source to."

char *const *parse_opts(int argc, char *argv[], char *const optstring, FILE **ofile);
char **parse_libs(char *libs[]);

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

#endif
