/*
 * parseopts.c - option parsing
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE file for copyright and license details.
 */

#include <err.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "parseopts.h"
#include "readline.h"

/* silence linter */
int getopt(int argc, char * const argv[], const char *optstring);
FILE *fdopen(int fd, const char *mode);
ssize_t getline(char **lineptr, size_t *n, FILE *stream);

extern char *optarg;
extern int optind, opterr, optopt;
extern char **comp_list, *comps[];

char *const *parse_opts(int argc, char *argv[], char *optstring, FILE **ofile)
{
	int opt, arg_count = 0, lib_count = 0, comp_count = 0;
	char **tmp, **cc_list, **sym_list;
	char *out_file = NULL;
	char *const cc = "gcc";
	char *const cc_arg_list[] = {
		"-O2", "-pipe", "-std=c11", "-xc",
		"/dev/stdin", "-o", "/dev/stdout", NULL
	};
	bool warn_flag = false, perl_flag = false;
	char *const warn_list[] = {
		"-pedantic-errors", "-Wall", "-Wextra", NULL
	};
	char *const *arg_list;
	char **lib_list = malloc(sizeof *lib_list);
	lib_list[lib_count++] = "cepl";

	/* don't print an error if option not found */
	opterr = 0;
	*ofile = NULL;

	/* initilize cc argument list */
	if ((cc_list = malloc((sizeof *cc_list) * ++arg_count)) == NULL)
		err(EXIT_FAILURE, "%s[%d] %s", "error during initial cc_list", arg_count - 1, "malloc()");
	if ((cc_list[arg_count - 1] = malloc(strlen(cc) + 1)) == NULL)
		err(EXIT_FAILURE, "%s[%d] %s", "error during initial cc_list", arg_count - 1, "malloc()");
	memset(cc_list[arg_count - 1], 0, strlen(cc) + 1);
	memcpy(cc_list[arg_count - 1], cc, strlen(cc) + 1);

	while ((opt = getopt(argc, argv, optstring)) != -1) {
		switch (opt) {
		/* dynamic library flag */
		case 'l':
			if ((tmp = realloc(cc_list, (sizeof *cc_list) * ++arg_count)) == NULL) {
				free(cc_list);
				err(EXIT_FAILURE, "%s[%d] %s", "error during library cc_list", arg_count - 1, "malloc()");
			}
			cc_list = tmp;
			if ((cc_list[arg_count - 1] = malloc(strlen(optarg) + 3)) == NULL) {
				free(cc_list);
				err(EXIT_FAILURE, "%s[%d] %s", "error during library cc_list", arg_count - 1, "malloc()");
			}
			memset(cc_list[arg_count - 1], 0, strlen(optarg) + 3);
			memcpy(cc_list[arg_count - 1], "-l", 2);
			memcpy(cc_list[arg_count - 1] + 2, optarg, strlen(optarg) + 1);

			if ((tmp = realloc(lib_list, (sizeof *lib_list) * ++lib_count)) == NULL) {
				free(lib_list);
				err(EXIT_FAILURE, "%s[%d] %s", "error during library lib_list", lib_count - 1, "malloc()");
			}
			lib_list = tmp;
			if ((lib_list[lib_count - 1] = malloc(strlen(optarg) + 1)) == NULL) {
				free(lib_list);
				err(EXIT_FAILURE, "%s[%d] %s", "error during library lib_list", lib_count - 1, "malloc()");
			}
			memset(lib_list[lib_count - 1], 0, strlen(optarg) + 1);
			memcpy(lib_list[lib_count - 1], optarg, strlen(optarg) + 1);
			break;

		/* header directory flag */
		case 'I':
			if ((tmp = realloc(cc_list, (sizeof *cc_list) * ++arg_count)) == NULL) {
				free(cc_list);
				err(EXIT_FAILURE, "%s[%d] %s", "error during header cc_list", arg_count - 1, "malloc()");
			}
			cc_list = tmp;
			if ((cc_list[arg_count - 1] = malloc(strlen(optarg) + 3)) == NULL) {
				free(cc_list);
				err(EXIT_FAILURE, "%s[%d] %s", "error during header cc_list", arg_count - 1, "malloc()");
			}
			memset(cc_list[arg_count - 1], 0, strlen(optarg) + 3);
			memcpy(cc_list[arg_count - 1], "-I", 2);
			memcpy(cc_list[arg_count - 1] + 2, optarg, strlen(optarg) + 1);
			break;

		/* output file flag */
		case 'o':
			if (out_file != NULL)
				errx(EXIT_FAILURE, "%s", "too many output files specified");
			out_file = optarg;
			if ((*ofile = fopen(out_file, "w")) == NULL)
				err(EXIT_FAILURE, "%s", "failed to create output file");
			break;

		/* warning flag */
		case 'w':
			/* break early if already set */
			if (warn_flag)
				break;
			for (int i = 0; warn_list[i]; i++) {
				if ((tmp = realloc(cc_list, (sizeof *cc_list) * ++arg_count)) == NULL) {
					free(cc_list);
					err(EXIT_FAILURE, "%s[%d] %s", "error during warning cc_list", arg_count - 1, "malloc()");
				}
				cc_list = tmp;
				if ((cc_list[arg_count - 1] = malloc(strlen(warn_list[i]) + 1)) == NULL) {
					free(cc_list);
					err(EXIT_FAILURE, "%s[%d] %s", "error during warning cc_list", arg_count - 1, "malloc()");
				}
				memset(cc_list[arg_count - 1], 0, strlen(warn_list[i]) + 1);
				memcpy(cc_list[arg_count - 1], warn_list[i], strlen(warn_list[i]) + 1);
			}
			warn_flag = true;
			break;

		/* perl flag */
		case 'p':
			perl_flag = true;
			break;

		/* version flag */
		case 'v':
			free(cc_list[arg_count - 1]);
			free(cc_list);
			errx(EXIT_FAILURE, "%s", CEPL_VERSION);
			break;

		/* usage and unrecognized flags */
		case 'h':
		case '?':
		default:
			free(cc_list[arg_count - 1]);
			free(cc_list);
			errx(EXIT_FAILURE, USAGE, argv[0]);
		}
	}

	/* finalize cc argument list */
	for (int i = 0; cc_arg_list[i]; i++) {
		if ((tmp = realloc(cc_list, (sizeof *cc_list) * ++arg_count)) == NULL) {
			free(cc_list);
			err(EXIT_FAILURE, "%s[%d] %s", "error during final cc_list", arg_count - 1, "malloc()");
		}
		cc_list = tmp;
		if ((cc_list[arg_count - 1] = malloc(strlen(cc_arg_list[i]) + 1)) == NULL) {
			free(cc_list);
			err(EXIT_FAILURE, "%s[%d] %s", "error during final cc_list", arg_count - 1, "malloc()");
		}
		memset(cc_list[arg_count - 1], 0, strlen(cc_arg_list[i]) + 1);
		memcpy(cc_list[arg_count - 1], cc_arg_list[i], strlen(cc_arg_list[i]) + 1);
	}

	if ((tmp = realloc(cc_list, (sizeof *cc_list) * ++arg_count)) == NULL) {
		free(cc_list);
		err(EXIT_FAILURE, "%s[%d] %s", "error during NULL delimiter cc_list", arg_count - 1, "malloc()");
	}
	cc_list = tmp;
	cc_list[arg_count - 1] = NULL;

	if ((tmp = realloc(lib_list, (sizeof *lib_list) * ++lib_count)) == NULL) {
		free(lib_list);
		err(EXIT_FAILURE, "%s[%d] %s", "error during library lib_list", lib_count - 1, "malloc()");
	}
	lib_list = tmp;
	lib_list[lib_count - 1] = NULL;
	sym_list = parse_libs(lib_list);

	if (perl_flag) {
		for (int i = 0; comps[i]; i++) {
			if ((tmp = realloc(comp_list, (sizeof *comp_list) * ++comp_count)) == NULL) {
				free(comp_list);
				err(EXIT_FAILURE, "%s[%d] %s", "error during comp_list", comp_count - 1, "malloc()");
			}
			comp_list = tmp;

			if ((comp_list[comp_count - 1] = malloc(strlen(comps[i]) + 1)) == NULL) {
				free(comp_list);
				err(EXIT_FAILURE, "%s[%d] %s", "error during comp_list", comp_count - 1, "malloc()");
			}
			memset(comp_list[comp_count - 1], 0, strlen(comps[i]) + 1);
			memcpy(comp_list[comp_count - 1], comps[i], strlen(comps[i]) + 1);
		}

		for (int j = 0; sym_list[j]; j++) {
			if ((tmp = realloc(comp_list, (sizeof *comp_list) * ++comp_count)) == NULL) {
				free(comp_list);
				err(EXIT_FAILURE, "%s[%d] %s", "error during library comp_list", arg_count - 1, "malloc()");
			}
			comp_list = tmp;

			if ((comp_list[comp_count - 1] = malloc(strlen(sym_list[j]) + 1)) == NULL) {
				free(comp_list);
				err(EXIT_FAILURE, "%s[%d] %s", "error during comp_list", comp_count - 1, "malloc()");
			}
			memset(comp_list[comp_count - 1], 0, strlen(sym_list[j]) + 1);
			memcpy(comp_list[comp_count - 1], sym_list[j], strlen(sym_list[j]) + 1);
		}
	}

	if ((tmp = realloc(comp_list, (sizeof *comp_list) * ++comp_count)) == NULL) {
		free(comp_list);
		err(EXIT_FAILURE, "%s[%d] %s", "error during NULL delimiter comp_list", comp_count - 1, "malloc()");
	}
	comp_list = tmp;

	comp_list[comp_count - 1] = NULL;
	arg_list = cc_list;
	return arg_list;
}

