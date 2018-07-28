/*
 * hist.c - history handling functions
 *
 * AUTHOR: Joey Pabalinas <joeypabalinas@gmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#include "hist.h"

/* externs */
extern struct str_list comp_list;

/* source file includes template */
char const *prologue =
	"#undef _BSD_SOURCE\n"
	"#define _BSD_SOURCE\n"
	"#undef _DEFAULT_SOURCE\n"
	"#define _DEFAULT_SOURCE\n"
	"#undef _GNU_SOURCE\n"
	"#define _GNU_SOURCE\n"
	"#undef _POSIX_C_SOURCE\n"
	"#define _POSIX_C_SOURCE 200809L\n"
	"#undef _SVID_SOURCE\n"
	"#define _SVID_SOURCE\n"
	"#undef _XOPEN_SOURCE\n"
	"#define _XOPEN_SOURCE 700\n\n"
	"#if defined(__INTEL_COMPILER)\n"
	"	typedef float _Float32;\n"
	"	typedef float _Float32x;\n"
	"	typedef double _Float64;\n"
	"	typedef double _Float64x;\n"
	"	typedef long double _Float128;\n"
	"	typedef enum ___LIB_VERSIONIMF_TYPE {\n"
	"		_IEEE_ = -1	/* IEEE-like behavior */\n"
	"		,_SVID_		/* SysV, Rel. 4 behavior */\n"
	"		,_XOPEN_	/* Unix98 */\n"
	"		,_POSIX_	/* POSIX */\n"
	"		,_ISOC_		/* ISO C9X */\n"
	"	} _LIB_VERSIONIMF_TYPE;\n"
	"# define _LIB_VERSION_TYPE _LIB_VERSIONIMF_TYPE;\n"
	"#else\n"
	"# include <complex.h>\n"
	"#endif /* defined(__INTEL_COMPILER) */\n"
	"#include <arpa/inet.h>\n"
	"#include <assert.h>\n"
	"#include <ctype.h>\n"
	"#include <error.h>\n"
	"#include <errno.h>\n"
	"#include <float.h>\n"
	"#include <fcntl.h>\n"
	"#include <limits.h>\n"
	"#include <locale.h>\n"
	"#include <math.h>\n"
	"#include <netdb.h>\n"
	"#include <netinet/in.h>\n"
	"#include <netinet/ip.h>\n"
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
	"#include <sys/types.h>\n"
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

void cleanup(struct program *restrict prog)
{
	/* readline teardown */
	rl_free_line_state();
	/* avoid segfault when stdin is not a tty */
	if (isatty(STDIN_FILENO))
		rl_cleanup_after_signal();
	/* free generated completions */
	free_str_list(&comp_list);
	free(prog->hist_file);
	prog->hist_file = NULL;
	free(prog->out_filename);
	prog->out_filename = NULL;
	free(prog->asm_filename);
	prog->asm_filename = NULL;
	for (size_t i = 0; i < ARR_LEN(prog->input_src); i++) {
		free(prog->input_src[i]);
		prog->input_src[i] = NULL;
	}
	if (isatty(STDIN_FILENO) && !prog->sflags.eval_flag)
		printf("\n%s\n\n", "Terminating program.");
}

int write_asm(struct program *restrict prog, char *const *restrict cc_args)
{
	/* return early if no file open */
	if (!prog->sflags.asm_flag || !prog->asm_filename || !*prog->asm_filename || !prog->src[1].total.buf)
		return -1;

	size_t buf_len = strlen(prog->src[1].total.buf) + 1;
	int pipe_cc[2], asm_fd, status;
	char src_buffer[buf_len];

	if (buf_len < 2)
		ERRX("%s", "empty source passed to write_asm()");
	/* add trailing '\n' */
	memcpy(src_buffer, prog->src[1].total.buf, buf_len - 1);
	src_buffer[buf_len - 1] = '\n';
	/* create pipe */
	if (pipe2(pipe_cc, O_CLOEXEC) < 0)
		ERR("%s", "error making pipe_cc pipe");
	if ((asm_fd = open(prog->asm_filename, O_WRONLY|O_CREAT|O_TRUNC, S_IWUSR|S_IRUSR|S_IRGRP|S_IROTH)) < 0) {
		close(pipe_cc[0]);
		close(pipe_cc[1]);
		WARN("%s", "error opening asm output file");
		return -1;
	}

	/* fork compiler */
	switch (fork()) {
	/* error */
	case -1:
		close(pipe_cc[0]);
		close(pipe_cc[1]);
		close(asm_fd);
		ERR("%s", "error forking compiler");
		break;

	/* child */
	case 0:
		reset_handlers();
		dup2(pipe_cc[0], STDIN_FILENO);
		dup2(asm_fd, STDOUT_FILENO);
		execvp(cc_args[0], cc_args);
		/* execvp() should never return */
		ERR("%s", "error forking compiler");
		break;

	/* parent */
	default:
		close(pipe_cc[0]);
		if (write(pipe_cc[1], src_buffer, sizeof src_buffer) < 0)
			ERR("%s", "error writing to pipe_cc[1]");
		close(pipe_cc[1]);
		wait(&status);
		/* convert 255 to -1 since WEXITSTATUS() only returns the low-order 8 bits */
		if (WIFEXITED(status) && WEXITSTATUS(status)) {
			WARNX("%s", "compiler returned non-zero exit code");
			return (WEXITSTATUS(status) != 0xff) ? WEXITSTATUS(status) : -1;
		}
	}

	fsync(asm_fd);
	close(asm_fd);
	/* program returned success */
	return 0;
}

