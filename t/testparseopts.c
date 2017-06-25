/*
 * t/testparseopts.c - unit-test for parseopts.c
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE file for copyright and license details.
 */

#include "tap.h"
#include "../src/parseopts.h"

int main (void)
{
	int argc = 5;
	char *argv[] = {"cepl", "-llib", "-l slib", "-Iinc", "-I sinc", "-o out"};
	char optstring[] = "hvl:I:o:";
	char *const *result = parse_opts(argc, argv, optstring);

	plan(1);

	printf("%s\n%s", "# generated compiler string: ", "# ");
	for (int i = 0; i < 15; i++)
		printf("%s ", result[i]);
	putchar('\n');
	is(result[0], "gcc", "test option parsing.");

	done_testing();
}
