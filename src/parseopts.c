/*
 * parseopts.c - option parsing
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE file for copyright and license details.
 */

#include "parseopts.h"
#include "readline.h"

/* silence linter */
int getopt(int argc, char * const argv[], const char *optstring);
FILE *fdopen(int fd, const char *mode);
ssize_t getline(char **lineptr, size_t *n, FILE *stream);

/* global toggle flag for warnings and completions */
bool warn_flag = false, perl_flag = false;

static char *line_ptr = NULL;
static char *const cc_arg_list[] = {
	"-O2", "-pipe", "-std=c11", "-S", "-xc",
	"/proc/self/fd/0", "-o/proc/self/fd/1", NULL
};
static char *const ld_arg_list[] = {
	"-xassembler", "/proc/self/fd/0",
	"-o/proc/self/fd/1", NULL
};
static char *const warn_list[] = {
	"-pedantic-errors", "-Wall", "-Wextra", NULL
};
static char **tmp = NULL, **cc_list = NULL, **lib_list = NULL, **sym_list = NULL;
static int lib_count = 0, arg_count = 0, comp_count = 0, ld_count = 0;

/* global completion list */
extern char **comp_list, *comps[];
/* global linker flag array */
extern char **ld_list;
/* getopts variables */
extern char *optarg;
extern int optind, opterr, optopt;

static inline void append_warnings(void)
{
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
}

static inline void append_symbols(void)
{
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
	if ((tmp = realloc(comp_list, (sizeof *comp_list) * ++comp_count)) == NULL) {
		free(comp_list);
		err(EXIT_FAILURE, "%s[%d] %s", "error during NULL delimiter comp_list", comp_count - 1, "malloc()");
	}
	comp_list = tmp;
	comp_list[comp_count - 1] = NULL;
}