void write_files(struct program *restrict prog)
{
	int out_fd;
	size_t buf_len, buf_pos;

	/* write out history/asm output */
	if (prog->sflags.hist_flag && write_history(prog->hist_file))
		WARN("%s", "write_history()");
	write_asm(prog, prog->cc_list.list);
	/* return early if no file open */
	if (!prog->sflags.out_flag || !prog->ofile || !prog->src[1].total.buf)
		return;
	if ((out_fd = fileno(prog->ofile)) < 0)
		return;
	/* find buffer length */
	for (buf_pos = 0, buf_len = 0; prog->src[1].total.buf[buf_pos]; buf_pos++);
	buf_len = buf_pos;
	buf_pos = 0;

	/* write out program to file */
	for (;;) {
		ssize_t ret;
		if ((ret = write(out_fd, prog->src[1].total.buf + buf_pos, buf_len - buf_pos)) < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			WARN("%s", "error writing to output fd");
			break;
		}
		/* break on EOF */
		if (!ret)
			break;
		buf_pos += ret;
	}
	fsync(out_fd);
	fclose(prog->ofile);
	prog->ofile = NULL;
}

void free_buffers(struct program *restrict prog)
{
	/* write out history/asm before freeing buffers */
	write_files(prog);
	/* clean up user data */
	free(prog->cur_line);
	prog->cur_line = NULL;
	free(prog->type_list.list);
	prog->type_list.list = NULL;
	free_str_list(&prog->id_list);
	free_str_list(&prog->cc_list);
	if (prog->var_list.list) {
		for (size_t i = 0; i < prog->var_list.cnt; i++)
			free(prog->var_list.list[i].id);
		free(prog->var_list.list);
	}
	/* free program structs */
	for (size_t i = 0; i < 2; i++) {
		free(prog->src[i].funcs.buf);
		free(prog->src[i].body.buf);
		free(prog->src[i].total.buf);
		free(prog->src[i].flags.list);
		free_str_list(&prog->src[i].hist);
		free_str_list(&prog->src[i].lines);
		prog->src[i].body.size = prog->src[i].funcs.size = prog->src[i].total.size = 0;
		prog->src[i].body.max = prog->src[i].funcs.max = prog->src[i].total.max = 1;
		prog->src[i].body.buf = prog->src[i].funcs.buf = prog->src[i].total.buf = NULL;
		prog->src[i].flags.list = NULL;
	}
}

void init_buffers(struct program *restrict prog)
{
	/* user is truncated source for display */
	xcalloc(char, &prog->src[0].funcs.buf, 1, 1, "init");
	xcalloc(char, &prog->src[0].body.buf, 1, strlen(prog_start_user) + 1, "init");
	xcalloc(char, &prog->src[0].total.buf, 1,
			strlen(prologue)
			+ strlen(prog_start_user)
			+ strlen(prog_end) + 3, "init");
	prog->src[0].funcs.size = prog->src[0].funcs.max = 1;
	prog->src[0].body.size = prog->src[0].body.max = strlen(prog_start_user) + 1;
	prog->src[0].total.size = prog->src[0].total.max = strlen(prologue)
			+ strlen(prog_start_user)
			+ strlen(prog_end) + 3;
	/* actual is source passed to compiler */
	xcalloc(char, &prog->src[1].funcs.buf, 1, strlen(prologue) + 1, "init");
	xcalloc(char, &prog->src[1].body.buf, 1, strlen(prog_start) + 1, "init");
	xcalloc(char, &prog->src[1].total.buf, 1, strlen(prologue)
			+ strlen(prog_start)
			+ strlen(prog_end) + 3, "init");
	prog->src[1].funcs.size = prog->src[1].funcs.max = strlen(prologue) + 1;
	prog->src[1].body.size = prog->src[1].body.max = strlen(prog_start) + 1;
	prog->src[1].total.size = prog->src[1].total.max = strlen(prologue)
			+ strlen(prog_start)
			+ strlen(prog_end) + 3;
	/* sanity check */
	for (size_t i = 0; i < 2; i++) {
		if (!prog->src[i].funcs.buf || !prog->src[i].body.buf || !prog->src[i].total.buf) {
			free_buffers(prog);
			cleanup(prog);
			ERR("%s", "prgm[2] calloc()");
		}
	}
	/* no memcpy for prgm[0].funcs */
	strmv(0, prog->src[0].body.buf, prog_start_user);
	strmv(0, prog->src[1].funcs.buf, prologue);
	strmv(0, prog->src[1].body.buf, prog_start);
	/* init source history and flag lists */
	for (size_t i = 0; i < 2; i++) {
		init_str_list(&prog->src[i].lines, "FOOBARTHISVALUEDOESNTMATTERTROLLOLOLOL");
		init_str_list(&prog->src[i].hist, "FOOBARTHISVALUEDOESNTMATTERTROLLOLOLOL");
		init_flag_list(&prog->src[i].flags);
	}
	init_var_list(&prog->var_list);
	init_type_list(&prog->type_list);
	init_str_list(&prog->id_list, "FOOBARTHISVALUEDOESNTMATTERTROLLOLOLOL");
}

