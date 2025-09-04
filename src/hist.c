/*
 * hist.c - history handling functions
 *
 * AUTHOR: Joey Pabalinas <joeypabalinas@gmail.com>
 * See LICENSE file for copyright and license details.
 */

#include "hist.h"

/* externs */
extern struct str_list comp_list;

/* source file includes templates */
char const *prologue = NULL;
char const *c_prologue =
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
	"#include <asm/types.h>\n\n"
	"#include <arpa/inet.h>\n"
	"#include <assert.h>\n"
	"#include <complex.h>\n"
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
char const *cxx_prologue =
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
	"#include <algorithm>\n"
	"#include <any>\n"
	"#include <atomic>\n"
	"#include <barrier>\n"
	"#include <bitset>\n"
	"#include <cassert>\n"
	"#include <cctype>\n"
	"#include <cerrno>\n"
	"#include <cfloat>\n"
	"#include <cinttypes>\n"
	"#include <climits>\n"
	"#include <cmath>\n"
	"#include <complex>\n"
	"#include <condition_variable>\n"
	"#include <coroutine>\n"
	"#include <csetjmp>\n"
	"#include <csignal>\n"
	"#include <cstdarg>\n"
	"#include <cstddef>\n"
	"#include <cstdint>\n"
	"#include <cstdio>\n"
	"#include <cstdlib>\n"
	"#include <cstring>\n"
	"#include <ctime>\n"
	"#include <exception>\n"
	"#include <filesystem>\n"
	"#include <fstream>\n"
	"#include <future>\n"
	"#include <ios>\n"
	"#include <iostream>\n"
	"#include <iterator>\n"
	"#include <limits>\n"
	"#include <list>\n"
	"#include <map>\n"
	"#include <memory>\n"
	"#include <mutex>\n"
	"#include <numbers>\n"
	"#include <queue>\n"
	"#include <random>\n"
	"#include <regex>\n"
	"#include <semaphore>\n"
	"#include <set>\n"
	"#include <source_location>\n"
	"#include <stack>\n"
	"#include <string>\n"
	"#include <thread>\n"
	"#include <typeinfo>\n"
	"#include <type_traits>\n"
	"#include <utility>\n"
	"#include <vector>\n\n"
	"extern char **environ;\n\n"
	"using namespace std;\n\n"
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

void cleanup(struct program *prog)
{
	/* avoid segfault when stdin is not a tty */
	if (isatty(STDIN_FILENO)) {
		/* readline teardown */
		rl_free_line_state();
		rl_cleanup_after_signal();
	}
	/* free generated completions */
	free_str_list(&comp_list);
	free(prog->hist_file);
	prog->hist_file = NULL;
	free(prog->out_filename);
	prog->out_filename = NULL;
	for (size_t i = 0; i < arr_len(prog->input_src); i++) {
		free(prog->input_src[i]);
		prog->input_src[i] = NULL;
	}
	if (isatty(STDIN_FILENO) && !(prog->state_flags & EVAL_FLAG))
		printf("\n%s\n\n", "Terminating program.");
}

void write_files(struct program *prog)
{
	int out_fd;
	size_t buf_len, buf_pos;

	/* write out history */
	if (prog->state_flags & HIST_FLAG)
		write_history(prog->hist_file);

	/* return early if no file open */
	if (!(prog->state_flags & OUT_FLAG) || !prog->ofile || !prog->src[1].total.buf)
		return;
	if ((out_fd = fileno(prog->ofile)) < 0)
		return;
	/* find buffer length */
	for (buf_pos = 0; prog->src[1].total.buf[buf_pos]; buf_pos++);
	buf_len = buf_pos;
	buf_pos = 0;

	/* write out program to file */
	for (;;) {
		ssize_t ret;
		if ((ret = write(out_fd, prog->src[1].total.buf + buf_pos, buf_len - buf_pos)) < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			WARN("error writing to output fd");
			break;
		}
		/* break on EOF */
		if (!ret)
			break;
		buf_pos += ret;
	}
	fsync(out_fd);
	xfclose(&prog->ofile);
	prog->ofile = NULL;
}

