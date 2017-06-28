/*
 * cepl.c - REPL translation unit
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE file for copyright and license details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "compile.h"
#include "readline.h"
#include "parseopts.h"

#define PROG_MAIN_START "int main(void)\n{\n"
#define PROG_MAIN_END "\n\treturn 0;\n}\n"
#define PROG_START "#define _GNU_SOURCE\n#define _POSIX_C_SOURCE 200809L\n#define _XOPEN_SOURCE 9001\n#define __USE_XOPEN\n#include <assert.h>\n#include <ctype.h>\n#include <err.h>\n#include <errno.h>\n#include <fcntl.h>\n#include <limits.h>\n#include <math.h>\n#include <stdalign.h>\n#include <stdbool.h>\n#include <stddef.h>\n#include <stdint.h>\n#include <stdio.h>\n#include <stdlib.h>\n#include <stdnoreturn.h>\n#include <string.h>\n#include <strings.h>\n#include <time.h>\n#include <uchar.h>\n#include <unistd.h>\n#include <sys/types.h>\n#include <sys/syscall.h>\n#include <sys/wait.h>\n#define _Atomic\n#define _Static_assert(a, b)\n\n" PROG_MAIN_START
#define PROG_END PROG_MAIN_END
#define MAIN_START_SIZE (strlen(PROG_MAIN_START) + 1)
#define MAIN_END_SIZE (MAIN_START_SIZE + strlen(PROG_MAIN_END) + 1)
#define START_SIZE (strlen(PROG_START) + 1)
#define END_SIZE (START_SIZE + strlen(PROG_END) + 1)

#define MEM_INIT do { memset(prog_main_start, 0, MAIN_START_SIZE); \
			memset(prog_main_end, 0, MAIN_END_SIZE); \
			memcpy(prog_main_start, PROG_MAIN_START, MAIN_START_SIZE); \
			memset(prog_start, 0, START_SIZE); \
			memset(prog_end, 0, END_SIZE); \
			memcpy(prog_start, PROG_START, START_SIZE); } while (0)

#define FREE_PROGS do { if (prog_main_start) free(prog_main_start); \
			if (prog_main_end) free(prog_main_end); \
			if (prog_start) free(prog_start); \
			if (prog_end) free(prog_end); } while (0)


extern char **comp_list;

int main(int argc, char *argv[])
{
	char optstring[] = "hvwpl:I:o:";
	char *prog_main_start = malloc(MAIN_START_SIZE);
	char *prog_main_end = malloc(MAIN_END_SIZE);
	char *prog_start = malloc(START_SIZE);
	char *prog_end = malloc(END_SIZE);
	FILE *ofile = NULL;
	char *const *cc_argv = parse_opts(argc, argv, optstring, &ofile);
	/* readline buffer */
	char *tmp = NULL, *line = NULL;

	/* initial sanity check */
	if (!prog_main_start || !prog_main_end || !prog_start || !prog_end) {
		FREE_PROGS;
		if (comp_list)
			free_argv(comp_list);
		err(EXIT_FAILURE, "%s", "error allocating inital pointers");
	}

	/* initialize source buffers */
	MEM_INIT;
	/* truncated output to show user */
	memcpy(prog_main_end, prog_main_start, strlen(prog_main_start) + 1);
	strcat(prog_main_end, PROG_MAIN_END);
	/* main program */
	memcpy(prog_end, prog_start, strlen(prog_start) + 1);
	strcat(prog_end, PROG_END);
	putchar('\n');
	printf(CEPL_VERSION);
	putchar('\n');

	/* enable completion */
	rl_completion_entry_function = &generator;
	rl_attempted_completion_function = &completer;
	rl_basic_word_break_characters = " \t\n\"\\'`@$><=|&{(";
	rl_completion_suppress_append = 1;
	rl_bind_key('\t', &rl_complete);

	/* repeat readline() until EOF is read */
	while ((line = readline("\n>>> ")) != NULL && *line) {
		/* re-enable completion if disabled */
		rl_bind_key('\t', &rl_complete);
		/* add to readline history */
		add_history(line);

		/* re-allocate enough memory for line + '\t' + ';' + '\n' + '\0' */
		if ((tmp = realloc(prog_main_start, strlen(prog_main_start) + strlen(line) + 4)) == NULL) {
			FREE_PROGS;
			if (comp_list)
				free_argv(comp_list);
			if (free_argv((char **)cc_argv) == -1)
				err(EXIT_FAILURE, "%s", "error during free_argv() call");
			err(EXIT_FAILURE, "error during realloc() for prog_main_start");
		}
		prog_main_start = tmp;
		if ((tmp = realloc(prog_main_end, strlen(prog_main_end) + strlen(line) + 4)) == NULL) {
			FREE_PROGS;
			if (comp_list)
				free_argv(comp_list);
			if (free_argv((char **)cc_argv) == -1)
				err(EXIT_FAILURE, "%s", "error during free_argv() call");
			err(EXIT_FAILURE, "error during realloc() for prog_main_end");
		}
		prog_main_end = tmp;
		if ((tmp = realloc(prog_start, strlen(prog_start) + strlen(line) + 4)) == NULL) {
			FREE_PROGS;
			if (comp_list)
				free_argv(comp_list);
			if (free_argv((char **)cc_argv) == -1)
				err(EXIT_FAILURE, "%s", "error during free_argv() call");
			err(EXIT_FAILURE, "error during realloc() for prog_start");
		}
		prog_start = tmp;
		if ((tmp = realloc(prog_end, strlen(prog_end) + strlen(line) + 4)) == NULL) {
			FREE_PROGS;
			if (comp_list)
				free_argv(comp_list);
			if (free_argv((char **)cc_argv) == -1)
				err(EXIT_FAILURE, "%s", "error during free_argv() call");
			err(EXIT_FAILURE, "error during realloc() for prog_end");
		}
		prog_end = tmp;

		/* start building program source */
		strcat(prog_main_start, "\t");
		strcat(prog_start, "\t");
		strcat(prog_main_start, strtok(line, "\n"));
		strcat(prog_start, strtok(line, "\n"));

		/* control sequence and preprocessor directive parsing */
		switch (line[0]) {
		case ';':
			switch(line[1]) {
			/* reset state */
			case 'r':
				MEM_INIT;
				break;
			/* TODO: more command handling */
			}
		/* don't append ';' for preprocessor directives */
		case '#': break;
		default:
			switch(line[strlen(line) - 1]) {
			/* don't append ';' if trailing ';' or '}' */
			case '}':
			case ';': break;
			default:
				prog_main_start = strcat(prog_main_start, ";");
				prog_start = strcat(prog_start, ";");
			}
		}

		/* finish building current iteration of source code */
		strcat(prog_main_start, "\n");
		strcat(prog_start, "\n");
		memcpy(prog_main_end, prog_main_start, strlen(prog_main_start) + 1);
		memcpy(prog_end, prog_start, strlen(prog_start) + 1);
		strcat(prog_main_end, PROG_MAIN_END);
		strcat(prog_end, PROG_END);
		/* print output and exit code */
		printf("\n%s:\n\n%s\n", argv[0], prog_main_end);
		printf("\n%s: %d\n", "exit status", compile("gcc", prog_end, cc_argv, argv));
		if (line) {
			free(line);
			line = NULL;
		}
	}

	/* write out program to file if applicable */
	if (ofile) {
		fwrite(prog_end, strlen(prog_end), 1, ofile);
		fputc('\n', ofile);
		fclose(ofile);
	}
	FREE_PROGS;
	if (line)
		free(line);
	if (comp_list)
		free_argv(comp_list);
	if (free_argv((char **)cc_argv) == -1)
		err(EXIT_FAILURE, "%s", "error during free_argv() call");
	printf("\n%s\n\n", "Terminating program.");

	return 0;
}
