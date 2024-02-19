/*
 * t/testreadline.c - unit-test for readline.c
 *
 * AUTHOR: Joey Pabalinas <joeypabalinas@gmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#include "tap.h"
#include "../src/errs.h"
#include "../src/readline.h"

int main (void)
{
	FILE *bitbucket;
	char *ln;

	plan(1);

	if (!(bitbucket = fopen("/dev/null", "r+b")))
		ERR("%s", "read_line() fopen()");
	rl_outstream = bitbucket;
	ok((ln = readline(NULL)) != NULL, "send keyboard input to readline.");
	rl_outstream = NULL;
	fclose(bitbucket);
	free(ln);

	done_testing();
}