void free_buffers(struct program *prog)
{
	/* write out history/asm before freeing buffers */
	write_files(prog);
	/* clean up user data */
	free(prog->cur_line);
	prog->cur_line = NULL;
	free_str_list(&prog->id_list);
	free_str_list(&prog->cc_list);
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

void init_buffers(struct program *prog)
{
	/* use appropriate prologue for compiler type (c or c++) */
	if (prog->cc_list.list[0][strlen(prog->cc_list.list[0]) - 1] == '+')
		prologue = cxx_prologue;
	else
		prologue = c_prologue;

	/* user is truncated source for display */
	xcalloc(&prog->src[0].funcs.buf, 1, 1, "init()");
	xcalloc(&prog->src[0].body.buf, 1, strlen(prog_start_user) + 1, "init()");
	xcalloc(&prog->src[0].total.buf, 1,
			strlen(prologue)
			+ strlen(prog_start_user)
			+ strlen(prog_end) + 3, "init()");
	prog->src[0].funcs.size = prog->src[0].funcs.max = 1;
	prog->src[0].body.size = prog->src[0].body.max = strlen(prog_start_user) + 1;
	prog->src[0].total.size = prog->src[0].total.max = strlen(prologue)
			+ strlen(prog_start_user)
			+ strlen(prog_end) + 3;
	/* actual is source passed to compiler */
	xcalloc(&prog->src[1].funcs.buf, 1, strlen(prologue) + 1, "init()");
	xcalloc(&prog->src[1].body.buf, 1, strlen(prog_start) + 1, "init()");
	xcalloc(&prog->src[1].total.buf, 1, strlen(prologue)
			+ strlen(prog_start)
			+ strlen(prog_end) + 3, "init()");
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
			ERR("prog[2] calloc()");
		}
	}
	/* no memcpy for prgm[0].funcs */
	strmv(0, prog->src[0].body.buf, prog_start_user);
	strmv(0, prog->src[1].funcs.buf, prologue);
	strmv(0, prog->src[1].body.buf, prog_start);
	/* init source history and flag lists */
	for (size_t i = 0; i < 2; i++) {
		init_str_list(&prog->src[i].lines, "initial");
		init_str_list(&prog->src[i].hist, "initial");
		init_flag_list(&prog->src[i].flags);
	}
}

size_t resize_sect(struct program *prog, struct source_section *sect, size_t off)
{
	/* sanity check */
	if (!sect->buf || !prog->cur_line)
		return 0;
	size_t alloc_sz = strlen(sect->buf) + strlen(prog->cur_line) + off + 1;
	if (!sect->size || !sect->max) {
		/* current length + line length + extra characters + \0 */
		xrealloc(&sect->buf, alloc_sz, "rsz_buf()");
		return alloc_sz;
	}
	sect->size += alloc_sz;
	/* realloc only if b_max is less than current size */
	if (sect->size < sect->max)
		return 0;
	/* double until size is reached */
	while ((sect->max <<= 1) < sect->size);
	/* current length + line length + extra characters + \0 */
	xrealloc(&sect->buf, sect->max, "rsz_buf()");
	return sect->size;
}

void pop_history(struct program *prog)
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

void build_body(struct program *prog)
{
	/* sanity check */
	if (!prog || !prog->cur_line) {
		WARNX("NULL pointer passed to build_body()");
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

void build_funcs(struct program *prog)
{
	/* sanity check */
	if (!prog || !prog->cur_line) {
		WARNX("NULL pointer passed to build_funcs()");
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

void build_final(struct program *prog, char **argv)
{
	/* sanity check */
	if (!prog || !argv) {
		WARNX("NULL pointer passed to build_final()");
		return;
	}
	/* finish building current iteration of source code */
	for (size_t i = 0; i < 2; i++) {
		strmv(0, prog->src[i].total.buf, prog->src[i].funcs.buf);
		strmv(CONCAT, prog->src[i].total.buf, prog->src[i].body.buf);
		strmv(CONCAT, prog->src[i].total.buf, prog_end);
	}
}
