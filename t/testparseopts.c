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
struct str_list comp_list;
/* source file includes template */
char const *prelude =
	"#ifndef _BSD_SOURCE\n"
	"#  define _BSD_SOURCE\n"
	"#endif\n"
	"#ifndef _DEFAULT_SOURCE\n"
	"#  define _DEFAULT_SOURCE\n"
	"#endif\n"
	"#ifndef _GNU_SOURCE\n"
	"#  define _GNU_SOURCE\n"
	"#endif\n"
	"#ifndef _POSIX_C_SOURCE\n"
	"#  define _POSIX_C_SOURCE 200809L\n"
	"#endif\n"
	"#ifndef _SVID_SOURCE\n"
	"#  define _SVID_SOURCE\n"
	"#endif\n"
	"#ifndef _XOPEN_SOURCE\n"
	"#  define _XOPEN_SOURCE 700\n"
	"#endif\n\n"
	"#ifdef __INTEL_COMPILER\n"
	"#  define float_t double\n"
	"#  define _Float32 float\n"
	"#  define _Float32x float\n"
	"#  define _Float64 double\n"
	"#  define _Float64x double\n"
	"#  define _Float128 float_t\n"
	"#else\n"
	"#  include <complex.h>\n"
	"#endif\n\n"
	"#include <assert.h>\n"
	"#include <ctype.h>\n"
	"#include <error.h>\n"
	"#include <errno.h>\n"
	"#include <float.h>\n"
	"#include <fcntl.h>\n"
	"#include <limits.h>\n"
	"#include <locale.h>\n"
	"#include <math.h>\n"
	"#include <pthread.h>\n"
	"#include <regex.h>\n"
	"#include <setjmp.h>\n"
	"#include <signal.h>\n"
	"#include <stdalign.h>\n"
	"#include <stdarg.h>\n"
	"#include <stdbool.h>\n"
	"#include <stddef.h>\n"
	"#include <stdint.h>\n"
	"#include <stdio.h>\n"
	"#include <stdlib.h>\n"
	"#include <stdnoreturn.h>\n"
	"#include <string.h>\n"
	"#include <strings.h>\n"
	"#include <sys/mman.h>\n"
	"#include <sys/types.h>\n"
	"#include <sys/sendfile.h>\n"
	"#include <sys/socket.h>\n"
	"#include <sys/syscall.h>\n"
	"#include <sys/wait.h>\n"
	"#include <time.h>\n"
	"#include <uchar.h>\n"
	"#include <wchar.h>\n"
	"#include <wctype.h>\n"
	"#include <unistd.h>\n\n"
	"extern char **environ;\n\n"
	"#line 1\n";

/* compiler pre-program */
char const *prog_start =
	"\nint main(int argc, char **argv)\n"
	"{\n"
		"\t(void)argc, (void)argv;\n";
/* pre-program shown to user */
char const *prog_start_user =
	"\nint main(int argc, char **argv)\n"
	"{\n";
/* final section */
char const *prog_end =
		"\n\treturn 0;\n"
	"}\n";

int main (void)
{
	char const optstring[] = "hptvwc:a:e:f:i:l:I:o:";
	char *libs[] = {"ssl", "readline", NULL};
	struct program prg = {0};
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
		WARN("%s\n%s", "failed calling mkstemp()", "attempting to create a tmpfile in ./ instead");
		if ((tmp_fd[0] = mkstemp(out_tmp)) == -1)
			ERR("%s", "mkstemp() out file");
		memset(asm_tmp, 0, sizeof asm_tmp);
		memcpy(asm_tmp, asm_fallback, sizeof asm_fallback);
		WARN("%s\n%s", "failed calling mkstemp()", "attempting to create a tmpfile in ./ instead");
		if ((tmp_fd[1] = mkstemp(asm_tmp)) == -1)
			ERR("%s", "mkstemp() asm file");
	}
	char *argv[] = {
		"cepl", "-lssl", "-I.",
		"-c", "gcc", "-o", out_tmp,
		"-a", asm_tmp, NULL
	};
	int argc = sizeof argv / sizeof argv[0] - 1;
	/* print argument strings */
	result = parse_opts(&prg, argc, argv, optstring);
	init_str_list(&prg.sym_list, "cepl");
	printf("%s\n%s", "# generated compiler string: ", "# ");
	for (int i = 0; result[i]; i++)
		printf("%s ", result[i]);
	printf("\n%s\n%s", "# using argv string: ", "# ");
	for (int i = 0; i < argc; i++)
		printf("%s %s", argv[i], (i == argc - 1) ? "\n" : "");

	plan(7);

	ok(result != NULL, "test option parsing.");
	like(result[0], "^(gcc|clang)$", "test generation of compiler string.");
	lives_ok({read_syms(&prg.sym_list, NULL);}, "test passing read_syms() empty filename.");
	lives_ok({parse_libs(&prg.sym_list, libs);}, "test shared library parsing.");
	ok((ret = free_str_list(&prg.sym_list)) != -1, "test free_str_list() doesn't return -1.");
	ok(ret == 1, "test free_str_list() return is exactly 1.");
	ok(free_str_list(&prg.sym_list) == -1, "test free_str_list() on empty pointer returns -1.");

	/* cleanup */
	free_argv(&result);
	close(tmp_fd[0]);
	close(tmp_fd[1]);
	if (remove(out_tmp) == -1)
		WARN("%s", "remove() out_tmp");
	if (remove(asm_tmp) == -1)
		WARN("%s", "remove() asm_tmp");
	free(out_filename);
	free(asm_filename);

	done_testing();
}
