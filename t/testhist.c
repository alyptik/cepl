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

/* test line */
char *line;
struct prog_src prog[2];
extern bool track_flag;

int main (void)
{
	struct var_list vars = {0, 0, NULL};
	char const optstring[] = "hptvwc:l:I:o:";
	int argc = 0;
	FILE volatile *output = NULL;
	char tempfile[] = "/tmp/ceplXXXXXX";

	int tmp_fd;
	if ((tmp_fd = mkstemp(tempfile)) == -1) {
		WARN("mkstemp()");
		memset(tempfile, 0, sizeof tempfile);
		memcpy(tempfile, "./ceplXXXXXX", strlen("./ceplXXXXXX") + 1);
		WARNX("attempting to create a tmpfile in ./ instead");
		if ((tmp_fd = mkstemp(tempfile)) == -1)
			ERR("mkstemp()");
	}
	char *argv[] = {
		"cepl", "-lssl", "-I.",
		"-c", "gcc", "-o", tempfile, NULL
	};
	track_flag = false;
	/* increment argument count */
	for (argc = 0; argv[argc]; argc++);

	plan(16);

	if (!(line = calloc(1, COUNT)))
		ERR("line calloc()");
	using_history();
	/* compiler arg array */
	char **cc_args = parse_opts(argc, argv, optstring, &output);
	strmv(0, line, "int foobar");

	/* initialize source buffers */
	lives_ok({init_buffers(&vars, &prog, &line);}, "test buffer initialization.");
	/* initiatalize compiler arg array */
	ok(cc_args != NULL, "test for non-NULL parse_opts() return.");
	lives_ok({build_final(&prog, &vars, argv);}, "test initial program build success.");

	/* re-allocate enough memory for line + '\t' + ';' + '\n' + '\0' */
	ok(resize_buffer(&prog[0].body, &prog[0].b_sz, &prog[0].b_max, 3) != 0, "test `b_sz[0] != 0`.");
	ok(resize_buffer(&prog[0].total, &prog[0].t_sz, &prog[0].t_max, 3) != 0, "test `t_sz[0] != 0`.");
	/* re-allocate enough memory for line + '\t' + ';' + '\n' + '\0' */
	ok(resize_buffer(&prog[1].body, &prog[1].b_sz, &prog[1].b_max, 3) != 0, "test `b_sz[1] != 0`.");
	ok(resize_buffer(&prog[1].total, &prog[1].t_sz, &prog[1].t_max, 3) != 0, "test `t_sz[1] != 0`.");
	ok(resize_buffer(&prog[1].funcs, &prog[1].f_sz, &prog[1].f_max, 3) != 0, "test `f_sz != 0`.");

	lives_ok({build_body(&prog, line);}, "test program body build success.");
	/* add line endings */
	for (size_t i = 0; i < 2; i++)
		strmv(CONCAT, prog[i].body, ";\n");

	lives_ok({build_final(&prog, &vars, argv);}, "test final program build success.");
	ok((compile(prog[1].total, cc_args, argv)) == 0, "test successful program compilation.");

	for (size_t i = 0; i < 2; i++)
		lives_ok({pop_history(&prog[i]);}, "test pop_history() prog[%zu] call.", i);
	lives_ok({build_final(&prog, &vars, argv);}, "test secondary program build success.");

	lives_ok({free_buffers(&prog, &line);}, "test successful free_buffers() call.");

	/* redirect stdout to /dev/null */
	int saved_fd[2], null_fd;
	if (!(null_fd = open("/dev/null", O_WRONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)))
		ERR("/dev/null open()");
	saved_fd[0] = dup(STDOUT_FILENO);
	saved_fd[1] = dup(STDERR_FILENO);
	dup2(null_fd, STDOUT_FILENO);
	dup2(null_fd, STDERR_FILENO);
	lives_ok({cleanup();}, "test successful cleanup() call.");
	dup2(saved_fd[0], STDOUT_FILENO);
	dup2(saved_fd[1], STDERR_FILENO);
	close(null_fd);

	/* cleanup */
	if (vars.list) {
		for (size_t i = 0; i < vars.cnt; i++) {
			if (vars.list[i].key) {
				free(vars.list[i].key);
				vars.list[i].key = NULL;
			}
		}
		free(vars.list);
		vars.list = NULL;
	}
	free_argv(&cc_args);
	close(tmp_fd);
	if (remove(tempfile) == -1)
		WARN("remove(tempfile)");

	done_testing();
}
