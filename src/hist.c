/*
 * hist.c - history handling functions
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#include "hist.h"

/* globals */
bool has_hist = false;
/* output filenames */
char *out_filename, *asm_filename;
/* name of REPL history file */
char *hist_file;
/* `-o` flag output file */
FILE *ofile;
/* program source strucs (prg[0] is truncated for interactive printing) */
PROG_SRC prg[2];
/* type, identifier, and var lists */
TYPE_LIST tl;
STR_LIST il;
VAR_LIST vl;

/* externs */
extern bool asm_flag, eval_flag, track_flag;
/* compiler argument list */
extern char **cc_argv;
/* completion list of generated symbols */
extern STR_LIST comp_list;
/* line buffer */
extern char *lptr;

void cleanup(void)
{
	/* free generated completions */
	free_str_list(&comp_list);
	/* append history to history file */
	if (has_hist) {
		if (write_history(hist_file)) {
			WARN("write_history()");
		}
	}
	free(hist_file);
	hist_file = NULL;
	free(out_filename);
	out_filename = NULL;
	free(asm_filename);
	asm_filename = NULL;
	if (isatty(STDIN_FILENO) && !eval_flag)
		printf("\n%s\n\n", "Terminating program.");
}

int write_asm(PROG_SRC (*restrict prgm)[], char *const cc_args[])
{
	/* return early if no file open */
	if (!asm_filename || !*asm_filename || !(*prgm) || !(*prgm)[1].total)
		return -1;

	int pipe_cc[2], asm_fd, status;
	char src_buffer[strlen((*prgm)[1].total) + 1];

	if (sizeof src_buffer < 2)
		ERRX("empty source passed to write_asm()");
	/* add trailing '\n' */
	memcpy(src_buffer, (*prgm)[1].total, sizeof src_buffer);
	src_buffer[sizeof src_buffer - 1] = '\n';
	/* create pipe */
	if (pipe(pipe_cc) == -1)
		ERR("error making pipe_cc pipe");
	/* set close-on-exec for pipe fds */
	set_cloexec(pipe_cc);

	if ((asm_fd = open(asm_filename, O_WRONLY|O_CREAT|O_TRUNC, S_IWUSR|S_IRUSR|S_IRGRP|S_IROTH)) == -1) {
		close(pipe_cc[0]);
		close(pipe_cc[1]);
		WARN("error opening asm output file");
		return -1;
	}

	/* fork compiler */
	switch (fork()) {
	/* error */
	case -1:
		close(pipe_cc[0]);
		close(pipe_cc[1]);
		close(asm_fd);
		ERR("error forking compiler");
		break;

	/* child */
	case 0:
		dup2(pipe_cc[0], STDIN_FILENO);
		dup2(asm_fd, STDOUT_FILENO);
		execvp(cc_args[0], cc_args);
		/* execvp() should never return */
		ERR("error forking compiler");
		break;

	/* parent */
	default:
		close(pipe_cc[0]);
		if (write(pipe_cc[1], src_buffer, sizeof src_buffer) == -1)
			ERR("error writing to pipe_cc[1]");
		close(pipe_cc[1]);
		wait(&status);
		/* convert 255 to -1 since WEXITSTATUS() only returns the low-order 8 bits */
		if (WIFEXITED(status) && WEXITSTATUS(status)) {
			WARNX("compiler returned non-zero exit code");
			return (WEXITSTATUS(status) != 0xff) ? WEXITSTATUS(status) : -1;
		}
	}

	fsync(asm_fd);
	close(asm_fd);
	/* program returned success */
	return 0;
}

void write_file(FILE **restrict out_file, PROG_SRC (*restrict prgm)[])
{
	/* return early if no file open */
	if (!out_file || !*out_file || !(*prgm) || !(*prgm)[1].total)
		return;
	/* write out program to file */
	fwrite((*prgm)[1].total, strlen((*prgm)[1].total), 1, *out_file);
	fputc('\n', *out_file);
	fflush(NULL);
	fclose(*out_file);
	*out_file = NULL;
}

