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
char *comp_arg_list[1];
/* global linker flags and completions structs */
STR_LIST ld_list, comp_list;

int main (void)
{
	FILE *ofile = NULL;
	char const optstring[] = "hptvwc:a:e:i:l:I:o:";
	char *libs[] = {"ssl", "readline", NULL};
	STR_LIST symbols = {0};
	char *out_filename = NULL, *asm_filename = NULL, **result;
	ptrdiff_t ret;
	char out_tmp[] = "/tmp/ceplXXXXXX";
	char out_fallback[] = "./ceplXXXXXX";
	char asm_tmp[] = "/tmp/cepl_asmXXXXXX";
	char asm_fallback[] = "./cepl_asmXXXXXX";
	int tmp_fd[2];
	if ((tmp_fd[0] = mkstemp(out_tmp)) == -1 || (tmp_fd[1] = mkstemp(asm_tmp)) == -1) {
		memset(out_tmp, 0, sizeof out_tmp);
		memcpy(out_tmp, out_fallback, sizeof out_fallback);
		WARNMSG("mkstemp()", "attempting to create a tmpfile in ./ instead");
		if ((tmp_fd[0] = mkstemp(out_tmp)) == -1)
			ERR("mkstemp() out file");
		memset(asm_tmp, 0, sizeof asm_tmp);
		memcpy(asm_tmp, asm_fallback, sizeof asm_fallback);
		WARNMSG("mkstemp()", "attempting to create a tmpfile in ./ instead");
		if ((tmp_fd[1] = mkstemp(asm_tmp)) == -1)
			ERR("mkstemp() asm file");
	}
	char *argv[] = {
		"cepl", "-lssl", "-I.",
		"-c", "gcc", "-o", out_tmp,
		"-a", asm_tmp, NULL
	};
	int argc = sizeof argv / sizeof argv[0] - 1;
	/* print argument strings */
	result = parse_opts(argc, argv, optstring, &ofile, &out_filename, &asm_filename);
	printf("%s\n%s", "# generated compiler string: ", "# ");
	for (int i = 0; result[i]; i++)
		printf("%s ", result[i]);
	printf("\n%s\n%s", "# using argv string: ", "# ");
	for (int i = 0; i < argc; i++)
		printf("%s %s", argv[i], (i == argc - 1) ? "\n" : "");
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
	close(tmp_fd[0]);
	close(tmp_fd[1]);
	if (remove(out_tmp) == -1)
		WARN("remove() out_tmp");
	if (remove(asm_tmp) == -1)
		WARN("remove() asm_tmp");
	free(out_filename);
	free(asm_filename);

	done_testing();
}
