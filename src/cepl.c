/*
 * cepl.c - REPL translation unit
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE file for copyright and license details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include "compile.h"

#define START_SIZE 65
#define END_SIZE 80
#define PROG_START "#include <stdio.h>\n#include <stdlib.h>\nint main(void) {\n"
#define PROG_END "\treturn 0;\n}\n"

/* silence linter */
ssize_t getline(char **lineptr, size_t *n, FILE *stream);

int main(int argc, char *argv[])
{
	size_t bufsize = 0;
	ssize_t line_size = 0;
	char *buf = NULL;
	char *prog_start = malloc(START_SIZE);
	char *prog_end = malloc(END_SIZE);
	char *const args[] = {"gcc", "-std=c11", "-xc", "/dev/stdin", "-o", "/dev/stdout", NULL};

	memset(prog_start, 0, START_SIZE);
	memset(prog_end, 0, END_SIZE);
	memcpy(prog_start, PROG_START, strlen(PROG_START) + 1);
	strcat(prog_start, "\n");
	memcpy(prog_end, prog_start, strlen(prog_start) + 1);
	strcat(prog_end, PROG_END);

	printf("\n%s\n\n", "CEPL v0.1.1");
	/* prompt character */
	printf("%s", "> ");

	while ((line_size = getline(&buf, &bufsize, stdin)) > 1) {
		/* allocate space for input + ";\n" */
		if ((prog_start = realloc(prog_start, strlen(prog_start) + (strlen(buf) + 4))) == NULL) {
			free(buf);
			free(prog_start);
			free(prog_end);
			err(EXIT_FAILURE, "error calling realloc() on prog_start");
		}
		if ((prog_end = realloc(prog_end, strlen(prog_end) + (strlen(buf) + 4))) == NULL) {
			free(buf);
			free(prog_start);
			free(prog_end);
			err(EXIT_FAILURE, "error calling realloc() on prog_end");
		}
		strcat(prog_start, "\t");
		strcat(prog_start, strtok(buf, "\n"));

		switch (buf[0]) {
		case ';':
			switch(buf[1]) {
			case 'r':
				memset(prog_start, 0, START_SIZE);
				memset(prog_end, 0, END_SIZE);
				memcpy(prog_start, PROG_START, strlen(PROG_START) + 1);
				break;
			/* TODO: more command handling */
			}
		/* fallthrough */
		case '#': break;
		default:
			/* append ';' to prog_start buffer if no trailing ';' or '}' */
			switch(buf[strlen(buf) - 1]) {
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
		printf("\n%s - %d:\n\n%s\n", argv[0], argc, prog_end);

		printf("\n%s: %d\n\n", "exit status", compile("gcc", prog_end, args, argv));
		/* prompt character */
		printf("%s", "> ");
	}

	if (buf)
		free(buf);
	if (prog_start)
		free(prog_start);
	if (prog_end)
		free(prog_end);
	/* getline failed to allocate memory */
	if (line_size == -1)
		err(EXIT_FAILURE, "error reading input with getline()");

	printf("\n%s\n\n", "Terminating program.");
	return 0;
}

