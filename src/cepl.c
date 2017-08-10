/*
 * cepl.c - REPL translation unit
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#ifndef _GNU_SOURCE
#	define _GNU_SOURCE
#endif

#include <sys/types.h>
#include "compile.h"
#include "readline.h"
#include "parseopts.h"

/* struct definition for generated program sources */
struct prog_src {
	char *funcs;
	char *body;
	char *final;
	struct str_list history;
	struct flag_list flags;
};

/* truncated source */
static struct prog_src user = {
	.funcs = NULL,
	.body = NULL,
	.final = NULL,
	.history = {.cnt = 0, .list = NULL},
	.flags = {.cnt = 0, .list = NULL},
};

/* actual source */
static struct prog_src actual = {
	.funcs = NULL,
	.body = NULL,
	.final = NULL,
	.history = {.cnt = 0, .list = NULL},
	.flags = {.cnt = 0, .list = NULL},
};

/* source file templates */
static char const prog_includes[] = "#define _BSD_SOURCE\n#define _DEFAULT_SOURCE\n#define _GNU_SOURCE\n#define _POSIX_C_SOURCE 200809L\n#define _SVID_SOURCE\n#define _XOPEN_SOURCE 700\n\n#include <assert.h>\n#include <ctype.h>\n#include <err.h>\n#include <errno.h>\n#include <error.h>\n#include <fcntl.h>\n#include <limits.h>\n#include <math.h>\n#include <regex.h>\n#include <signal.h>\n#include <stdalign.h>\n#include <stdarg.h>\n#include <stdbool.h>\n#include <stddef.h>\n#include <stdint.h>\n#include <stdio.h>\n#include <stdlib.h>\n#include <stdnoreturn.h>\n#include <string.h>\n#include <strings.h>\n#include <time.h>\n#include <uchar.h>\n#include <wchar.h>\n#include <unistd.h>\n#include <linux/memfd.h>\n#include <sys/mman.h>\n#include <sys/types.h>\n#include <sys/syscall.h>\n#include <sys/wait.h>\n\n#define _Atomic\n#define _Static_assert(a, b)\n#define UNUSED __attribute__ ((unused))\n\nextern char **environ;\n";
static char const prog_start[] = "\n\nint main(int argc UNUSED, char *argv[] UNUSED)\n{\n";
static char const prog_end[] = "\n\treturn 0;\n}\n";
/* line and token buffers */
static char *line;
static char *tok_buf;
/* output file */
static FILE *ofile;
/* compiler arg array */
static char **cc_argv;

/* completion list of generated symbols */
extern struct str_list comp_list;
/* toggle flag for warnings and completions */
extern bool warn_flag, parse_flag, out_flag;

static inline void free_buffers(void)
{
	/* write out program to file if applicable */
	if (ofile) {
		fwrite(actual.final, strlen(actual.final), 1, ofile);
		fputc('\n', ofile);
		fclose(ofile);
		ofile = NULL;
	}
	if (user.funcs)
		free(user.funcs);
	if (actual.funcs)
		free(actual.funcs);
	if (user.body)
		free(user.body);
	if (actual.body)
		free(actual.body);
	if (user.final)
		free(user.final);
	if (actual.final)
		free(actual.final);
	if (user.flags.list)
		free(user.flags.list);
	if (actual.flags.list)
		free(actual.flags.list);
	/* free char ** vectors */
	if (user.history.list) {
		append_str(&user.history, NULL, 0);
		free_argv(user.history.list);
	}
	if (actual.history.list) {
		append_str(&actual.history, NULL, 0);
		free_argv(actual.history.list);
	}
	if (cc_argv)
		free_argv(cc_argv);
	user.body = NULL;
	actual.body = NULL;
	user.final = NULL;
	actual.final = NULL;
	user.history.list = NULL;
	actual.history.list = NULL;
	user.flags.list = NULL;
	actual.flags.list = NULL;
	cc_argv = NULL;
	user.history.cnt = 0;
	actual.history.cnt = 0;
	user.flags.cnt = 0;
	actual.flags.cnt = 0;
}

