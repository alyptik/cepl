/*
 * t/testreadline.c - Unit-test for readline.c
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE file for copyright and license details.
 */

#include "tap.h"
#include "../src/readline.h"

int main (void)
{
	/* char *const argv[] = {"testcompile", NULL}; */
	/* char *src = "int main(void) { return 0; }"; */
	/* char *const cc_args[] = {"gcc", "-O2", "-pipe", "-Wall", "-Wextra", "-pedantic-errors", "-std=c11", "-xc", "/dev/stdin", "-o", "/dev/stdout", NULL}; */
	char *line = "foobar";

	plan(1);

	diag("%s\n", "type any character string");
	ok((line = readline("> ")) != NULL && *line, "read keyboard input.");
	if (line)
		free(line);

	done_testing();
}
