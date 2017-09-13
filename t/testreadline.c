/*
 * t/testreadline.c - unit-test for readline.c
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#include "tap.h"
#include "../src/readline.h"

int main (void)
{
	char *line = "foobar";

	plan(1);

	ok((line = readline("> ")) != NULL && *line, "send keyboard input to readline.");
	free(line);

	done_testing();
}