static inline void init_buffers(void)
{
	user.funcs = malloc(1);
	actual.funcs = malloc(strlen(prog_includes) + 1);
	user.body = malloc(strlen(prog_start) + 1);
	actual.body = malloc(strlen(prog_start) + 1);
	user.final = malloc(strlen(prog_includes) + strlen(prog_start) + strlen(prog_end) + 3);
	actual.final = malloc(strlen(prog_includes) + strlen(prog_start) + strlen(prog_end) + 3);
	/* sanity check */
	if (!user.funcs || !actual.funcs || !user.body || !actual.body || !user.final || !actual.final) {
		free_buffers();
		if (line)
			free(line);
		if (comp_list.list)
			free_argv(comp_list.list);
		err(EXIT_FAILURE, "error allocating initial pointers at %d", __LINE__);
	}
	/* zero source buffers */
	memset(user.funcs, 0, 1);
	memset(actual.funcs, 0, strlen(prog_includes) + 1);
	memset(user.body, 0, strlen(prog_start) + 1);
	memset(actual.body, 0, strlen(prog_start) + 1);
	memset(user.final, 0, strlen(prog_includes) + strlen(prog_start) + strlen(prog_end) + 3);
	memset(actual.final, 0, strlen(prog_includes) + strlen(prog_start) + strlen(prog_end) + 3);
	/* no memcpy for user.funcs */
	memcpy(actual.funcs, prog_includes, strlen(prog_includes) + 1);
	memcpy(user.body, prog_start, strlen(prog_start) + 1);
	memcpy(actual.body, prog_start, strlen(prog_start) + 1);
	/* init source history and flag lists */
	init_list(&user.history, "cepl");
	init_list(&actual.history, "cepl");
	init_flag_list(&user.flags);
	init_flag_list(&actual.flags);
}

static inline void resize_buffer(char **buf, size_t offset)
{
	char *tmp;
	/* current length + line length + extra characters + \0 */
	if ((tmp = realloc(*buf, strlen(*buf) + strlen(line) + offset + 1)) == NULL) {
		free_buffers();
		if (line)
			free(line);
		if (comp_list.list)
			free_argv(comp_list.list);
		err(EXIT_FAILURE, "error during resize_buffer() at line %d", __LINE__);
	}
	*buf = tmp;
}

static inline void build_funcs(void)
{
	append_str(&user.history, user.funcs, 0);
	append_str(&actual.history, actual.funcs, 0);
	append_flag(&user.flags, NOT_IN_MAIN);
	append_flag(&actual.flags, NOT_IN_MAIN);
	/* generate function buffers */
	strcat(user.funcs, tok_buf);
	strcat(actual.funcs, tok_buf);
	strcat(user.funcs, "\n");
	strcat(actual.funcs, "\n");
}

static inline void build_body(void)
{
	append_str(&user.history, user.body, 0);
	append_str(&actual.history, actual.body, 0);
	append_flag(&user.flags, IN_MAIN);
	append_flag(&actual.flags, IN_MAIN);
	strcat(user.body, "\t");
	strcat(actual.body, "\t");
	strcat(user.body, strtok(line, "\0\n"));
	strcat(actual.body, strtok(line, "\0\n"));
}

static inline void build_final(void)
{
	/* finish building current iteration of source code */
	memcpy(user.final, user.funcs, strlen(user.funcs) + 1);
	memcpy(actual.final, actual.funcs, strlen(actual.funcs) + 1);
	strcat(user.final, user.body);
	strcat(actual.final, actual.body);
	strcat(user.final, prog_end);
	strcat(actual.final, prog_end);
}

static inline void undo(struct prog_src *prog)
{
	switch(prog->flags.list[prog->flags.cnt - 1]) {
	case NOT_IN_MAIN:
		prog->flags.cnt--;
		prog->history.cnt--;
		memcpy(prog->funcs, prog->history.list[prog->history.cnt], strlen(prog->history.list[prog->history.cnt]) + 1);
		free(prog->history.list[prog->history.cnt]);
		break;
	case IN_MAIN:
		prog->flags.cnt--;
		prog->history.cnt--;
		memcpy(prog->body, prog->history.list[prog->history.cnt], strlen(prog->history.list[prog->history.cnt]) + 1);
		free(prog->history.list[prog->history.cnt]);
		break;
	case EMPTY: /* fallthrough */
	default:
		warnx("\n\nline %d:\n%s\n\n", __LINE__, prog->body);
	}
}