void free_buffers(VAR_LIST *restrict vlist, TYPE_LIST *restrict tlist, STR_LIST *restrict ilist, PROG_SRC (*restrict prgm)[], char **restrict ln)
{
	/* write out history/asm before freeing buffers */
	write_file(&ofile, prgm);
	write_asm(prgm, cc_argv);
	/* clean up user data */
	free_str_list(ilist);
	free(*ln);
	*ln = NULL;
	lptr = NULL;
	free(tlist->list);
	tlist->list = NULL;
	free_argv(&cc_argv);
	if (vlist->list) {
		for (size_t i = 0; i < vlist->cnt; i++)
			free(vlist->list[i].key);
		free(vlist->list);
	}
	/* free program structs */
	for (size_t i = 0; i < 2; i++) {
		free((*prgm)[i].f);
		free((*prgm)[i].b);
		free((*prgm)[i].total);
		free((*prgm)[i].flags.list);
		free_str_list(&(*prgm)[i].hist);
		free_str_list(&(*prgm)[i].lines);
		(*prgm)[i].b_sz = (*prgm)[i].f_sz = (*prgm)[i].t_sz = 0;
		(*prgm)[i].b_max = (*prgm)[i].f_max = (*prgm)[i].t_max = 1;
		/* `(void *)` casts needed to chain diff ptr types */
		(*prgm)[i].b = (*prgm)[i].f = (*prgm)[i].total = (void *)((*prgm)[i].flags.list = NULL);
	}
}

void init_buffers(VAR_LIST *restrict vlist, TYPE_LIST *restrict tlist, STR_LIST *restrict ilist, PROG_SRC (*restrict prgm)[], char **restrict ln)
{
	/* user is truncated source for display */
	(*prgm)[0].f = calloc(1, 1);
	(*prgm)[0].b = calloc(1, strlen(prog_start_user) + 1);
	(*prgm)[0].total = calloc(1, strlen(prelude) + strlen(prog_start_user) + strlen(prog_end) + 3);
	(*prgm)[0].f_sz = (*prgm)[0].f_max = 1;
	(*prgm)[0].b_sz = (*prgm)[0].b_max = strlen(prog_start_user) + 1;
	(*prgm)[0].t_sz = (*prgm)[0].t_max = strlen(prelude) + strlen(prog_start_user) + strlen(prog_end) + 3;
	/* actual is source passed to compiler */
	(*prgm)[1].f = calloc(1, strlen(prelude) + 1);
	(*prgm)[1].b = calloc(1, strlen(prog_start) + 1);
	(*prgm)[1].total = calloc(1, strlen(prelude) + strlen(prog_start) + strlen(prog_end) + 3);
	(*prgm)[1].f_sz = (*prgm)[1].f_max = strlen(prelude) + 1;
	(*prgm)[1].b_sz = (*prgm)[1].b_max = strlen(prog_start) + 1;
	(*prgm)[1].t_sz = (*prgm)[1].t_max = strlen(prelude) + strlen(prog_start) + strlen(prog_end) + 3;
	/* sanity check */
	for (size_t i = 0; i < 2; i++) {
		if (!(*prgm)[i].f || !(*prgm)[i].b || !(*prgm)[i].total) {
			free_buffers(vlist, tlist, ilist, prgm, ln);
			cleanup();
			ERR("prgm[2] calloc()");
		}
	}
	/* no memcpy for prgm[0].f */
	strmv(0, (*prgm)[0].b, prog_start_user);
	strmv(0, (*prgm)[1].f, prelude);
	strmv(0, (*prgm)[1].b, prog_start);
	/* init source history and flag lists */
	for (size_t i = 0; i < 2; i++) {
		init_list(&(*prgm)[i].lines, "FOOBARTHISVALUEDOESNTMATTERTROLLOLOLOL");
		init_list(&(*prgm)[i].hist, "FOOBARTHISVALUEDOESNTMATTERTROLLOLOLOL");
		init_flag_list(&(*prgm)[i].flags);
	}
	init_vlist(vlist);
	init_tlist(tlist);
	init_list(ilist, "FOOBARTHISVALUEDOESNTMATTERTROLLOLOLOL");
}

size_t rsz_buf(char **restrict buf, size_t *restrict buf_sz, size_t *restrict b_max, size_t off, VAR_LIST *restrict vlist, TYPE_LIST *restrict tlist, STR_LIST *restrict ilist, PROG_SRC (*restrict prgm)[], char **restrict ln)
{
	/* sanity check */
	if (!buf || !*buf || !ln)
		return 0;
	char *tmp;
	size_t alloc_sz = strlen(*buf) + strlen(*ln) + off + 1;
	if (!buf_sz || !b_max) {
		/* current length + line length + extra characters + \0 */
		if (!(tmp = realloc(*buf, alloc_sz))) {
			free_buffers(vlist, tlist, ilist, prgm, ln);
			cleanup();
			ERR("rsz_buf()");
		}
		*buf = tmp;
		return alloc_sz;
	}
	*buf_sz += alloc_sz;
	/* realloc only if b_max is less than current size */
	if (*buf_sz < *b_max)
		return 0;
	/* check if size too large */
	if (*buf_sz > ARRAY_MAX)
		ERRX("*buf_sz > (SIZE_MAX / 2 - 1)");
	/* double until size is reached */
	while ((*b_max *= 2) < *buf_sz);
	/* current length + line length + extra characters + \0 */
	if (!(tmp = realloc(*buf, *b_max))) {
		free_buffers(vlist, tlist, ilist, prgm, ln);
		cleanup();
		ERR("rsz_buf()");
	}
	*buf = tmp;
	return *buf_sz;
}

