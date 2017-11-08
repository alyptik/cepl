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

/* global compiler arg array */
char **cc_argv;
bool eval_flag = false, track_flag = false;
/* global completion list struct */
STR_LIST comp_list;
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
/* static line buffer */
char *lptr;

/* static var lists */
static TYPE_LIST tl;
static VAR_LIST vl;
static STR_LIST il;
static char asm_error[] = "/a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/p/q/r/s/t/u/v/w/s/y/z";
static char *argv[] = {"cepl", NULL};

/* externs */
extern PROG_SRC prg[2];
extern char *asm_filename;

/* print_vars() shim */
int print_vars(VAR_LIST *restrict vlist, char const *restrict src, char *const cc_args[], char *const exec_args[])
{
	(void)vlist, (void)src, (void)cc_args, (void)exec_args;
	return 0;
}

int main (void)
{
	int saved_fd = dup(STDERR_FILENO);

	plan(15);

	using_history();
	if (!(lptr = calloc(1, EVAL_LIMIT)))
		ERR("lptr calloc()");
	strmv(0, lptr, "int foobar");
	asm_filename = asm_error;

	/* initialize source buffers */
	lives_ok({init_buffers(&vl, &tl, &il, &prg, &lptr);}, "test buffer initialization.");
	/* initiatalize compiler arg array */
	lives_ok({build_final(&prg, &vl, argv);}, "test initial program build success.");
	/* re-allocate enough memory for line + '\t' + ';' + '\n' + '\0' */
	ok((rsz_buf(&prg[0].b, &prg[0].b_sz, &prg[0].b_max, 3, &vl, &tl, &il, &prg, &lptr)), "b_sz[0] != 0");
	ok((rsz_buf(&prg[0].total, &prg[0].t_sz, &prg[0].t_max, 3, &vl, &tl, &il, &prg, &lptr)), "t_sz[0] != 0");
	/* re-allocate enough memory for line + '\t' + ';' + '\n' + '\0' */
	ok((rsz_buf(&prg[1].b, &prg[1].b_sz, &prg[1].b_max, 3, &vl, &tl, &il, &prg, &lptr)), "gb_sz[1] != 0");
	ok((rsz_buf(&prg[1].total, &prg[1].t_sz, &prg[1].t_max, 3, &vl, &tl, &il, &prg, &lptr)), "gt_sz[1] != 0");
	ok((rsz_buf(&prg[1].f, &prg[1].f_sz, &prg[1].f_max, 3, &vl, &tl, &il, &prg, &lptr)), "gf_sz != 0");
	lives_ok({build_body(&prg, lptr);}, "test program body build success.");

	/* add lptr endings */
	for (size_t i = 0; i < 2; i++)
		strmv(CONCAT, prg[i].b, ";\n");
	lives_ok({build_final(&prg, &vl, argv);}, "test final program build success.");
	for (size_t i = 0; i < 2; i++)
		lives_ok({pop_history(&prg[i]);}, "test pop_history() prog[%zu] call.", i);
	lives_ok({build_final(&prg, &vl, argv);}, "test secondary program build success.");

	/* cleanup */
	close(STDERR_FILENO);
	ok(write_asm(&prg, cc_arg_list) == -1, "test return of -1 on failed `write_asm()`.");
	dup2(saved_fd, STDERR_FILENO);
	asm_filename = NULL;
	lives_ok({free_buffers(&vl, &tl, &il, &prg, &lptr);}, "test successful free_buffers() call.");
	saved_fd = dup(STDIN_FILENO);
	close(STDIN_FILENO);
	lives_ok({cleanup();}, "test successful cleanup() call.");
	dup2(saved_fd, STDIN_FILENO);

	done_testing();
}
