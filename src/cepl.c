/*
 * cepl.c - REPL translation unit
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE file for copyright and license details.
 */

#include <sys/types.h>
#include "compile.h"
#include "readline.h"
#include "parseopts.h"

/* source file templates */
#define PROG_MAIN_START	("int main(int argc, char *argv[])\n{\n")
#define PROG_START	("#define _GNU_SOURCE\n#define _POSIX_C_SOURCE 200809L\n#define _XOPEN_SOURCE 9001\n#define __USE_XOPEN\n#define _Atomic\n#define _Static_assert(a, b)\n#define UNUSED __attribute__ ((unused))\n#include <assert.h>\n#include <ctype.h>\n#include <err.h>\n#include <errno.h>\n#include <fcntl.h>\n#include <limits.h>\n#include <math.h>\n#include <signal.h>\n#include <stdalign.h>\n#include <stdarg.h>\n#include <stdbool.h>\n#include <stddef.h>\n#include <stdint.h>\n#include <stdio.h>\n#include <stdlib.h>\n#include <stdnoreturn.h>\n#include <string.h>\n#include <strings.h>\n#include <time.h>\n#include <uchar.h>\n#include <unistd.h>\n#include <sys/mman.h>\n#include <sys/types.h>\n#include <sys/syscall.h>\n#include <sys/wait.h>\nextern char **environ;\n\nint main(int argc UNUSED, char *argv[] UNUSED)\n{\n")
#define PROG_INCLUDES	("#define _GNU_SOURCE\n#define _POSIX_C_SOURCE 200809L\n#define _XOPEN_SOURCE 9001\n#define __USE_XOPEN\n#define _Atomic\n#define _Static_assert(a, b)\n#define UNUSED __attribute__ ((unused))\n#include <assert.h>\n#include <ctype.h>\n#include <err.h>\n#include <errno.h>\n#include <fcntl.h>\n#include <limits.h>\n#include <math.h>\n#include <signal.h>\n#include <stdalign.h>\n#include <stdarg.h>\n#include <stdbool.h>\n#include <stddef.h>\n#include <stdint.h>\n#include <stdio.h>\n#include <stdlib.h>\n#include <stdnoreturn.h>\n#include <string.h>\n#include <strings.h>\n#include <time.h>\n#include <uchar.h>\n#include <unistd.h>\n#include <sys/mman.h>\n#include <sys/types.h>\n#include <sys/syscall.h>\n#include <sys/wait.h>\nextern char **environ;\n\n")
#define PROG_MAIN_END	("\n\treturn 0;\n}\n")
#define PROG_END	("\n\treturn 0;\n}\n")
/* character lengths of buffer components */
#define MAIN_START_SIZE	(strlen(PROG_MAIN_START) + 2)
#define START_SIZE	(strlen(PROG_START) + 2)
#define INCLUDES_SIZE	(strlen(PROG_INCLUDES) + 2)
#define MAIN_END_SIZE	(MAIN_START_SIZE + strlen(PROG_MAIN_END) + 2)
#define END_SIZE	(START_SIZE + strlen(PROG_END) + 2)

/* readline buffer */
static char *line = NULL;
/* beautified source buffer start block */
static char *prog_main_start = NULL;
/* actual source buffer start block */
static char *prog_start = NULL;
/* beautified source buffer end block */
static char *prog_main_end = NULL;
/* actual source buffer end block */
static char *prog_end = NULL;
/* compiler arg array */
static char *const *cc_argv;

/* completion list of generated symbols */
extern struct str_list comp_list;
/* toggle flag for warnings and completions */
extern bool warn_flag, parse_flag;

static inline void free_buffers(void)
{
	if (prog_main_start)
		free(prog_main_start);
	if (prog_start)
		free(prog_start);
	if (prog_main_end)
		free(prog_main_end);
	if (prog_end)
		free(prog_end);
	prog_main_start = NULL;
	prog_start = NULL;
	prog_main_end = NULL;
	prog_end = NULL;
}

static inline void init_buffers(void)
{
	prog_main_start = malloc(MAIN_START_SIZE);
	prog_start = malloc(START_SIZE);
	prog_main_end = malloc(MAIN_END_SIZE);
	prog_end = malloc(END_SIZE);
	/* sanity check */
	if (!prog_main_start || !prog_start || !prog_main_end || !prog_end) {
		free_buffers();
		if (cc_argv)
			free_argv((char **)cc_argv);
		if (comp_list.list)
			free_argv(comp_list.list);
		err(EXIT_FAILURE, "%s", "error allocating initial pointers");
	}
	memset(prog_main_start, 0, MAIN_START_SIZE);
	memset(prog_start, 0, START_SIZE);
	memset(prog_main_end, 0, MAIN_END_SIZE);
	memset(prog_end, 0, END_SIZE);
	memcpy(prog_main_start, PROG_MAIN_START, MAIN_START_SIZE);
	memcpy(prog_start, PROG_START, START_SIZE);
}

static inline void resize_buffers(char **buffer, size_t offset)
{
	char *tmp;
	/* current length + line length + extra characters + \0 */
	if ((tmp = realloc(*buffer, strlen(*buffer) + strlen(line) + offset + 1)) == NULL) {
		free_buffers();
		if (cc_argv)
			free_argv((char **)cc_argv);
		if (comp_list.list)
			free_argv(comp_list.list);
		err(EXIT_FAILURE, "error during resize_buffers(%s, %lu)", *buffer, offset);
	}
	*buffer = tmp;
}

