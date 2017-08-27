/*
 * t/testparseopts.c - unit-test for parseopts.c
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#include "tap.h"
#include "../src/errors.h"
#include "../src/parseopts.h"

/* silence linter */
int mkstemp(char *template);

int main (void)
{
	char tempfile[] = "/tmp/ceplXXXXXX";
	int tmp_fd;
	if ((tmp_fd = mkstemp(tempfile)) == -1) {
		WARNGEN("mkstemp()");
		memset(tempfile, 0, sizeof tempfile);
		memcpy(tempfile, "./ceplXXXXXX", strlen("./ceplXXXXXX") + 1);
		WARNX("attempting to create a tmpfile in ./ instead");
		if ((tmp_fd = mkstemp(tempfile)) == -1)
			ERRGEN("mkstemp()");
	}
	volatile FILE *ofile = NULL;
	int argc;
	char *argv[] = {
		"cepl", "-lssl", "-I.",
		"-c", "gcc", "-o", tempfile, NULL
	};
	char const optstring[] = "hptvwc:l:I:o:";
	char *libs[] = {"ssl", "readline", NULL};
	char **result;
	ssize_t ret;
	struct str_list symbols = {.cnt = 0, .list = NULL};

	/* print argument strings */
	for (argc = 0; argv[argc]; argc++);
	result = parse_opts(argc, argv, optstring, &ofile);
	printf("%s\n%s", "# generated compiler string: ", "# ");
	for (int i = 0; result[i]; i++)
		printf("%s ", result[i]);
	putchar('\n');
	printf("%s\n%s", "# using argv string: ", "# ");
	for (int i = 0; i < argc; i++)
		printf("%s ", argv[i]);
	putchar('\n');
	init_list(&symbols, "cepl");

	plan(6);

	ok(result != NULL, "test option parsing.");
	like(result[0], "^(gcc|clang)$", "test generation of compiler string.");
	lives_ok({read_syms(&symbols, NULL);}, "test passing read_syms() empty filename.");
	lives_ok({parse_libs(&symbols, libs);}, "test shared library parsing.");
	ok((ret = free_str_list(&symbols)) != -1, "test free_str_list() doesn't return -1.");
	ok(ret == 1, "test free_str_list() return is exactly 1.");

	/* cleanup */
	free_argv(result);
	close(tmp_fd);
	if (remove(tempfile) == -1)
		WARNGEN("remove()");

	done_testing();
}
