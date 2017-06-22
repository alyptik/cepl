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

int main(int argc, char **argv)
{
	char *buf = NULL;
	char *dest = NULL;
	size_t bufsize = 0;
	size_t prevsize = 0;
	ssize_t ret;
	char *const args[] = {"gcc", "-xc", "-", "-o", "/dev/stdout", NULL};

	printf("\n%s\n\n", "CEPL v0.1.1");
	/* prompt character */
	printf("%s", "> ");

	while ((ret = getline(&buf, &bufsize, stdin)) > 1) {
		/* allocate space for input + ";\n" */
		dest = realloc(dest, sizeof *dest + (strlen(buf) + 3));
		if (strlen(dest) == prevsize) {
			warn("%s", "error during realloc()");
			break;
		}
		prevsize = strlen(dest);
		dest = strcat(dest, strtok(buf, "\n"));
		/* append ';' to dest buffer if no trailing ';' or '}' */
		if (buf[strlen(buf) - 1] != '}' && buf[strlen(buf) - 1] != ';')
			dest = strcat(dest, ";");
		dest = strcat(dest, "\n");

		/* TODO: remove after logic finalized */
		printf("%s - %d:\n%s\n", argv[0], argc, dest);
		/* prompt character */
		printf("%s", "> ");
	}

	if (buf)
		free(buf);
	if (dest)
		free(dest);
	/* getline failed to allocate memory */
	if (ret == -1)
		err(EXIT_FAILURE, "error reading input with getline()");

	printf("\n%s\n\n", "Terminating program.");
	return 0;
}

