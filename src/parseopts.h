/*
 * parseopts.h - option parsing
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE file for copyright and license details.
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

#define CEPL_VERSION "CEPL v0.5.2"
#define USAGE "[-hpvw] [-c<compiler>] [-l<library>] [-I<include dir>] [-o<output.c>]\n\n\t-h: Show help/usage information.\n\t-p: Add symbols from dynamic libraries to readline completion.\n\t-v: Show version information.\n\t-w: Compile with ”-pedantic-errors -Wall -Wextra” flags.\n\t-c: Specify alternate compiler.\n\t-l: Link against specified library (flag can be repeated).\n\t-I: Search directory for header files (flag can be repeated).\n\t-o: Name of the file to output source to.\n\nInput lines prefixed with a “;” are used to control internal state.\n\n\t;p[arse]: Toggle -p (shared library parsing) flag\n\t;q[uit]: Exit CEPL\n\t;r[eset]: Reset CEPL to its initial program state\n\t:w[arnings]: Toggle -w (warnings) flag"
/* perl script to pull symbols from shared libraries */
#define ELF_SCRIPT "./elfsyms"
/* pipe buffer size */
#define COUNT sysconf(_SC_PAGESIZE)

/* silence linter */
int getopt(int argc, char * const argv[], const char *optstring);
FILE *fdopen(int fd, const char *mode);
ssize_t getline(char **lineptr, size_t *n, FILE *stream);

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
