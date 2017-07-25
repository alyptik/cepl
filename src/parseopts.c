/*
 * parseopts.c - option parsing
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#include <getopt.h>
#include "parseopts.h"
#include "readline.h"

/* silence linter */
int getopt_long(int argc, char *const argv[], char const *optstring, struct option const *longopts, int *longindex);
FILE *fdopen(int fd, char const *mode);
ssize_t getline(char **lineptr, size_t *n, FILE *stream);

/* global toggle flag for warnings and completions */
bool warn_flag = false, parse_flag = false;

static struct option long_opts[] = {
	{"help", no_argument, 0, 'h'},
	{"parse", no_argument, 0, 'p'},
	{"version", no_argument, 0, 'v'},
	{"warnings", no_argument, 0, 'w'},
	{"compiler", required_argument, 0, 'c'},
	{0, 0, 0, 0}
};
static char *const cc_arg_list[] = {
	"-O0", "-pipe", "-fPIC", "-std=c11",
	"-S", "-xc", "/proc/self/fd/0",
	"-o", "/proc/self/fd/1", NULL
};
static char *const ld_arg_list[] = {
	"-O0", "-pipe", "-fPIC", "-std=c11",
	"-xassembler", "/proc/self/fd/0",
	"-o", "/proc/self/fd/1", NULL
};
static char *const warn_list[] = {
	"-pedantic-errors", "-Wall", "-Wextra", NULL
};
static int option_index = 0;
static char *tmp_arg, *line_ptr = NULL;
static char **tmp_list = NULL, **sym_list = NULL;
/* compiler arguments and library list structs */
static struct str_list cc_list = {.cnt = 0, .list = NULL}, lib_list = {.cnt = 0, .list = NULL};

/* getopts variables */
extern char *optarg;
extern int optind, opterr, optopt;
/* global completion list */
extern char *comp_arg_list[];
/* global linker flags and completions structs */
extern struct str_list ld_list, comp_list;

static inline void init_list(struct str_list *argv, char *initial_str)
{
	if ((argv->list = malloc((sizeof *argv->list) * ++argv->cnt)) == NULL)
		err(EXIT_FAILURE, "%s", "error during initial list_ptr malloc()");
	if ((*(argv->list + argv->cnt - 1) = malloc(strlen(initial_str) + 1)) == NULL)
		err(EXIT_FAILURE, "%s", "error during initial list_ptr[0] malloc()");
	memset(*(argv->list + argv->cnt - 1), 0, strlen(initial_str) + 1);
	memcpy(*(argv->list + argv->cnt - 1), initial_str, strlen(initial_str) + 1);
}

static inline void append_str(struct str_list *argv, char *str, size_t offset)
{
	char **temp;
	if ((temp = realloc(argv->list, (sizeof *argv->list) * ++argv->cnt)) == NULL)
		err(EXIT_FAILURE, "%s[%d] %s", "error during list_ptr", argv->cnt - 1, "malloc()");
	argv->list = temp;
	if (!str) {
		*(argv->list + argv->cnt - 1) = NULL;
	} else {
		if ((*(argv->list + argv->cnt - 1) = malloc(strlen(str) + offset + 1)) == NULL)
			err(EXIT_FAILURE, "%s[%d]", "error appending string to list_ptr", argv->cnt - 1);
		memset(*(argv->list + argv->cnt - 1), 0, strlen(str) + offset + 1);
		memcpy(*(argv->list + argv->cnt - 1) + offset, str, strlen(str) + 1);
	}
}

