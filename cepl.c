/*
 * cepl.c - REPL translation unit
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE file for copyright and license details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

int main(int argc, char **argv)
{
	char *buf = NULL;
	size_t bufsize = 0;
	ssize_t ret;

	printf("\n%s\n\n", "CEPL v0.1.1");
	/* prompt character */
	printf("%s", "> ");
	while ((ret = getline(&buf, &bufsize, stdin)) > 1) {
		printf("%s - %d: %s\n", argv[0], argc, buf);
		/* prompt character */
		printf("%s", "> ");
	}
	printf("\n%s\n\n", "Terminating program.");

	return 0;
}