int main(int argc, char *argv[])
{
	char const optstring[] = "hvwpc:l:I:o:";

	/* initialize source buffers */
	init_buffers();
	/* initiatalize compiler arg array */
	cc_argv = parse_opts(argc, argv, optstring, &ofile);
	/* initialize user.final and actual.final then print version */
	build_final();
	printf("\n%s\n", CEPL_VERSION);
	/* enable completion */
	rl_completion_entry_function = &generator;
	rl_attempted_completion_function = &completer;
	rl_basic_word_break_characters = " \t\n\"\\'`@$><=|&{}()[]";
	rl_completion_suppress_append = 1;
	rl_bind_key('\t', &rl_complete);

	/* loop readline() until EOF is read */
	while ((line = readline("\n>>> ")) && *line) {
		/* re-enable completion if disabled */
		rl_bind_key('\t', &rl_complete);
		/* add to readline history */
		add_history(line);
		/* re-allocate enough memory for line + '\t' + ';' + '\n' + '\0' */
		resize_buffer(&user.body, 3);
		resize_buffer(&actual.body, 3);
		resize_buffer(&user.final, 3);
		resize_buffer(&actual.final, 3);

		/* control sequence and preprocessor directive parsing */
		switch (line[0]) {
		case ';':
			switch(line[1]) {
			/* clean up and exit program */
			case 'q':
				goto EXIT;
				/* unused break */
				break;

			/* toggle output file writing */
			case 'o':
				/* if file is open, close it break early */
				if (out_flag && ofile) {
					fwrite(actual.final, strlen(actual.final), 1, ofile);
					fputc('\n', ofile);
					fclose(ofile);
					ofile = NULL;
					break;
				}
				/* toggle global warning flag */
				out_flag ^= true;
				/* break if file name empty */
				if (!(tok_buf = strpbrk(line, " \t")) || strspn(tok_buf, " \t") == strlen(tok_buf))
					break;
				/* increment pointer to start of definition */
				tok_buf += strspn(tok_buf, " \t");
				/* output file flag */
				if (out_flag && ((ofile = fopen(tok_buf, "w")) == NULL))
					err(EXIT_FAILURE, "%s", "failed to create output file");
				break;

			/* toggle library parsing */
			case 'p':
				free_buffers();
				init_buffers();
				/* toggle global parse flag */
				parse_flag ^= true;
				/* re-initiatalize compiler arg array */
				cc_argv = parse_opts(argc, argv, optstring, &ofile);
				break;

			/* toggle warnings */
			case 'w':
				free_buffers();
				init_buffers();
				/* toggle global warning flag */
				warn_flag ^= true;
				/* re-initiatalize compiler arg array */
				cc_argv = parse_opts(argc, argv, optstring, &ofile);
				break;

			/* reset state */
			case 'r':
				free_buffers();
				init_buffers();
				/* re-initiatalize compiler arg array */
				cc_argv = parse_opts(argc, argv, optstring, &ofile);
				break;

			/* define an include/macro/function */
			case 'i': /* fallthrough */
			case 'm': /* fallthrough */
			case 'f':
				/* break if function definition empty */
				if (!(tok_buf = strpbrk(line, " \t")) || strspn(tok_buf, " \t") == strlen(tok_buf))
					break;
				/* increment pointer to start of definition */
				tok_buf += strspn(tok_buf, " \t");
				/* re-allocate enough memory for line + '\n' + '\n' + '\0' */
				resize_buffer(&user.funcs, strlen(tok_buf) + 3);
				resize_buffer(&actual.funcs, strlen(tok_buf) + 3);
				build_funcs();
				break;

			/* show usage information */
			case 'h':
				printf("\nUsage: %s %s\n", argv[0], USAGE);
				break;

			/* undo last statement */
			case 'u':
				/* break early if no history to undo */
				if (user.flags.cnt < 2 || actual.flags.cnt < 2)
					break;
				undo(&user);
				undo(&actual);
				break;

			/* unknown command becomes a noop */
			default:
				build_body();
				strcat(user.body, "\n");
				strcat(actual.body, "\n");
			}
			break;

		/* dont append ';' for preprocessor directives */
		case '#':
			/* remove trailing ' ' and '\t' */
			for (register int i = strlen(line) - 1; line[i] == ' ' || line[i] == '\t'; i--)
				line[i] = '\0';
			/* start building program source */
			build_body();
			strcat(user.body, "\n");
			strcat(actual.body, "\n");
			break;

		default:
			/* remove trailing ' ' and '\t' */
			for (register int i = strlen(line) - 1; line[i] == ' ' || line[i] == '\t'; i--)
				line[i] = '\0';
			switch(line[strlen(line) - 1]) {
			case '{': /* fallthough */
			case '}': /* fallthough */
			case ';': /* fallthough */
			case '\\':
				build_body();
				/* remove extra trailing ';' */
				for (register int i = strlen(user.body) - 1; user.body[i - 1] == ';'; i--)
					user.body[i] = '\0';
				for (register int i = strlen(actual.body) - 1; actual.body[i - 1] == ';'; i--)
					actual.body[i] = '\0';
				strcat(user.body, "\n");
				strcat(actual.body, "\n");
				break;
			default:
				/* append ';' if no trailing '}', ';', or '\' */
				build_body();
				strcat(user.body, ";\n");
				strcat(actual.body, ";\n");
			}
		}

		build_final();
		/* print output and exit code */
		printf("\n%s:\n\n%s\n", argv[0], user.final);
		printf("\nexit status: %d\n", compile(actual.final, cc_argv, argv));
		if (line) {
			free(line);
			line = NULL;
		}
	}

EXIT:
	free_buffers();
	if (line)
		free(line);
	if (comp_list.list)
		free_argv(comp_list.list);
	printf("\n%s\n\n", "Terminating program.");

	return 0;
}
