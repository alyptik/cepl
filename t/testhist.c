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

/* global linker arguments struct */
char *const cc_arg_list[] = {
	"-O0", "-pipe",
	"-fPIC", "-std=c11",
	"-Wno-unused-parameter",
	"-S", "-xc", "/dev/stdin",
	"-o", "/dev/stdout", NULL
};
char *const ld_arg_list[] = {
	"-O0", "-pipe",
	"-fPIC", "-std=c11",
	"-Wno-unused-parameter",
	"-xassembler", "/dev/stdin",
	"-o", "/dev/stdout", NULL
};
char *const warn_list[] = {
	"-pedantic-errors", "-Wall", "-Wextra", NULL
};
/* global compiler arg array */
char *argv[] = {"cepl", NULL}, **cc_argv;
bool track_flag = false;
/* global completion list struct */
struct str_list comp_list;
struct prog_src prg[2];

/* static line buffer */
static char *ln;
/* static var lists */
static enum var_type *tl;
static struct var_list vl;
static struct str_list il;

/* print_vars() shim */
int print_vars(struct var_list *vlist, char const *src, char *const cc_args[], char *const exec_args[])
{
	(void)vlist, (void)src, (void)cc_args, (void)exec_args;
	return 0;
}

int main (void)
{
	using_history();
	if (!(ln = calloc(1, COUNT)))
		ERR("ln calloc()");
	strmv(0, ln, "int foobar");

	plan(14);

	/* initialize source buffers */
	lives_ok({init_buffers(&vl, &tl, &il, &prg, &ln);}, "test buffer initialization.");
	/* initiatalize compiler arg array */
	lives_ok({build_final(&prg, &vl, argv);}, "test initial program build success.");
	/* re-allocate enough memory for line + '\t' + ';' + '\n' + '\0' */
	ok((rsz_buf(&prg[0].b, &prg[0].b_sz, &prg[0].b_max, 3, &vl, &tl, &il, &prg, &ln)), "b_sz[0] != 0");
	ok((rsz_buf(&prg[0].total, &prg[0].t_sz, &prg[0].t_max, 3, &vl, &tl, &il, &prg, &ln)), "t_sz[0] != 0");
	/* re-allocate enough memory for line + '\t' + ';' + '\n' + '\0' */
	ok((rsz_buf(&prg[1].b, &prg[1].b_sz, &prg[1].b_max, 3, &vl, &tl, &il, &prg, &ln)), "gb_sz[1] != 0");
	ok((rsz_buf(&prg[1].total, &prg[1].t_sz, &prg[1].t_max, 3, &vl, &tl, &il, &prg, &ln)), "gt_sz[1] != 0");
	ok((rsz_buf(&prg[1].f, &prg[1].f_sz, &prg[1].f_max, 3, &vl, &tl, &il, &prg, &ln)), "gf_sz != 0");
	lives_ok({build_body(&prg, ln);}, "test program body build success.");

	/* add ln endings */
	for (size_t i = 0; i < 2; i++)
		strmv(CONCAT, prg[i].b, ";\n");
	lives_ok({build_final(&prg, &vl, argv);}, "test final program build success.");
	for (size_t i = 0; i < 2; i++)
		lives_ok({pop_history(&prg[i]);}, "test pop_history() prog[%zu] call.", i);
	lives_ok({build_final(&prg, &vl, argv);}, "test secondary program build success.");
	lives_ok({free_buffers(&vl, &tl, &il, &prg, &ln);}, "test successful free_buffers() call.");

	/* cleanup */
	int saved_fd = dup(STDIN_FILENO);
	close(STDIN_FILENO);
	lives_ok({cleanup();}, "test successful cleanup() call.");
	if (vl.list) {
		for (size_t i = 0; i < vl.cnt; i++)
			if (vl.list[i].key) free(vl.list[i].key);
		free(vl.list);
	}
	dup2(saved_fd, STDIN_FILENO);

	done_testing();
}
