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

#define CEPL_VERSION "CEPL v0.1.2"
#define PROG_START "#define _GNU_SOURCE\n#define _POSIX_C_SOURCE 200809L\n#define _XOPEN_SOURCE 9001\n#define __USE_XOPEN\n#include <assert.h>\n#include <ctype.h>\n#include <err.h>\n#include <errno.h>\n#include <fcntl.h>\n#include <limits.h>\n#include <math.h>\n#include <stdalign.h>\n#include <stdbool.h>\n#include <stddef.h>\n#include <stdint.h>\n#include <stdio.h>\n#include <stdlib.h>\n#include <stdnoreturn.h>\n#include <string.h>\n#include <strings.h>\n#include <time.h>\n#include <uchar.h>\n#include <unistd.h>\n#include <sys/types.h>\n#include <sys/syscall.h>\n#include <sys/wait.h>\n#define _Atomic\n#define _Static_assert(a, b)\nint main(void)\n{\n"
#define PROG_END "\treturn 0;\n}\n"
#define START_SIZE (strlen(PROG_START) + 1)
#define END_SIZE (START_SIZE + strlen(PROG_END) + 1)

/* arguments to pass to compiler */
static char *const cc_args[] = {"gcc", "-O2", "-pipe", "-Wall", "-Wextra", "-pedantic-errors", "-std=c11", "-xc", "/dev/stdin", "-o", "/dev/stdout", NULL};
/* silence linter warnings */
ssize_t getline(char **lineptr, size_t *n, FILE *stream);

int main(int argc, char *argv[])
{
	size_t buf_size = 0;
	ssize_t line_size = 0;
	char *buf = NULL;
	char *prog_start = malloc(START_SIZE);
	char *prog_end = malloc(END_SIZE);
	/* temp pointer for realloc */
	char *tmp = NULL;

	memset(prog_start, 0, START_SIZE);
	memset(prog_end, 0, END_SIZE);
	memcpy(prog_start, PROG_START, START_SIZE);
	memcpy(prog_end, prog_start, strlen(prog_start) + 1);
	strcat(prog_end, PROG_END);

	printf("\n%s\n\n", CEPL_VERSION);
	/* prompt character */
	printf("%s", "> ");

	while ((line_size = getline(&buf, &buf_size, stdin)) > 1) {
		/* re-allocate enough for buf + '\t' + ';' + '\n' + '\0' */
		if ((tmp = realloc(prog_start, strlen(prog_start) + strlen(buf) + 4)) == NULL) {
			free(buf);
			free(prog_start);
			free(prog_end);
			err(EXIT_FAILURE, "error during realloc() for prog_start");
		}
		prog_start = tmp;
		if ((tmp = realloc(prog_end, strlen(prog_end) + strlen(buf) + 4)) == NULL) {
			free(buf);
			free(prog_start);
			free(prog_end);
			err(EXIT_FAILURE, "error during realloc() for prog_end");
		}
		prog_end = tmp;

		/* build program source */
		strcat(prog_start, "\t");
		strcat(prog_start, strtok(buf, "\n"));
		switch (buf[0]) {
		case ';':
			switch(buf[1]) {
			/* reset state */
			case 'r':
				memset(prog_start, 0, START_SIZE);
				memset(prog_end, 0, END_SIZE);
				memcpy(prog_start, PROG_START, START_SIZE);
				break;
			/* TODO: more command handling */
			}
		/* don't append ';' for preprocessor directives */
		case '#': break;
		default:
			switch(buf[strlen(buf) - 1]) {
			/* don't append ';' if trailing ';' or '}' */
			case '}':
			case ';': break;
			default:
				prog_start = strcat(prog_start, ";");
			}
		}
		strcat(prog_start, "\n");
		memcpy(prog_end, prog_start, strlen(prog_start) + 1);
		strcat(prog_end, PROG_END);

		/* TODO: finalize output format */
		printf("\n%s — [%s] = \"%d\":\n\n%s\n", argv[0], "argc", argc, prog_end);

		printf("\n%s: %d\n\n", "exit status", compile("gcc", prog_end, cc_args, argv));
		/* prompt character */
		printf("%s", "> ");
	}

	if (buf)
		free(buf);
	if (prog_start)
		free(prog_start);
	if (prog_end)
		free(prog_end);
	if (line_size == -1)
		err(EXIT_FAILURE, "error reading input with getline()");
	printf("\n%s\n\n", "Terminating program.");

	return 0;
}

