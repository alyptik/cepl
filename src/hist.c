/*
 * hist.c - history handling functions
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#include "hist.h"

/* externs */
extern struct str_list comp_list;

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
	"#  define _Float128 float_t\n"
	"#else\n"
	"#  include <complex.h>\n"
	"#endif\n\n"
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
	rl_cleanup_after_signal();
	/* free generated completions */
	free_str_list(&comp_list);
	/* append history to history file */
	if (prog->has_hist && write_history(prog->hist_file))
		WARN("%s", "write_history()");
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
	if (isatty(STDIN_FILENO) && !prog->eval_flag)
		printf("\n%s\n\n", "Terminating program.");
}

int write_asm(struct program *restrict prog, char *const *restrict cc_args)
{
	/* return early if no file open */
	if (!prog->asm_flag || !prog->asm_filename || !*prog->asm_filename || !prog->src[1].total)
		return -1;

	int pipe_cc[2], asm_fd, status;
	char src_buffer[strlen(prog->src[1].total) + 1];

	if (sizeof src_buffer < 2)
		ERRX("%s", "empty source passed to write_asm()");
	/* add trailing '\n' */
	memcpy(src_buffer, prog->src[1].total, sizeof src_buffer);
	src_buffer[sizeof src_buffer - 1] = '\n';
	/* create pipe */
	if (pipe(pipe_cc) == -1)
		ERR("%s", "error making pipe_cc pipe");
	/* set close-on-exec for pipe fds */
	set_cloexec(pipe_cc);

	if ((asm_fd = open(prog->asm_filename, O_WRONLY|O_CREAT|O_TRUNC,
					S_IWUSR|S_IRUSR|S_IRGRP|S_IROTH)) == -1) {
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
		dup2(pipe_cc[0], STDIN_FILENO);
		dup2(asm_fd, STDOUT_FILENO);
		execvp(cc_args[0], cc_args);
		/* execvp() should never return */
		ERR("%s", "error forking compiler");
		break;

	/* parent */
	default:
		close(pipe_cc[0]);
		if (write(pipe_cc[1], src_buffer, sizeof src_buffer) == -1)
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

void write_file(struct program *restrict prog)
{
	/* return early if no file open */
	if (!prog->out_flag || !prog->ofile || !prog->src[1].total)
		return;
	/* write out program to file */
	fwrite(prog->src[1].total, strlen(prog->src[1].total), 1, prog->ofile);
	fputc('\n', prog->ofile);
	fflush(NULL);
	fclose(prog->ofile);
	prog->ofile = NULL;
}

void free_buffers(struct program *restrict prog)
{
	/* write out history/asm before freeing buffers */
	write_file(prog);
	write_asm(prog, prog->cc_list.list);
	/* clean up user data */
	free_str_list(&prog->id_list);
	free(prog->cur_line);
	prog->cur_line = NULL;
	free(prog->type_list.list);
	prog->type_list.list = NULL;
	free_str_list(&prog->cc_list);
	if (prog->var_list.list) {
		for (size_t i = 0; i < prog->var_list.cnt; i++)
			free(prog->var_list.list[i].id);
		free(prog->var_list.list);
	}
	/* free program structs */
	for (size_t i = 0; i < 2; i++) {
		free(prog->src[i].funcs);
		free(prog->src[i].body);
		free(prog->src[i].total);
		free(prog->src[i].flags.list);
		free_str_list(&prog->src[i].hist);
		free_str_list(&prog->src[i].lines);
		prog->src[i].body_size = prog->src[i].funcs_size = prog->src[i].total_size = 0;
		prog->src[i].body_max = prog->src[i].funcs_max = prog->src[i].total_max = 1;
		/* `(void *)` casts needed to chain diff ptr types */
		prog->src[i].body = prog->src[i].funcs = prog->src[i].total = (void *)(prog->src[i].flags.list = NULL);
	}
}

void init_buffers(struct program *restrict prog)
{
	/* user is truncated source for display */
	xcalloc(char, &prog->src[0].funcs, 1, 1, "init");
	xcalloc(char, &prog->src[0].body, 1, strlen(prog_start_user) + 1, "init");
	xcalloc(char, &prog->src[0].total, 1,
			strlen(prelude)
			+ strlen(prog_start_user)
			+ strlen(prog_end) + 3, "init");
	prog->src[0].funcs_size = prog->src[0].funcs_max = 1;
	prog->src[0].body_size = prog->src[0].body_max = strlen(prog_start_user) + 1;
	prog->src[0].total_size = prog->src[0].total_max = strlen(prelude)
			+ strlen(prog_start_user)
			+ strlen(prog_end) + 3;
	/* actual is source passed to compiler */
	xcalloc(char, &prog->src[1].funcs, 1, strlen(prelude) + 1, "init");
	xcalloc(char, &prog->src[1].body, 1, strlen(prog_start) + 1, "init");
	xcalloc(char, &prog->src[1].total, 1, strlen(prelude)
			+ strlen(prog_start)
			+ strlen(prog_end) + 3, "init");
	prog->src[1].funcs_size = prog->src[1].funcs_max = strlen(prelude) + 1;
	prog->src[1].body_size = prog->src[1].body_max = strlen(prog_start) + 1;
	prog->src[1].total_size = prog->src[1].total_max = strlen(prelude)
			+ strlen(prog_start)
			+ strlen(prog_end) + 3;
	/* sanity check */
	for (size_t i = 0; i < 2; i++) {
		if (!prog->src[i].funcs || !prog->src[i].body || !prog->src[i].total) {
			free_buffers(prog);
			cleanup(prog);
			ERR("%s", "prgm[2] calloc()");
		}
	}
	/* no memcpy for prgm[0].funcs */
	strmv(0, prog->src[0].body, prog_start_user);
	strmv(0, prog->src[1].funcs, prelude);
	strmv(0, prog->src[1].body, prog_start);
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

size_t rsz_buf(struct program *restrict prog, char **buf_str, size_t *buf_sz, size_t *buf_max, size_t off)
{
	/* sanity check */
	if (!buf_str || !*buf_str || !prog->cur_line)
		return 0;
	size_t alloc_sz = strlen(*buf_str) + strlen(prog->cur_line) + off + 1;
	if (!buf_sz || !buf_max) {
		/* current length + line length + extra characters + \0 */
		xrealloc(char, buf_str, alloc_sz, "rsz_buf()");
		return alloc_sz;
	}
	*buf_sz += alloc_sz;
	/* realloc only if b_max is less than current size */
	if (*buf_sz < *buf_max)
		return 0;
	/* double until size is reached */
	while ((*buf_max *= 2) < *buf_sz);
	/* current length + line length + extra characters + \0 */
	xrealloc(char, buf_str, *buf_max, "rsz_buf()");
	return *buf_sz;
}

void pop_history(struct source *restrict src)
{
	switch(src->flags.list[--src->flags.cnt]) {
	case NOT_IN_MAIN:
		src->hist.cnt = src->lines.cnt = src->flags.cnt;
		strmv(0, src->funcs, src->hist.list[src->hist.cnt]);
		free(src->hist.list[src->hist.cnt]);
		free(src->lines.list[src->lines.cnt]);
		src->hist.list[src->hist.cnt] = src->lines.list[src->lines.cnt] = NULL;
		break;
	case IN_MAIN:
		src->hist.cnt = src->lines.cnt = src->flags.cnt;
		strmv(0, src->body, src->hist.list[src->hist.cnt]);
		free(src->hist.list[src->hist.cnt]);
		free(src->lines.list[src->lines.cnt]);
		src->hist.list[src->hist.cnt] = src->lines.list[src->lines.cnt] = NULL;
		break;
	case EMPTY: /* fallthrough */
	default:
		/* revert decrement */
		src->flags.cnt++;
	}
}

/* look for current line in readline history */
void dedup_history(char **restrict line)
{
	/* return early on empty input */
	if (!line || !*line)
		return;
	/* strip leading whitespace */
	char *strip = *line;
	strip += strspn(strip, " \t");
	/* current entry and forward/backward function pointers  */
	HIST_ENTRY *(*seek_hist[])() = {previous_history, next_history};
	/* save current position */
	int hpos = where_history();
	for (size_t i = 0; i < 2; i++) {
		while (history_search_prefix(strip, i - 1) != -1) {
			/* if this line is already in the history, remove the earlier entry */
			HIST_ENTRY *ent = current_history();
			/* HIST_ENTRY *ent = history_get(hpos[1]); */
			if (!ent || !ent->line || strcmp(*line, ent->line)) {
				/* break if at end of list */
				if (!seek_hist[i]())
					break;
				continue;
			}
			/* free application data */
			ent = remove_history(where_history());
			histdata_t data = free_history_entry(ent);
			if (data)
				free(data);
		}
		history_set_pos(hpos);
	}
	history_set_pos(hpos);
	add_history(strip);
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
		append_str(&prog->src[i].hist, prog->src[i].body, 0);
		append_flag(&prog->src[i].flags, IN_MAIN);
		strmv(CONCAT, prog->src[i].body, "\t");
		strmv(CONCAT, prog->src[i].body, prog->cur_line);
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
		append_str(&prog->src[i].hist, prog->src[i].funcs, 0);
		append_flag(&prog->src[i].flags, NOT_IN_MAIN);
		/* generate function buffers */
		strmv(CONCAT, prog->src[i].funcs, prog->cur_line);
	}
}

void build_final(struct program *restrict prog, char **restrict argv)
{
	/* sanity check */
	if (!prog || !argv) {
		WARNX("%s", "NULL pointer passed to build_final()");
		return;
	}
	/* finish building current iteration of source code */
	for (size_t i = 0; i < 2; i++) {
		strmv(0, prog->src[i].total, prog->src[i].funcs);
		strmv(CONCAT, prog->src[i].total, prog->src[i].body);
		/* print variable values */
		if (prog->track_flag && i == 1)
			print_vars(prog, prog->cc_list.list, argv);
		strmv(CONCAT, prog->src[i].total, prog_end);
	}
}