size_t resize_sect(struct program *restrict prog, struct source_section *restrict sect, size_t off)
{
	/* sanity check */
	if (!sect->buf || !prog->cur_line)
		return 0;
	size_t alloc_sz = strlen(sect->buf) + strlen(prog->cur_line) + off + 1;
	if (!sect->size || !sect->max) {
		/* current length + line length + extra characters + \0 */
		xrealloc(char, &sect->buf, alloc_sz, "rsz_buf()");
		return alloc_sz;
	}
	sect->size += alloc_sz;
	/* realloc only if b_max is less than current size */
	if (sect->size < sect->max)
		return 0;
	/* double until size is reached */
	while ((sect->max <<= 1) < sect->size);
	/* current length + line length + extra characters + \0 */
	xrealloc(char, &sect->buf, sect->max, "rsz_buf()");
	return sect->size;
}

void pop_history(struct program *restrict prog)
{
	for (size_t i = 0; i < 2; i++) {
		switch(prog->src[i].flags.list[--prog->src[i].flags.cnt]) {
		case NOT_IN_MAIN:
			prog->src[i].hist.cnt = prog->src[i].lines.cnt = prog->src[i].flags.cnt;
			strmv(0, prog->src[i].funcs.buf, prog->src[i].hist.list[prog->src[i].hist.cnt]);
			free(prog->src[i].hist.list[prog->src[i].hist.cnt]);
			free(prog->src[i].lines.list[prog->src[i].lines.cnt]);
			prog->src[i].hist.list[prog->src[i].hist.cnt] = NULL;
			prog->src[i].lines.list[prog->src[i].lines.cnt] = NULL;
			break;
		case IN_MAIN:
			prog->src[i].hist.cnt = prog->src[i].lines.cnt = prog->src[i].flags.cnt;
			strmv(0, prog->src[i].body.buf, prog->src[i].hist.list[prog->src[i].hist.cnt]);
			free(prog->src[i].hist.list[prog->src[i].hist.cnt]);
			free(prog->src[i].lines.list[prog->src[i].lines.cnt]);
			prog->src[i].hist.list[prog->src[i].hist.cnt] = NULL;
			prog->src[i].lines.list[prog->src[i].lines.cnt] = NULL;
			break;
		case EMPTY: /* fallthrough */
		default:
			/* revert decrement */
			prog->src[i].flags.cnt++;
		}
	}
}

void build_body(struct program *restrict prog)
{
	/* sanity check */
	if (!prog || !prog->cur_line) {
		WARNX("%s", "NULL pointer passed to build_body()");
		return;
	}
	for (size_t i = 0; i < 2; i++) {
		append_str(&prog->src[i].lines, prog->cur_line, 0);
		append_str(&prog->src[i].hist, prog->src[i].body.buf, 0);
		append_flag(&prog->src[i].flags, IN_MAIN);
		strmv(CONCAT, prog->src[i].body.buf, "\t");
		strmv(CONCAT, prog->src[i].body.buf, prog->cur_line);
	}
}

void build_funcs(struct program *restrict prog)
{
	/* sanity check */
	if (!prog || !prog->cur_line) {
		WARNX("%s", "NULL pointer passed to build_funcs()");
		return;
	}
	for (size_t i = 0; i < 2; i++) {
		append_str(&prog->src[i].lines, prog->cur_line, 0);
		append_str(&prog->src[i].hist, prog->src[i].funcs.buf, 0);
		append_flag(&prog->src[i].flags, NOT_IN_MAIN);
		/* generate function buffers */
		strmv(CONCAT, prog->src[i].funcs.buf, prog->cur_line);
	}
}

void build_final(struct program *restrict prog, char **argv)
{
	/* sanity check */
	if (!prog || !argv) {
		WARNX("%s", "NULL pointer passed to build_final()");
		return;
	}
	/* finish building current iteration of source code */
	for (size_t i = 0; i < 2; i++) {
		strmv(0, prog->src[i].total.buf, prog->src[i].funcs.buf);
		strmv(CONCAT, prog->src[i].total.buf, prog->src[i].body.buf);
		/* print variable values */
		if (prog->sflags.track_flag && i == 1)
			print_vars(prog, prog->cc_list.list, argv);
		strmv(CONCAT, prog->src[i].total.buf, prog_end);
	}
}
