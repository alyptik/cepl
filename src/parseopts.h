/*
 * parseopts.h - option parsing
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE file for copyright and license details.
 */

#ifndef PARSEOPTS_H
#define PARSEOPTS_H

#include <stdio.h>
#define CEPL_VERSION "CEPL v0.1.6"

char *const *parse_opts(int argc, char *argv[], char *optstring, FILE **ofile);

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
