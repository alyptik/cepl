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
	FILE *ofile = NULL;
	int argc = 0;
	char *argv[] = {
		"cepl", "-lssl", "-I.",
		"-o/tmp/test", NULL
	};
	char *const optstring = "hvwpc:l:I:o:";
	char *libs[] = {"ssl", "readline", NULL};
	char *const *result;

	for (; argv[argc]; argc++);
	result = parse_opts(argc, argv, optstring, &ofile);
	printf("%s\n%s", "# generated compiler string: ", "# ");
	for (int i = 0; result[i]; (printf("%s ", result[i]), i++));
	putchar('\n');

	plan(2);

	like(result[0], "^(gcc|icc|clang)$", "test option parsing.");
	ok(parse_libs(libs) != NULL, "test library parsing.");

	done_testing();

	free_argv((char **)result);

}