char *const *parse_opts(int argc, char *argv[], char *const optstring, FILE **ofile)
{
	int opt;
	char *const *arg_list;
	char *const gcc = "gcc";
	char *const ccc = "clang";
	char *const icc = "icc";
	char *out_file = NULL;

	/* don't print an error if option not found */
	opterr = 0;
	/* initialize variables */
	*ofile = NULL;
	lib_count = 0, arg_count = 0, comp_count = 0, ld_count = 0;
	tmp = NULL, cc_list = NULL, lib_list = NULL, sym_list = NULL;

	if ((lib_list = malloc(sizeof *lib_list)) == NULL)
		err(EXIT_FAILURE, "%s", "error duing lib_list malloc()");
	if ((lib_list[lib_count++] = malloc(strlen("cepl") + 1)) == NULL)
		err(EXIT_FAILURE, "%s", "error duing lib_list malloc()");
	memset(lib_list[lib_count - 1], 0, strlen("cepl") + 1);
	memcpy(lib_list[lib_count - 1], "cepl", strlen("cepl") + 1);

	/* initilize cc argument list with enough space for the longest compiler string (clang) */
	if ((cc_list = malloc((sizeof *cc_list) * ++arg_count)) == NULL)
		err(EXIT_FAILURE, "%s[%d] %s", "error during initial cc_list", arg_count - 1, "malloc()");
	if ((cc_list[arg_count - 1] = malloc(strlen(ccc) + 1)) == NULL)
		err(EXIT_FAILURE, "%s[%d] %s", "error during initial cc_list", arg_count - 1, "malloc()");
	memset(cc_list[arg_count - 1], 0, strlen(ccc) + 1);

	/* initilize ld argument list */
	if (ld_list)
		free_argv(ld_list);
	if ((ld_list = malloc((sizeof *ld_list) * ++ld_count)) == NULL)
		err(EXIT_FAILURE, "%s[%d] %s", "error during initial ld_list", ld_count - 1, "malloc()");
	if ((ld_list[ld_count - 1] = malloc(strlen(gcc) + 1)) == NULL)
		err(EXIT_FAILURE, "%s[%d] %s", "error during initial ld_list", ld_count - 1, "malloc()");
	memset(ld_list[ld_count - 1], 0, strlen(gcc) + 1);
	memcpy(ld_list[ld_count - 1], gcc, strlen(gcc) + 1);

	while ((opt = getopt(argc, argv, optstring)) != -1) {
		switch (opt) {

		/* switch compiler to clang */
		case 'c':
			if (!cc_list[0][0])
				memcpy(cc_list[0], ccc, strlen(ccc) + 1);
			break;

		/* switch compiler to icc */
		case 'i':
			if (!cc_list[0][0])
				memcpy(cc_list[0], icc, strlen(icc) + 1);
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

		/* dynamic library flag */
		case 'l':
			if ((tmp = realloc(ld_list, (sizeof *ld_list) * ++ld_count)) == NULL) {
				free(ld_list);
				err(EXIT_FAILURE, "%s[%d] %s", "error during library ld_list", ld_count - 1, "malloc()");
			}
			ld_list = tmp;
			if ((ld_list[ld_count - 1] = malloc(strlen(optarg) + 3)) == NULL) {
				free(ld_list);
				err(EXIT_FAILURE, "%s[%d] %s", "error during library ld_list", ld_count - 1, "malloc()");
			}
			memset(ld_list[ld_count - 1], 0, strlen(optarg) + 3);
			memcpy(ld_list[ld_count - 1], "-l", 2);
			memcpy(ld_list[ld_count - 1] + 2, optarg, strlen(optarg) + 1);

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

		/* output file flag */
		case 'o':
			if (out_file != NULL)
				errx(EXIT_FAILURE, "%s", "too many output files specified");
			out_file = optarg;
			if ((*ofile = fopen(out_file, "w")) == NULL)
				err(EXIT_FAILURE, "%s", "failed to create output file");
			break;

		/* perl flag */
		case 'p':
			perl_flag ^= true;
			break;

		/* warning flag */
		case 'w':
			warn_flag ^= true;
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
			errx(EXIT_FAILURE, "Usage: %s %s", argv[0], USAGE);
		}
	}

	/* append NULL to lib_list */
	if ((tmp = realloc(lib_list, (sizeof *lib_list) * ++lib_count)) == NULL) {
		free(lib_list);
		err(EXIT_FAILURE, "%s[%d] %s", "error during library lib_list", lib_count - 1, "malloc()");
	}
	lib_list = tmp;
	lib_list[lib_count - 1] = NULL;

	/* default to gcc as a compiler */
	if (!cc_list[0][0])
		memcpy(cc_list[0], gcc, strlen(gcc) + 1);

	/* append warning flags */
	if (warn_flag)
		append_warnings();

	/* parse ELF library for completions */
	if (perl_flag) {
		if (comp_list)
			free_argv(comp_list);
		sym_list = parse_libs(lib_list);
		comp_list = malloc(sizeof *comp_list);
		append_symbols();
		if (sym_list)
			free(sym_list);
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
	/* append NULL to cc_list */
	if ((tmp = realloc(cc_list, (sizeof *cc_list) * ++arg_count)) == NULL) {
		free(cc_list);
		err(EXIT_FAILURE, "%s[%d] %s", "error during NULL delimiter cc_list", arg_count - 1, "malloc()");
	}
	cc_list = tmp;
	cc_list[arg_count - 1] = NULL;

	/* finalize ld argument list */
	for (int i = 0; ld_arg_list[i]; i++) {
		if ((tmp = realloc(ld_list, (sizeof *ld_list) * ++ld_count)) == NULL) {
			free(ld_list);
			err(EXIT_FAILURE, "%s[%d] %s", "error during final ld_list", ld_count - 1, "malloc()");
		}
		ld_list = tmp;
		if ((ld_list[ld_count - 1] = malloc(strlen(ld_arg_list[i]) + 1)) == NULL) {
			free(ld_list);
			err(EXIT_FAILURE, "%s[%d] %s", "error during final ld_list", ld_count - 1, "malloc()");
		}
		memset(ld_list[ld_count - 1], 0, strlen(ld_arg_list[i]) + 1);
		memcpy(ld_list[ld_count - 1], ld_arg_list[i], strlen(ld_arg_list[i]) + 1);
	}
	/* append NULL to ld_list */
	if ((tmp = realloc(ld_list, (sizeof *ld_list) * ++ld_count)) == NULL) {
		free(ld_list);
		err(EXIT_FAILURE, "%s[%d] %s", "error during NULL delimiter ld_list", ld_count - 1, "malloc()");
	}
	ld_list = tmp;
	ld_list[ld_count - 1] = NULL;

	if (line_ptr) {
		free(line_ptr);
		line_ptr = NULL;
	}
	if (free_argv(lib_list) == -1)
		warn("%s", "error freeing lib_list");
	lib_list = NULL;

	arg_list = cc_list;
	return arg_list;
}

char **parse_libs(char *libs[]) {
	int status, i = 0;
	int pipe_nm[2];
	char **tokens, **tmp;
	FILE *nm_input;
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
		execv(ELF_SCRIPT, libs);
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

		getline(&line_ptr, &line_size, nm_input);
		fclose(nm_input);
		close(pipe_nm[0]);

		if ((tokens = malloc(sizeof *tokens)) == NULL)
			err(EXIT_FAILURE, "%s", "error during parse_libs() tokens malloc()");
		tokens[i++] = strtok(line_ptr, " \t\n");
		if ((tmp = realloc(tokens, (sizeof *tokens) * ++i)) == NULL) {
			free(tokens);
			err(EXIT_FAILURE, "%s", "error during parse_libs() tmp malloc()");
		}
		tokens = tmp;

		while ((tokens[i - 1] = strtok(NULL, " \t\n")) != NULL) {
			if ((tmp = realloc(tokens, (sizeof *tokens) * ++i)) == NULL) {
				free(tokens);
				err(EXIT_FAILURE, "%s", "error during parse_libs() tmp malloc()");
			}
			tokens = tmp;
		}

		return tokens;
	}
}