static inline void build_src(void)
{
	strcat(prog_main_start, "\t");
	strcat(prog_start, "\t");
	strcat(prog_main_start, strtok(line, "\0\n"));
	strcat(prog_start, strtok(line, "\0\n"));
}

int main(int argc, char *argv[])
{
	FILE *ofile = NULL;
	char *const optstring = "hvwpc:l:I:o:";
	char *func_buf = malloc(START_SIZE);
	char *tok_buf;

	/* initialize source buffers */
	init_buffers();
	memset(func_buf, 0, START_SIZE);
	/* initiatalize compiler arg array */
	cc_argv = parse_opts(argc, argv, optstring, &ofile);

	/* truncated output to show user */
	memcpy(prog_main_end, prog_main_start, strlen(prog_main_start) + 1);
	strcat(prog_main_end, PROG_MAIN_END);
	/* main program */
	memcpy(prog_end, prog_start, strlen(prog_start) + 1);
	strcat(prog_end, PROG_END);
	printf("\n%s\n", CEPL_VERSION);

	/* enable completion */
	rl_completion_entry_function = &generator;
	rl_attempted_completion_function = &completer;
	rl_basic_word_break_characters = " \t\n\"\\'`@$><=|&{(";
	rl_completion_suppress_append = 1;
	rl_bind_key('\t', &rl_complete);

	/* repeat readline() until EOF is read */
	while ((line = readline("\n>>> ")) && *line) {
		/* re-enable completion if disabled */
		rl_bind_key('\t', &rl_complete);
		/* add to readline history */
		add_history(line);
		/* re-allocate enough memory for line + '\t' + ';' + '\n' + '\0' */
		resize_buffers(&prog_main_start, 3);
		resize_buffers(&prog_start, 3);
		resize_buffers(&prog_main_end, 3);
		resize_buffers(&prog_end, 3);

		/* control sequence and preprocessor directive parsing */
		switch (line[0]) {
		case ';':
			/* TODO: more command handling */
			switch(line[1]) {
			/* define a function */
			case 'f':
				resize_buffers(&func_buf, strlen(prog_start) + 3);
				/* ignore up to the first space after ; */
				(void) strtok(line, " ");
				tok_buf = strtok(NULL, "\0\n");
				/* generate source buffer */
				memset(func_buf, 0, strlen(prog_start) + 3);
				memcpy(func_buf, PROG_INCLUDES, INCLUDES_SIZE);
				strcat(func_buf, tok_buf);
				strcat(func_buf, "\n\n");
				strcat(func_buf, prog_main_start);
				memcpy(prog_start, func_buf, strlen(func_buf) + 1);
				/* generate truncated buffer */
				memset(func_buf, 0, strlen(prog_start) + 3);
				memcpy(func_buf, tok_buf, strlen(tok_buf) + 1);
				strcat(func_buf, "\n\n");
				strcat(func_buf, prog_main_start);
				memcpy(prog_main_start, func_buf, strlen(func_buf) + 1);
				break;

			/* reset state */
			case 'r':
				free_buffers();
				init_buffers();
				free_argv((char **)cc_argv);
				/* re-initiatalize compiler arg array */
				cc_argv = parse_opts(argc, argv, optstring, &ofile);
				break;

			/* toggle warnings */
			case 'w':
				warn_flag ^= true;
				free_buffers();
				init_buffers();
				free_argv((char **)cc_argv);
				/* re-initiatalize compiler arg array */
				cc_argv = parse_opts(argc, argv, optstring, &ofile);
				break;

			/* toggle parsing libraries for completions */
			case 'p':
				parse_flag ^= true;
				free_buffers();
				init_buffers();
				free_argv((char **)cc_argv);
				/* re-initiatalize compiler arg array */
				cc_argv = parse_opts(argc, argv, optstring, &ofile);
				break;

			/* break from readline loop */
			case 'q':
				goto QUIT;
				/* unused */
				break;

			/* unknown command becomes a noop */
			default:
				build_src();
				strcat(prog_main_start, "\n");
				strcat(prog_start, "\n");
			}
			break;

		/* dont append ; for preprocessor directives */
		case '#':
			/* start building program source */
			build_src();
			strcat(prog_main_start, "\n");
			strcat(prog_start, "\n");
			break;

		default:
			switch(line[strlen(line) - 1]) {
			case '}': /* fallthough */
			case ';': /* fallthough */
			case '\\':
				build_src();
				strcat(prog_main_start, "\n");
				strcat(prog_start, "\n");
				break;

			default:
				/* append ; if no trailing }, ;, or \ */
				build_src();
				strcat(prog_main_start, ";\n");
				strcat(prog_start, ";\n");
			}
		}
		/* finish building current iteration of source code */
		memcpy(prog_main_end, prog_main_start, strlen(prog_main_start) + 1);
		memcpy(prog_end, prog_start, strlen(prog_start) + 1);
		strcat(prog_main_end, PROG_MAIN_END);
		strcat(prog_end, PROG_END);

		/* print output and exit code */
		printf("\n%s:\n\n%s\n", argv[0], prog_main_end);
		printf("\n%s: %d\n", "exit status", compile(prog_end, cc_argv, argv));

		if (line)
			free(line);
	}

QUIT:
	/* write out program to file if applicable */
	if (ofile) {
		fwrite(prog_end, strlen(prog_end), 1, ofile);
		fputc('\n', ofile);
		fclose(ofile);
	}
	free_buffers();
	if (cc_argv)
		free_argv((char **)cc_argv);
	if (comp_list.list)
		free_argv(comp_list.list);
	if (line)
		free(line);
	printf("\n%s\n\n", "Terminating program.");

	return 0;
}
