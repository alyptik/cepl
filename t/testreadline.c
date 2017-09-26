/*
 * t/testreadline.c - unit-test for readline.c
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#include "tap.h"
#include "../src/errs.h"
#include "../src/readline.h"

int main (void)
{
	char *line = "foobar";

	plan(1);

	FILE *null;
	if (!(null = fopen("/dev/null", "r+b")))
		ERR("read_line() fopen()");
	rl_outstream = null;
	ok((line = readline(NULL)) != NULL, "send keyboard input to readline.");
	rl_outstream = NULL;
	fclose(null);
	free(line);

	done_testing();
}
