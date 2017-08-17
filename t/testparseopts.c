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
	volatile FILE *ofile = NULL;
	int argc = 0;
	char *argv[] = {
		"cepl", "-lssl", "-I.",
		"-cgcc", "-o/tmp/test", NULL
	};
	char const optstring[] = "hvwpc:l:I:o:";
	char *libs[] = {"ssl", "readline", NULL};
	char **result;
	ssize_t ret;
	struct str_list symbols = {.cnt = 0, .list = NULL};

	for (; argv[argc]; argc++);
	result = parse_opts(argc, argv, optstring, &ofile);
	printf("%s\n%s", "# generated compiler string: ", "# ");
	for (int i = 0; result[i]; i++)
		printf("%s ", result[i]);
	putchar('\n');
	init_list(&symbols, "cepl");

	plan(6);

	ok(result != NULL, "test option parsing.");
	like(result[0], "^(gcc|clang)$", "test generation of compiler string.");
	lives_ok({read_syms(&symbols, NULL);}, "test passing read_syms() empty filename.");
	lives_ok({parse_libs(&symbols, libs);}, "test shared library parsing.");
	ok((ret = free_str_list(&symbols)) != -1, "test free_str_list() doesn't return -1.");
	ok(ret == 1, "test free_str_list() return is exactly 1.");

	done_testing();

	free_argv(result);
}
