/*
 * parseopts.c - option parsing
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE file for copyright and license details.
 */

#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "parseopts.h"

/* silence linter */
int getopt(int argc, char * const argv[], const char *optstring);

extern char *optarg;
extern int optind, opterr, optopt;

char *const *parse_opts(int argc, char *argv[], char *optstring, FILE **ofile)
{
	int opt, arg_count = 0;
	char **tmp, **cc_list;
	char *out_file = NULL;
	char *const cc = "gcc";
	char *const cc_arg_list[] = {
		"-O2", "-pipe", "-Wall", "-Wextra",
		"-pedantic-errors", "-std=c11", "-xc",
		"/dev/stdin", "-o", "/dev/stdout", NULL
	};
	char *const *arg_list;

	/* don't print an error if option not found */
	opterr = 0;
	*ofile = NULL;

	/* initilize cc argument list */
	if ((cc_list = malloc((sizeof *cc_list) * ++arg_count)) == NULL)
		err(EXIT_FAILURE, "%s[%d] %s", "error during cc_list", arg_count - 1, "malloc()");
	if ((cc_list[arg_count - 1] = malloc(strlen(cc) + 1)) == NULL)
		err(EXIT_FAILURE, "%s[%d] %s", "error during cc_list", arg_count - 1, "malloc()");
	memset(cc_list[arg_count - 1], 0, strlen(cc) + 1);
	memcpy(cc_list[arg_count - 1], cc, strlen(cc) + 1);

	while ((opt = getopt(argc, argv, optstring)) != -1) {
		switch (opt) {
		case 'v':
			errx(EXIT_FAILURE, "%s", CEPL_VERSION);
			break;
		case 'l':
			if ((tmp = realloc(cc_list, (sizeof *cc_list) * ++arg_count)) == NULL) {
				free(cc_list);
				err(EXIT_FAILURE, "%s[%d] %s", "error during cc_list", arg_count - 1, "malloc()");
			}
			cc_list = tmp;
			if ((cc_list[arg_count - 1] = malloc(strlen(optarg) + 3)) == NULL) {
				free(cc_list);
				err(EXIT_FAILURE, "%s[%d] %s", "error during cc_list", arg_count - 1, "malloc()");
			}
			memset(cc_list[arg_count - 1], 0, strlen(optarg) + 3);
			memcpy(cc_list[arg_count - 1], "-l", 2);
			memcpy(cc_list[arg_count - 1] + 2, optarg, strlen(optarg) + 1);
			break;
		case 'I':
			if ((tmp = realloc(cc_list, (sizeof *cc_list) * ++arg_count)) == NULL) {
				free(cc_list);
				err(EXIT_FAILURE, "%s[%d] %s", "error during cc_list", arg_count - 1, "malloc()");
			}
			cc_list = tmp;
			if ((cc_list[arg_count - 1] = malloc(strlen(optarg) + 3)) == NULL) {
				free(cc_list);
				err(EXIT_FAILURE, "%s[%d] %s", "error during cc_list", arg_count - 1, "malloc()");
			}
			memset(cc_list[arg_count - 1], 0, strlen(optarg) + 3);
			memcpy(cc_list[arg_count - 1], "-I", 2);
			memcpy(cc_list[arg_count - 1] + 2, optarg, strlen(optarg) + 1);
			break;
		case 'o':
			if (out_file != NULL)
				errx(EXIT_FAILURE, "%s", "too many output files specified");
			out_file = optarg;
			if ((*ofile = fopen(out_file, "w")) == NULL)
				err(EXIT_FAILURE, "%s", "failed to create output file");
			break;
		case 'h':
		case '?':
		default:
			errx(EXIT_FAILURE, "usage: %s [-hv] [-l<library>] [-I<include dir>] [-o<sessionlog.c>]", argv[0]);
		}
	}

	/* finalize cc argument list */
	for (int i = 0; cc_arg_list[i]; i++) {
		if ((tmp = realloc(cc_list, (sizeof *cc_list) * ++arg_count)) == NULL) {
			free(cc_list);
			err(EXIT_FAILURE, "%s[%d] %s", "error during cc_list", arg_count - 1, "malloc()");
		}
		cc_list = tmp;
		if ((cc_list[arg_count - 1] = malloc(strlen(cc_arg_list[i]) + 1)) == NULL) {
			free(cc_list);
			err(EXIT_FAILURE, "%s[%d] %s", "error during cc_list", arg_count - 1, "malloc()");
		}
		memset(cc_list[arg_count - 1], 0, strlen(cc_arg_list[i]) + 1);
		memcpy(cc_list[arg_count - 1], cc_arg_list[i], strlen(cc_arg_list[i]) + 1);
	}

	if ((tmp = realloc(cc_list, (sizeof *cc_list) * ++arg_count)) == NULL) {
		free(cc_list);
		err(EXIT_FAILURE, "%s[%d] %s", "error during cc_list", arg_count - 1, "malloc()");
	}
	cc_list = tmp;
	cc_list[arg_count - 1] = NULL;

	arg_list = cc_list;
	return arg_list;
}
