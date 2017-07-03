/*
 * parseopts.c - option parsing
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE file for copyright and license details.
 */

#include "parseopts.h"
#include "readline.h"

/* silence linter */
int getopt(int argc, char *const argv[], const char *optstring);
FILE *fdopen(int fd, char const *mode);
ssize_t getline(char **lineptr, size_t *n, FILE *stream);

/* global toggle flag for warnings and completions */
bool warn_flag = false, perl_flag = false;

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
static char *tmp_arg, *line_ptr = NULL;
static char **tmp_list = NULL, **cc_list = NULL, **lib_list = NULL, **sym_list = NULL;
static int lib_count = 0, arg_count = 0, comp_count = 0, ld_count = 0;

/* global completion list */
extern char **comp_list, *comps[];
/* global linker flag array */
extern char **ld_list;
/* getopts variables */
extern char *optarg;
extern int optind, opterr, optopt;

static inline void init_list(char ***list_ptr, int *count, char *initial_str)
{
	if ((*list_ptr = malloc((sizeof **list_ptr) * ++*count)) == NULL)
		err(EXIT_FAILURE, "%s", "error during initial list_ptr malloc()");
	if ((*(*list_ptr + *count - 1) = malloc(strlen(initial_str) + 1)) == NULL)
		err(EXIT_FAILURE, "%s", "error during initial list_ptr[0] malloc()");
	memset(*(*list_ptr + *count - 1), 0, strlen(initial_str) + 1);
	memcpy(*(*list_ptr + *count - 1), initial_str, strlen(initial_str) + 1);
}

static inline void append_str(char ***list_ptr, int *count, char *str, size_t offset)
{
	char **temp;
	if ((temp = realloc(*list_ptr, (sizeof **list_ptr) * ++*count)) == NULL)
		err(EXIT_FAILURE, "%s[%d] %s", "error during list_ptr", *count - 1, "malloc()");
	*list_ptr = temp;
	if ((*(*list_ptr + *count - 1) = malloc(strlen(str) + offset + 1)) == NULL)
		err(EXIT_FAILURE, "%s[%d]", "error appending string to list_ptr", *count - 1);
	memset(*(*list_ptr + *count - 1), 0, strlen(str) + offset + 1);
	memcpy(*(*list_ptr + *count - 1) + offset, str, strlen(str) + 1);
}

static inline void append_null(char ***list_ptr, int *count)
{
	char **temp;
	if ((temp = realloc(*list_ptr, (sizeof **list_ptr) * ++*count)) == NULL)
		err(EXIT_FAILURE, "%s[%d] %s", "error during NULL delimiter to list_ptr", *count - 1, "malloc()");
	*list_ptr = temp;
	*(*list_ptr + *count - 1) = NULL;
}

char *const *parse_opts(int argc, char *argv[], char *const optstring, FILE **ofile)
{
	int opt;
	char *const *arg_list;
	char *out_file = NULL;

	if (lib_list)
		free_argv(lib_list);
	if (ld_list)
		free_argv(ld_list);

	/* zero variables */
	*ofile = NULL;
	lib_count = 0, arg_count = 0, comp_count = 0, ld_count = 0;
	tmp_list = NULL, cc_list = NULL, lib_list = NULL, sym_list = NULL;

	/* don't print an error if option not found */
	opterr = 0;

	/* initilize argument lists */
	init_list(&cc_list, &arg_count, "gcc");
	init_list(&ld_list, &ld_count, "gcc");
	init_list(&lib_list, &lib_count, "./elfsyms");

	/* re-zero cc_list[0] so -c argument can be added */
	memset(cc_list[0], 0, strlen(cc_list[0]) + 1);

	while ((opt = getopt(argc, argv, optstring)) != -1) {
		switch (opt) {

		/* switch compiler */
		case 'c':
			if (!cc_list[0][0]) {
				/* copy argument to cc_list[0] */
				if ((tmp_arg = realloc(cc_list[0], strlen(optarg) + 1)) == NULL)
					err(EXIT_FAILURE, "%s[%d] %s", "error during initial cc_list", 0, "malloc()");
				cc_list[0] = tmp_arg;
				memset(cc_list[0], 0, strlen(optarg) + 1);
				memcpy(cc_list[0], optarg, strlen(optarg) + 1);
				/* TODO: fix seek errors when not using gcc as ld */
			}
			break;

		/* header directory flag */
		case 'I':
			append_str(&cc_list, &arg_count, optarg, 2);
			memcpy(cc_list[arg_count - 1], "-I", 2);
			break;

		/* dynamic library flag */
		case 'l':
			append_str(&lib_list, &lib_count, optarg, 0);
			append_str(&ld_list, &ld_count, optarg, 2);
			memcpy(ld_list[ld_count - 1], "-l", 2);
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
			errx(EXIT_FAILURE, "%s", CEPL_VERSION);
			break;

		/* usage and unrecognized flags */
		case 'h':
		case '?':
		default:
			errx(EXIT_FAILURE, "Usage: %s %s", argv[0], USAGE);
		}
	}

	/* append warning flags */
	if (warn_flag) {
		for (int i = 0; warn_list[i]; i++)
			append_str(&cc_list, &arg_count, warn_list[i], 0);
	}

	/* default to gcc as a compiler */
	if (!cc_list[0][0])
		memcpy(cc_list[0], "gcc", strlen("gcc") + 1);

	/* finalize argument lists */
	for (int i = 0; cc_arg_list[i]; i++)
		append_str(&cc_list, &arg_count, cc_arg_list[i], 0);
	for (int i = 0; ld_arg_list[i]; i++)
		append_str(&ld_list, &ld_count, ld_arg_list[i], 0);

	/* append NULL to generated lists */
	append_null(&cc_list, &arg_count);
	append_null(&ld_list, &ld_count);
	append_null(&lib_list, &lib_count);

	/* parse ELF shared libraries for completions */
	if (perl_flag) {
		if (comp_list)
			free_argv(comp_list);
		comp_list = malloc(sizeof *comp_list);
		sym_list = parse_libs(lib_list);
		for (int i = 0; comps[i]; i++)
			append_str(&comp_list, &comp_count, comps[i], 0);
		for (int i = 0; sym_list[i]; i++)
			append_str(&comp_list, &comp_count, sym_list[i], 0);
		append_null(&comp_list, &comp_count);
		free(sym_list);
	}

	if (line_ptr) {
		free(line_ptr);
		line_ptr = NULL;
	}

	arg_list = cc_list;
	return arg_list;
}

char **parse_libs(char *libs[]) {
	int status, i = 0;
	int pipe_nm[2];
	char **tokens, **tmp;
	FILE *nm_input;
	size_t line_size = 0;

	if (line_ptr) {
		free(line_ptr);
		line_ptr = NULL;
	}

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

