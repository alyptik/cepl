/*
 * t/testhist.c - unit-test for hist.c
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#include "tap.h"
#include "../src/hist.h"
#include "../src/parseopts.h"

/* silence linter */
int mkstemp(char *__template);

/* globals */
struct str_list comp_list;
char *input_src[3];
/* global completion list struct */
char *const cc_arg_list[] = {
	"-O0", "-pipe", "-fPIC",
	"-fverbose-asm", "-std=c11",
	"-S", "-xc", "/dev/stdin",
	"-o", "/dev/stdout",
	NULL
};
char *const ld_arg_list[] = {
	"-O0", "-pipe",
	"-fPIC", "-no-pie",
	"-xassembler", "/dev/stdin",
	"-lm", "-o", "/dev/stdout",
	NULL
};
char *const warn_list[] = {
	"-Wall", "-Wextra",
	"-Wno-unused",
	"-pedantic-errors",
	NULL
};
/* tty state globals */
int have_modes = 0;
struct termio save_modes[4] = {0};

static char asm_error[] = "/a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/p/q/r/s/t/u/v/w/s/y/z";
static char *argv[] = {"cepl", NULL};

/* print_vars() stub */
int print_vars(struct program *restrict prog, char *const *restrict cc_args, char **exec_args)
{
	(void)prog, (void)cc_args, (void)exec_args;
	return 0;
}

int main (void)
{
	int saved_fd = dup(STDERR_FILENO);
	struct program prg = {0};
	plan(15);

	using_history();
	xcalloc(char, &prg.cur_line, 1, EVAL_LIMIT, "lptr calloc()");
	strmv(0, prg.cur_line, "int foobar");
	prg.asm_filename = asm_error;

	/* initialize source buffers */
	lives_ok({init_buffers(&prg);}, "test buffer initialization.");
	/* initiatalize compiler arg array */
	/* re-allocate enough memory for line + '\t' + ';' + '\n' + '\0' */
	lives_ok({build_final(&prg, argv);}, "test initial program build success.");
	ok(rsz_buf(&prg, &prg.src[0].body, &prg.src[0].body_size, &prg.src[0].body_max, 3), "body_sz[0] != 0");
	ok(rsz_buf(&prg, &prg.src[0].total, &prg.src[0].total_size, &prg.src[0].total_max, 3), "total_sz[0] != 0");
	/* re-allocate enough memory for line + '\t' + ';' + '\n' + '\0' */
	ok(rsz_buf(&prg, &prg.src[1].body, &prg.src[1].body_size, &prg.src[1].body_max, 3), "gbody_sz[1] != 0");
	ok(rsz_buf(&prg, &prg.src[1].total, &prg.src[1].total_size, &prg.src[1].total_max, 3), "gtotal_sz[1] != 0");
	ok(rsz_buf(&prg, &prg.src[1].funcs, &prg.src[1].funcs_size, &prg.src[1].funcs_max, 3), "gfuncs_sz != 0");
	lives_ok({build_body(&prg);}, "test program body build success.");

	/* add lptr endings */
	for (size_t i = 0; i < 2; i++)
		strmv(CONCAT, prg.src[i].body, ";\n");
	lives_ok({build_final(&prg, argv);}, "test final program build success.");
	for (size_t i = 0; i < 2; i++)
		lives_ok({pop_history(&prg.src[i]);}, "test pop_history() prog[%zu] call.", i);
	lives_ok({build_final(&prg, argv);}, "test secondary program build success.");

	/* cleanup */
	close(STDERR_FILENO);
	ok(write_asm(&prg, cc_arg_list) == -1, "test return of -1 on failed `write_asm()`.");
	dup2(saved_fd, STDERR_FILENO);
	prg.asm_filename = NULL;
	lives_ok({free_buffers(&prg);}, "test successful free_buffers() call.");
	saved_fd = dup(STDIN_FILENO);
	close(STDIN_FILENO);
	lives_ok({cleanup(&prg);}, "test successful cleanup() call.");
	dup2(saved_fd, STDIN_FILENO);

	done_testing();
}