char **parse_libs(char *libs[]) {
	int status, i = 0;
	int pipe_nm[2];
	char **tokens, **tmp;
	FILE *nm_input;
	char *input_line = NULL;
	size_t line_size = 0;

	pipe(pipe_nm);

	/* fork nm */
	switch (fork()) {
	/* error */
	case -1:
		close(pipe_nm[0]);
		close(pipe_nm[1]);
		err(EXIT_FAILURE, "%s", "error forking nm");
		break;

	/* child */
	case 0:
		close(pipe_nm[0]);
		dup2(pipe_nm[1], 1);
		execv("./elfsyms.pl", libs);
		/* execv() should never return */
		err(EXIT_FAILURE, "%s", "error forking nm");
		break;

	/* parent */
	default:
		close(pipe_nm[1]);
		nm_input = fdopen(pipe_nm[0], "r");
		wait(&status);
		if (status != 0) {
			warnx("%s", "nm non-zero exit code");
			return NULL;
		}
		getline(&input_line, &line_size, nm_input);
		fclose(nm_input);
		close(pipe_nm[0]);
		if ((tokens = malloc(sizeof *tokens)) == NULL)
			err(EXIT_FAILURE, "%s", "error during parse_libs() tokens malloc()");
		tokens[i++] = strtok(input_line, " \n");

		for (; (tokens[i - 1] = strtok(NULL, " \n")); i++) {
			if ((tmp = realloc(tokens, (sizeof *tokens) * (i + 1))) == NULL) {
				free(tokens);
				err(EXIT_FAILURE, "%s", "error during parse_libs() tmp malloc()");
			}
			tokens = tmp;
		}
		tokens[i - 1] = NULL;

		return tokens;
	}
}
