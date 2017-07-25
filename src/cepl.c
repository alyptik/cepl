/*
 * cepl.c - REPL translation unit
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#include <sys/types.h>
#include "compile.h"
#include "readline.h"
#include "parseopts.h"

/* source file templates */
#define PROG_MAIN_START	("int main(int argc, char *argv[])\n{\n")
#define PROG_START	("#define _BSD_SOURCE\n#define _DEFAULT_SOURCE\n#define _GNU_SOURCE\n#define _POSIX_C_SOURCE 200809L\n#define _SVID_SOURCE\n#define _XOPEN_SOURCE 700\n#define _Atomic\n#define _Static_assert(a, b)\n#define UNUSED __attribute__ ((unused))\n#include <assert.h>\n#include <ctype.h>\n#include <err.h>\n#include <errno.h>\n#include <fcntl.h>\n#include <limits.h>\n#include <math.h>\n#include <regex.h>\n#include <signal.h>\n#include <stdalign.h>\n#include <stdarg.h>\n#include <stdbool.h>\n#include <stddef.h>\n#include <stdint.h>\n#include <stdio.h>\n#include <stdlib.h>\n#include <stdnoreturn.h>\n#include <string.h>\n#include <strings.h>\n#include <time.h>\n#include <uchar.h>\n#include <wchar.h>\n#include <unistd.h>\n#include <sys/mman.h>\n#include <sys/types.h>\n#include <sys/syscall.h>\n#include <sys/wait.h>\nextern char **environ;\n\nint main(int argc UNUSED, char *argv[] UNUSED)\n{\n")
#define PROG_INCLUDES	("#define _BSD_SOURCE\n#define _DEFAULT_SOURCE\n#define _GNU_SOURCE\n#define _POSIX_C_SOURCE 200809L\n#define _SVID_SOURCE\n#define _XOPEN_SOURCE 700\n#define _Atomic\n#define _Static_assert(a, b)\n#define UNUSED __attribute__ ((unused))\n#include <assert.h>\n#include <ctype.h>\n#include <err.h>\n#include <errno.h>\n#include <fcntl.h>\n#include <limits.h>\n#include <math.h>\n#include <regex.h>\n#include <signal.h>\n#include <stdalign.h>\n#include <stdarg.h>\n#include <stdbool.h>\n#include <stddef.h>\n#include <stdint.h>\n#include <stdio.h>\n#include <stdlib.h>\n#include <stdnoreturn.h>\n#include <string.h>\n#include <strings.h>\n#include <time.h>\n#include <uchar.h>\n#include <wchar.h>\n#include <unistd.h>\n#include <sys/mman.h>\n#include <sys/types.h>\n#include <sys/syscall.h>\n#include <sys/wait.h>\nextern char **environ;\n\n")
#define PROG_MAIN_END	("\n\treturn 0;\n}\n")
#define PROG_END	("\n\treturn 0;\n}\n")
/* character lengths of buffer components */
#define MAIN_START_SIZE	(strlen(PROG_MAIN_START) + 1)
#define START_SIZE	(strlen(PROG_START) + 1)
#define INCLUDES_SIZE	(strlen(PROG_INCLUDES) + 1)
#define MAIN_END_SIZE	(MAIN_START_SIZE + strlen(PROG_MAIN_END) + 1)
#define END_SIZE	(START_SIZE + strlen(PROG_END) + 1)

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
/* function definition buffer */
static char *func_buf = NULL;
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
	if (func_buf)
		free(func_buf);
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
	func_buf = malloc(START_SIZE);
	/* sanity check */
	if (!prog_main_start || !prog_start || !prog_main_end || !prog_end || !func_buf) {
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
	memset(func_buf, 0, START_SIZE);
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

static inline void build_body(void)
{
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

int main(int argc, char *argv[])
{
	char const optstring[] = "hvwpc:l:I:o:";
	char *tok_buf;

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

	/* repeat readline() until EOF is read */
	while ((line = readline("\n>>> ")) && *line) {
		/* re-enable completion if disabled */
		rl_bind_key('\t', &rl_complete);
		/* add to readline history */
		add_history(line);
		/* re-allocate enough memory for line + '\t' + ';' + '\n' + '\0' */
		resize_buffer(&prog_main_start, 3);
		resize_buffer(&prog_start, 3);
		resize_buffer(&prog_main_end, 3);
		resize_buffer(&prog_end, 3);

		/* control sequence and preprocessor directive parsing */
		switch (line[0]) {
		case ';':
			/* TODO: more command handling */
			switch(line[1]) {
			/* clean up and exit program */
			case 'q':
				goto EXIT;
				/* unused break */
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
				/* generate function buffers */
				strcat(user.funcs, tok_buf);
				strcat(actual.funcs, tok_buf);
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
		if (line)
			free(line);
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
