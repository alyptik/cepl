/*
 * t/testparseopts.c - unit-test for parseopts.c
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#include "tap.h"
#include "../src/parseopts.h"

/* silence linter */
int mkstemp(char *__template);

/* global completion list */
char *comp_arg_list[] = {0};
/* global linker flags and completions structs */
struct str_list ld_list, comp_list;

int main (void)
{
	char hist_tmp[] = "/tmp/ceplXXXXXX";
	char hist_fallback[] = "./ceplXXXXXX";
	char *asm_file = NULL;
	int tmp_fd;
	if ((tmp_fd = mkstemp(hist_tmp)) == -1) {
		memset(hist_tmp, 0, sizeof hist_tmp);
		memcpy(hist_tmp, hist_fallback, sizeof hist_fallback);
		WARNMSG("mkstemp()", "attempting to create a tmpfile in ./ instead");
		if ((tmp_fd = mkstemp(hist_tmp)) == -1)
			ERR("mkstemp()");
	}
	FILE *ofile = NULL;
	char *argv[] = {
		"cepl", "-lssl", "-I.",
		"-c", "gcc", "-o", hist_tmp, NULL
	};
	int argc = sizeof argv / sizeof argv[0] - 1;
	char const optstring[] = "hptvwc:l:I:o:";
	char *libs[] = {"ssl", "readline", NULL};
	struct str_list symbols = {0};
	char **result, *out_filename = NULL;
	ptrdiff_t ret;

	/* print argument strings */
	result = parse_opts(argc, argv, optstring, &ofile, &out_filename, &asm_file);
	printf("%s\n%s", "# generated compiler string: ", "# ");
	for (int i = 0; result[i]; i++)
		printf("%s ", result[i]);
	printf("\n%s\n%s", "# using argv string: ", "# ");
	for (int i = 0; i < argc; i++)
		printf("%s ", argv[i]);
	putchar('\n');
	init_list(&symbols, "cepl");

	plan(7);

	ok(result != NULL, "test option parsing.");
	like(result[0], "^(gcc|clang)$", "test generation of compiler string.");
	lives_ok({read_syms(&symbols, NULL);}, "test passing read_syms() empty filename.");
	lives_ok({parse_libs(&symbols, libs);}, "test shared library parsing.");
	ok((ret = free_str_list(&symbols)) != -1, "test free_str_list() doesn't return -1.");
	ok(ret == 1, "test free_str_list() return is exactly 1.");
	ok(free_str_list(&symbols) == -1, "test free_str_list() on empty pointer returns -1.");

	/* cleanup */
	free_argv(&result);
	close(tmp_fd);
	if (remove(hist_tmp) == -1)
		WARN("remove()");
	if (out_filename)
		free(out_filename);

	done_testing();
}