void pop_history(PROG_SRC *restrict prgm)
{
	switch(prgm->flags.list[--prgm->flags.cnt]) {
	case NOT_IN_MAIN:
		prgm->hist.cnt = prgm->lines.cnt = prgm->flags.cnt;
		strmv(0, prgm->f, prgm->hist.list[prgm->hist.cnt]);
		free(prgm->hist.list[prgm->hist.cnt]);
		free(prgm->lines.list[prgm->lines.cnt]);
		prgm->hist.list[prgm->hist.cnt] = prgm->lines.list[prgm->lines.cnt] = NULL;
		break;
	case IN_MAIN:
		prgm->hist.cnt = prgm->lines.cnt = prgm->flags.cnt;
		strmv(0, prgm->b, prgm->hist.list[prgm->hist.cnt]);
		free(prgm->hist.list[prgm->hist.cnt]);
		free(prgm->lines.list[prgm->lines.cnt]);
		prgm->hist.list[prgm->hist.cnt] = prgm->lines.list[prgm->lines.cnt] = NULL;
		break;
	case EMPTY: /* fallthrough */
	default:
		/* revert decrement */
		prgm->flags.cnt++;
	}
}

/* look for current ln in readln history */
void dedup_history(char **restrict ln)
{
	/* return early on empty input */
	if (!ln || !*ln)
		return;
	/* strip leading whitespace */
	char *strip = *ln;
	strip += strspn(strip, " \t");
	/* current entry and forward/backward function pointers  */
	HIST_ENTRY *(*seek_hist[])() = {&previous_history, &next_history};
	/* save current position */
	int hpos = where_history();
	for (size_t i = 0; i < 2; i++) {
		while (history_search_prefix(strip, i - 1) != -1) {
			/* if this line is already in the history, remove the earlier entry */
			HIST_ENTRY *ent = current_history();
			/* HIST_ENTRY *ent = history_get(hpos[1]); */
			if (!ent || !ent->line || strcmp(*ln, ent->line)) {
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

void build_body(PROG_SRC (*restrict prgm)[], char *restrict ln)
{
	/* sanity check */
	if (!prgm || !ln) {
		WARNX("NULL pointer passed to build_body()");
		return;
	}
	for (size_t i = 0; i < 2; i++) {
		append_str(&(*prgm)[i].lines, ln, 0);
		append_str(&(*prgm)[i].hist, (*prgm)[i].b, 0);
		append_flag(&(*prgm)[i].flags, IN_MAIN);
		strmv(CONCAT, (*prgm)[i].b, "\t");
		strmv(CONCAT, (*prgm)[i].b, ln);
	}
}

void build_funcs(PROG_SRC (*restrict prgm)[], char *restrict ln)
{
	/* sanity check */
	if (!prgm || !ln) {
		WARNX("NULL pointer passed to build_funcs()");
		return;
	}
	for (size_t i = 0; i < 2; i++) {
		append_str(&(*prgm)[i].lines, ln, 0);
		append_str(&(*prgm)[i].hist, (*prgm)[i].f, 0);
		append_flag(&(*prgm)[i].flags, NOT_IN_MAIN);
		/* generate function buffers */
		strmv(CONCAT, (*prgm)[i].f, ln);
	}
}

void build_final(PROG_SRC (*restrict prgm)[], VAR_LIST *restrict vlist, char *argv[])
{
	/* sanity check */
	if (!prgm || !argv) {
		WARNX("NULL pointer passed to build_final()");
		return;
	}
	/* finish building current iteration of source code */
	for (size_t i = 0; i < 2; i++) {
		strmv(0, (*prgm)[i].total, (*prgm)[i].f);
		strmv(CONCAT, (*prgm)[i].total, (*prgm)[i].b);
		/* print variable values */
		if (track_flag && i == 1)
			print_vars(vlist, (*prgm)[i].total, cc_argv, argv);
		strmv(CONCAT, (*prgm)[i].total, prog_end);
	}
}
