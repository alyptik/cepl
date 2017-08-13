/*
 * t/testparseopts.c - unit-test for parseopts.c
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#include "tap.h"
#include "../src/parseopts.h"

int main (void)
{
	FILE *ofile = NULL;
	int argc = 0;
	char *argv[] = {
		"cepl", "-lssl", "-I.",
		"-cgcc", "-o/tmp/test", NULL
	};
	char const optstring[] = "hvwpc:l:I:o:";
	char *libs[] = {"ssl", "readline", NULL};
	char **result;
	struct str_list symbols = {.cnt = 0, .list = NULL};

	for (; argv[argc]; argc++);
	result = parse_opts(argc, argv, optstring, &ofile);
	printf("%s\n%s", "# generated compiler string: ", "# ");
	for (int i = 0; result[i]; i++)
		printf("%s ", result[i]);
	putchar('\n');
	init_list(&symbols, "cepl");

	plan(3);

	ok(result != NULL, "test option parsing.");
	like(result[0], "^(gcc|clang)$", "test generation of compiler string.");
	lives_ok({parse_libs(&symbols, libs);}, "test library parsing.");

	done_testing();

	free_argv(symbols.list);
	free_argv(result);
}
