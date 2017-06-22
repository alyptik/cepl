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

	printf("\n%s\n\n", "CEPL v0.1.1");
	/* prompt character */
	printf("%s", "> ");

	while ((ret = getline(&buf, &bufsize, stdin)) > 1) {
		dest = realloc(dest, sizeof *dest + (strlen(buf) + 3));
		/* allocate space for input + ";\n" */
		if (sizeof *dest == prevsize) {
			warn("%s", "error during realloc");
			break;
		}

		dest = strcat(dest, strtok(buf, "\n"));
		/* append ; to the end of dest buffer if no trailing ; */
		if (strrchr(buf, ';') == NULL || strrchr(buf, ';') + 1 < buf + strlen(buf))
			dest = strcat(dest, ";");
		dest = strcat(dest, "\n");

		printf("%s - %d:\n%s\n", argv[0], argc, dest);
		/* prompt character */
		printf("%s", "> ");
	}

	if (ret == -1)
		err(1, "error reading input with getline");
	if (buf)
		free(buf);
	if (dest)
		free(dest);
	printf("\n%s\n\n", "Terminating program.");

	return 0;
}

