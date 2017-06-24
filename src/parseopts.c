/*
 * parseopts.c - option parsing
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE file for copyright and license details.
 */

#include <err.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "parseopts.h"

/* silent linter */
int getopt(int argc, char * const argv[], const char *optstring);
int getopt_long(int argc, char * const argv[], const char *optstring, const struct option *longopts, int *longindex);

extern char *optarg;
extern int optind, opterr, optopt;

static char optstring[] = "hvl:I:o:";

char *const *parse_opts(int *argc, char **argv[], char *optstring)
{
	int opt, libc = 0, incc = 0, ccc = 0;
	char **libs, **inc_dirs;
	char *out_file = NULL;
	char *const cc = "gcc";
	char *const cc_arg_list[] = {
		"-O2", "-pipe", "-Wall", "-Wextra",
		"-pedantic-errors", "-std=c11", "-xc",
		"/dev/stdin", "-o", "/dev/stdout", NULL
	};
	char *const *cc_args;
	char **arg_list;

	/* initilize cc argument list */
	if ((arg_list[ccc++] = malloc(strlen(cc) + 1)) == NULL)
		err(EXIT_FAILURE, "%s", "error during arg_list[0] malloc()");
	memset(arg_list[ccc], 0, strlen(cc) + 1);
	memcpy(arg_list[ccc], cc, strlen(cc) + 1);

	while ((opt = getopt(*argc, *argv, optstring)) != -1) {
		switch (opt) {
		case 'v':
			errx(EXIT_FAILURE, "%s", CEPL_VERSION);
			break;
		case 'l':
			break;
		case 'I':
			break;
		case 'o':
			if (out_file != NULL)
				errx(EXIT_FAILURE, "%s", "too many output files specified");
			break;
		case 'h':
		case '?':
		default:
			errx(EXIT_FAILURE, "usage: %s [-hv] [-l<library>] [-I<include dir>] [-o<sessionlog.c>]", *argv[0]);
		}
	}

	cc_args = arg_list;
	return cc_args;
}
