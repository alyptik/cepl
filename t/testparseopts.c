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
		"cepl", "-lssl", "-l", "tap",
		"-Iinc", "-I", "space",
		"-o/tmp/test", NULL
	};
	char optstring[] = "hvwpl:I:o:";
	char *libs[] = {"cepl", "ssl", "tap", NULL};
	char *const *result;

	for (; argv[argc]; argc++);
	result = parse_opts(argc, argv, optstring, &ofile);
	printf("%s\n%s", "# generated compiler string: ", "# ");
	for (int i = 0; result[i]; (printf("%s ", result[i]), i++));
	putchar('\n');

	plan(3);

	is(result[0], "gcc", "test option parsing.");
	like(result[5], "^-O2$", "test cc_argv[5] matches \"-O2\"");
	ok(parse_libs(libs) != NULL, "test library parsing.");

	done_testing();

	free_cc_argv((char **)result);

}
