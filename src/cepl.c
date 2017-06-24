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

#define CEPL_VERSION "CEPL v0.1.2"
#define PROG_MAIN_START "int main(void)\n{\n"
#define PROG_MAIN_END "\n\treturn 0;\n}\n"
#define PROG_START "#define _GNU_SOURCE\n#define _POSIX_C_SOURCE 200809L\n#define _XOPEN_SOURCE 9001\n#define __USE_XOPEN\n#include <assert.h>\n#include <ctype.h>\n#include <err.h>\n#include <errno.h>\n#include <fcntl.h>\n#include <limits.h>\n#include <math.h>\n#include <stdalign.h>\n#include <stdbool.h>\n#include <stddef.h>\n#include <stdint.h>\n#include <stdio.h>\n#include <stdlib.h>\n#include <stdnoreturn.h>\n#include <string.h>\n#include <strings.h>\n#include <time.h>\n#include <uchar.h>\n#include <unistd.h>\n#include <sys/types.h>\n#include <sys/syscall.h>\n#include <sys/wait.h>\n#define _Atomic\n#define _Static_assert(a, b)\n" PROG_MAIN_START
#define PROG_END PROG_MAIN_END
#define MAIN_START_SIZE (strlen(PROG_MAIN_START) + 1)
#define MAIN_END_SIZE (MAIN_START_SIZE + strlen(PROG_MAIN_END) + 1)
#define START_SIZE (strlen(PROG_START) + 1)
#define END_SIZE (START_SIZE + strlen(PROG_END) + 1)
#define UNUSED __attribute__ ((unused))
/* silence linter warnings */
ssize_t getline(char **lineptr, size_t *n, FILE *stream);

/* arguments to pass to compiler */
static char *const cc_args[] = {"gcc", "-O2", "-pipe", "-Wall", "-Wextra", "-pedantic-errors", "-std=c11", "-xc", "/dev/stdin", "-o", "/dev/stdout", NULL};
/* readline buffer */
static char *line = NULL;

int main(int argc UNUSED, char *argv[])
{
	char *prog_main_start = malloc(MAIN_START_SIZE);
	char *prog_main_end = malloc(MAIN_END_SIZE);
	char *prog_start = malloc(START_SIZE);
	char *prog_end = malloc(END_SIZE);
	/* temp char pointer for realloc() */
	char *tmp = NULL;

	/* initial sanity check */
	if (prog_main_start == NULL || prog_main_end == NULL || prog_start == NULL || prog_end == NULL)
		err(EXIT_FAILURE, "%s", "error allocating inital pointers");

	/* truncated output to show user */
	memset(prog_main_start, 0, MAIN_START_SIZE);
	memset(prog_main_end, 0, MAIN_END_SIZE);
	memcpy(prog_main_start, PROG_MAIN_START, MAIN_START_SIZE);
	memcpy(prog_main_end, prog_main_start, strlen(prog_main_start) + 1);
	strcat(prog_main_end, PROG_MAIN_END);
	/* main program */
	memset(prog_start, 0, START_SIZE);
	memset(prog_end, 0, END_SIZE);
	memcpy(prog_start, PROG_START, START_SIZE);
	memcpy(prog_end, prog_start, strlen(prog_start) + 1);
	strcat(prog_end, PROG_END);

	/* disable filename completion */
	rl_bind_key('\t', rl_abort);
	printf("\n%s\n", CEPL_VERSION);

	/* repeat readline() until EOF is read */
	while ((line = readline("\n>>> ")) != NULL && *line) {
		/* add to readline history */
		add_history(line);

		/* re-allocate enough for line + '\t' + ';' + '\n' + '\0' */
		if ((tmp = realloc(prog_main_start, strlen(prog_main_start) + strlen(line) + 4)) == NULL) {
			free(line);
			free(prog_main_start);
			free(prog_main_end);
			free(prog_start);
			free(prog_end);
			err(EXIT_FAILURE, "error during realloc() for prog_main_start");
		}
		prog_main_start = tmp;
		if ((tmp = realloc(prog_main_end, strlen(prog_main_end) + strlen(line) + 4)) == NULL) {
			free(line);
			free(prog_main_start);
			free(prog_main_end);
			free(prog_start);
			free(prog_end);
			err(EXIT_FAILURE, "error during realloc() for prog_main_end");
		}
		prog_main_end = tmp;
		if ((tmp = realloc(prog_start, strlen(prog_start) + strlen(line) + 4)) == NULL) {
			free(line);
			free(prog_main_start);
			free(prog_main_end);
			free(prog_start);
			free(prog_end);
			err(EXIT_FAILURE, "error during realloc() for prog_start");
		}
		prog_start = tmp;
		if ((tmp = realloc(prog_end, strlen(prog_end) + strlen(line) + 4)) == NULL) {
			free(line);
			free(prog_main_start);
			free(prog_main_end);
			free(prog_start);
			free(prog_end);
			err(EXIT_FAILURE, "error during realloc() for prog_end");
		}
		prog_end = tmp;

		/* build program source */
		strcat(prog_main_start, "\t");
		strcat(prog_start, "\t");
		strcat(prog_main_start, strtok(line, "\n"));
		strcat(prog_start, strtok(line, "\n"));
		switch (line[0]) {
		case ';':
			switch(line[1]) {
			/* reset state */
			case 'r':
				memset(prog_main_start, 0, MAIN_START_SIZE);
				memset(prog_main_end, 0, MAIN_END_SIZE);
				memcpy(prog_main_start, PROG_MAIN_START, MAIN_START_SIZE);
				memset(prog_start, 0, START_SIZE);
				memset(prog_end, 0, END_SIZE);
				memcpy(prog_start, PROG_START, START_SIZE);
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
		strcat(prog_main_start, "\n");
		strcat(prog_start, "\n");
		memcpy(prog_main_end, prog_main_start, strlen(prog_main_start) + 1);
		memcpy(prog_end, prog_start, strlen(prog_start) + 1);
		strcat(prog_main_end, PROG_MAIN_END);
		strcat(prog_end, PROG_END);

		/* TODO: finalize output format */
		printf("\n%s:\n\n%s\n", argv[0], prog_main_end);
		printf("\n%s: %d\n", "exit status", compile("gcc", prog_end, cc_args, argv));
		if (line) {
			free(line);
			line = NULL;
		}
	}

	if (line)
		free(line);
	if (prog_main_start)
		free(prog_main_start);
	if (prog_main_end)
		free(prog_main_end);
	if (prog_start)
		free(prog_start);
	if (prog_end)
		free(prog_end);

	printf("\n%s\n\n", "Terminating program.");
	return 0;
}