char **parse_opts(int argc, char *argv[], char const optstring[], FILE **ofile)
{
	int opt;
	char *out_file = NULL;

	if (lib_list.list)
		free_argv(lib_list.list);
	if (ld_list.list)
		free_argv(ld_list.list);

	*ofile = NULL;
	lib_list.cnt = 0, cc_list.cnt = 0, comp_list.cnt = 0, ld_list.cnt = 0;
	tmp_list = NULL, sym_list = NULL;
	cc_list.list = NULL, lib_list.list = NULL;
	/* don't print an error if option not found */
	opterr = 0;

	/* initilize argument lists */
	init_list(&cc_list, "gcc");
	init_list(&ld_list, "gcc");
	init_list(&lib_list, "./elfsyms");
	/* re-zero cc_list.list[0] so -c argument can be added */
	memset(cc_list.list[0], 0, strlen(cc_list.list[0]) + 1);

	/* TODO: fix seek errors when not using gcc as ld */

	while ((opt = getopt_long(argc, argv, optstring, long_opts, &option_index)) != -1) {
		switch (opt) {

		/* switch compiler */
		case 'c':
			if (!cc_list.list[0][0]) {
				/* copy argument to cc_list.list[0] */
				if ((tmp_arg = realloc(cc_list.list[0], strlen(optarg) + 1)) == NULL)
					err(EXIT_FAILURE, "%s[%d] %s", "error during initial cc_list.list", 0, "malloc()");
				cc_list.list[0] = tmp_arg;
				memset(cc_list.list[0], 0, strlen(optarg) + 1);
				memcpy(cc_list.list[0], optarg, strlen(optarg) + 1);
			}
			break;

		/* header directory flag */
		case 'I':
			append_str(&cc_list, optarg, 2);
			memcpy(cc_list.list[cc_list.cnt - 1], "-I", 2);
			break;

		/* dynamic library flag */
		case 'l':
			append_str(&lib_list, optarg, 0);
			append_str(&ld_list, optarg, 2);
			memcpy(ld_list.list[ld_list.cnt - 1], "-l", 2);
			break;

		/* output file flag */
		case 'o':
			if (out_file != NULL)
				errx(EXIT_FAILURE, "%s", "too many output files specified");
			out_file = optarg;
			if ((*ofile = fopen(out_file, "w")) == NULL)
				err(EXIT_FAILURE, "%s", "failed to create output file");
			break;

		/* parse flag */
		case 'p':
			parse_flag ^= true;
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
		for (register int i = 0; warn_list[i]; i++)
			append_str(&cc_list, warn_list[i], 0);
	}

	/* default to gcc as a compiler */
	if (!cc_list.list[0][0])
		memcpy(cc_list.list[0], "gcc", strlen("gcc") + 1);

	/* finalize argument lists */
	for (register int i = 0; cc_arg_list[i]; i++)
		append_str(&cc_list, cc_arg_list[i], 0);
	for (register int i = 0; ld_arg_list[i]; i++)
		append_str(&ld_list, ld_arg_list[i], 0);

	/* append NULL to generated lists */
	append_str(&cc_list, NULL, 0);
	append_str(&ld_list, NULL, 0);
	append_str(&lib_list, NULL, 0);

	/* parse ELF shared libraries for completions */
	if (parse_flag) {
		if (comp_list.list)
			free_argv(comp_list.list);
		comp_list.list = malloc(sizeof *comp_list.list);
		sym_list = parse_libs(lib_list.list);
		for (register int i = 0; comp_arg_list[i]; i++)
			append_str(&comp_list, comp_arg_list[i], 0);
		for (register int i = 0; sym_list[i]; i++)
			append_str(&comp_list, sym_list[i], 0);
		append_str(&comp_list, NULL, 0);
		free(sym_list);
	}

	if (line_ptr) {
		free(line_ptr);
		line_ptr = NULL;
	}

	return cc_list.list;
}

char **parse_libs(char *libs[]) {
	int status;
	int pipe_nm[2];
	char **tokens, **tmp;
	FILE *nm_input;
	size_t line_size = 0;
	register int token_count = 0;

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
		wait(&status);
		if (status != 0) {
			warnx("%s", "nm non-zero exit code");
			return NULL;
		}

		nm_input = fdopen(pipe_nm[0], "r");
		getline(&line_ptr, &line_size, nm_input);
		fclose(nm_input);
		close(pipe_nm[0]);

		if ((tokens = malloc((sizeof *tokens) * ++token_count)) == NULL)
			err(EXIT_FAILURE, "%s", "error during parse_libs() tokens malloc()");
		tokens[token_count - 1] = strtok(line_ptr, " \t\n");
		if ((tmp = realloc(tokens, (sizeof *tokens) * ++token_count)) == NULL) {
			free(tokens);
			err(EXIT_FAILURE, "%s", "error during parse_libs() tmp malloc()");
		}
		tokens = tmp;

		while ((tokens[token_count - 1] = strtok(NULL, " \t\n")) != NULL) {
			if ((tmp = realloc(tokens, (sizeof *tokens) * ++token_count)) == NULL) {
				free(tokens);
				err(EXIT_FAILURE, "%s", "error during parse_libs() tmp malloc()");
			}
			tokens = tmp;
		}

		return tokens;
	}
}

