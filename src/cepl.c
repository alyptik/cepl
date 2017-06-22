/*
 * cepl.c - REPL translation unit
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE file for copyright and license details.
 */

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include "compile.h"

/* silence linter */
ssize_t getline(char **lineptr, size_t *n, FILE *stream);
extern int pipemain[2];

int main(int argc, char *argv[])
{
	char *buf = NULL;
	char *dest = malloc(60);
	char *final = malloc(74);
	size_t bufsize = 0;
	ssize_t ret;
	/* char *const args[] = {"gcc", "-xc", "/dev/stdin", "-o", "/dev/stdout", NULL}; */
	char *const args[] = {"gcc", "-xc", "/dev/stdin", "-o", "/tmp/cepl", NULL};

	strcpy(dest, "#include <stdio.h>\n#include <stdlib.h>\nint main(void) {\n");
	strcpy(final, dest);
	strcat(final, "return 0;\n}\n");

	printf("\n%s\n\n", "CEPL v0.1.1");
	/* prompt character */
	printf("%s", "> ");

	while ((ret = getline(&buf, &bufsize, stdin)) > 1) {
		/* allocate space for input + ";\n" */
		char *tmp;
		if ((tmp = realloc(dest, strlen(dest) + (strlen(buf) + 3))) == NULL) {
			free(buf);
			free(dest);
			free(final);
			err(EXIT_FAILURE, "error calling realloc()");
		}
		dest = tmp;

		/* dest = strcat(dest, strtok(buf, "\n")); */
		/* [> append ';' to dest buffer if no leading '#' or trailing '}' <] */
		/* if (buf[0] != ';' && buf[0] != '#' && buf[strlen(buf) - 1] != '}') */
		/*         dest = strcat(dest, ";"); */
		/* dest = strcat(dest, "\n"); */

		strcat(dest, strtok(buf, "\n"));
		/* append ';' to dest buffer if no trailing ';' or '}' */
		switch (buf[0]) {
		/* TODO: command handling */
		case ';':
		case '#': break;
		default:
			switch(buf[strlen(buf) - 1]) {
			case '}':
			case ';': break;
			default:
				  dest = strcat(dest, ";");
			}
		}
		strcat(dest, "\n");

		if ((tmp = realloc(final, strlen(final) + (strlen(buf) + 3))) == NULL) {
			free(buf);
			free(dest);
			free(final);
			err(EXIT_FAILURE, "error calling realloc()");
		}
		final = tmp;

		/* memcpy(final, dest, strlen(dest) + 1); */
		strcpy(final, dest);
		strcat(final, "return 0;\n}\n");
		puts(final);

		if (compile("gcc", final, args, argv) == 0)
			err(EXIT_FAILURE, "no fd returned by compile()");

		/* pipe_fd(pipemain[0], out_fd); */
		/* TODO: remove after logic finalized */
		printf("%s - %d:\n%s\n", argv[0], argc, dest);
		/* prompt character */
		printf("%s", "> ");
	}

	if (buf)
		free(buf);
	if (dest)
		free(dest);
	if (final)
		free(final);
	/* getline failed to allocate memory */
	if (ret == -1)
		err(EXIT_FAILURE, "error reading input with getline()");

	printf("\n%s\n\n", "Terminating program.");
	return 0;
}

