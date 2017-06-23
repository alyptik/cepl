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
	int ret;
	char *buf = NULL;
	char *prog_start = malloc(START_SIZE);
	char *prog_end = malloc(END_SIZE);
	size_t bufsize = 0;
	ssize_t line_size;
	char *const args[] = {"gcc", "-std=c11", "-xc", "/dev/stdin", "-o", "/dev/stdout", NULL};

	strcpy(prog_start, PROG_START);
	strcpy(prog_end, prog_start);
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
			err(EXIT_FAILURE, "error calling realloc()");
		}

		strcat(prog_start, "\t");
		strcat(prog_start, strtok(buf, "\n"));
		/* append ';' to prog_start buffer if no trailing ';' or '}' */
		switch (buf[0]) {
		/* TODO: command handling */
		case ';':
		case '#': break;
		default:
			switch(buf[strlen(buf) - 1]) {
			case '}':
			case ';': break;
			default:
				  prog_start = strcat(prog_start, ";");
			}
		}
		strcat(prog_start, "\n");

		if ((prog_end = realloc(prog_end, strlen(prog_end) + (strlen(buf) + 4))) == NULL) {
			free(buf);
			free(prog_start);
			free(prog_end);
			err(EXIT_FAILURE, "error calling realloc()");
		}
		/* memcpy(prog_end, prog_start, strlen(prog_start) + 1); */
		strcpy(prog_end, prog_start);
		strcat(prog_end, PROG_END);

		/* TODO: remove after logic prog_endized */
		printf("\n%s - %d:\n\n%s\n", argv[0], argc, prog_end);

		ret = compile("gcc", prog_end, args, argv);
		/* putchar('\n'); */
		printf("\n%s: %d\n\n", "exit status:", ret);
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

