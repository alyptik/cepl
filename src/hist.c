/*
 * hist.c - history handling functions
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#include "hist.h"

/* externs */
extern bool track_flag;
/* compiler argument list */
extern char **cc_argv;
/* completion list of generated symbols */
extern struct str_list comp_list;

/* globals */
char *hist_file;
/* `-o` flag output file */
FILE volatile *ofile;
/* program source strucs (prog[0] is truncated for interactive printing) */
struct prog_src prog[2];
/* global history file flag */
bool has_hist = false;
/* type, identifier, and var lists */
enum var_type *types;
struct str_list ids;
struct var_list vars;

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
	if (hist_file) {
		free(hist_file);
		hist_file = NULL;
	}
	if (isatty(STDIN_FILENO))
		printf("\n%s\n\n", "Terminating program.");
}

void write_file(FILE volatile **out_file, struct prog_src (*prgm)[])
{
	/* return early if no file open */
	if (!out_file || !*out_file || !(*prgm) || !(*prgm)[1].total)
		return;
	/* write out program to file */
	FILE *output = (FILE *)*out_file;
	fwrite((*prgm)[1].total, strlen((*prgm)[1].total), 1, output);
	fputc('\n', output);
	fflush(NULL);
	fclose(output);
	*out_file = NULL;
}

void free_buffers(struct var_list *vlist, enum var_type **tlist, struct str_list *ilist, struct prog_src (*prgm)[], char **ln)
{
	/* write out history before freeing buffers */
	write_file(&ofile, prgm);
	free_str_list(ilist);
	/* clean up user data */
	if (*ln) {
		free(*ln);
		*ln = NULL;
	}
	if (*tlist) {
		free(*tlist);
		*tlist = NULL;
	}
	/* free vectors */
	if (cc_argv)
		free_argv(&cc_argv);
	if (vlist->list) {
		for (size_t i = 0; i < vlist->cnt; i++) {
			if (vlist->list[i].key)
				free(vlist->list[i].key);
		}
		free(vlist->list);
		vlist->list = NULL;
	}
	/* free program structs */
	for (size_t i = 0; i < 2; i++) {
		if ((*prgm)[i].funcs)
			free((*prgm)[i].funcs);
		if ((*prgm)[i].body)
			free((*prgm)[i].body);
		if ((*prgm)[i].total)
			free((*prgm)[i].total);
		if ((*prgm)[i].flags.list)
			free((*prgm)[i].flags.list);
		free_str_list(&(*prgm)[i].hist);
		free_str_list(&(*prgm)[i].lines);
		(*prgm)[i].b_sz = (*prgm)[i].f_sz = (*prgm)[i].t_sz = 0;
		(*prgm)[i].b_max = (*prgm)[i].f_max = (*prgm)[i].t_max = 1;
		/* `(void *)` casts needed to chain diff ptr types */
		(*prgm)[i].body = (*prgm)[i].funcs = (*prgm)[i].total = (void *)((*prgm)[i].flags.list = NULL);
	}
}

void init_buffers(struct var_list *vlist, enum var_type **tlist, struct str_list *ilist, struct prog_src (*prgm)[], char **ln)
{
	/* user is truncated source for display */
	(*prgm)[0].funcs = calloc(1, 1);
	(*prgm)[0].body = calloc(1, strlen(prog_start_user) + 1);
	(*prgm)[0].total = calloc(1, strlen(prelude) + strlen(prog_start_user) + strlen(prog_end) + 3);
	(*prgm)[0].f_sz = (*prgm)[0].f_max = 1;
	(*prgm)[0].b_sz = (*prgm)[0].b_max = strlen(prog_start_user) + 1;
	(*prgm)[0].t_sz = (*prgm)[0].t_max = strlen(prelude) + strlen(prog_start_user) + strlen(prog_end) + 3;
	/* actual is source passed to compiler */
	(*prgm)[1].funcs = calloc(1, strlen(prelude) + 1);
	(*prgm)[1].body = calloc(1, strlen(prog_start) + 1);
	(*prgm)[1].total = calloc(1, strlen(prelude) + strlen(prog_start) + strlen(prog_end) + 3);
	(*prgm)[1].f_sz = (*prgm)[1].f_max = strlen(prelude) + 1;
	(*prgm)[1].b_sz = (*prgm)[1].b_max = strlen(prog_start) + 1;
	(*prgm)[1].t_sz = (*prgm)[1].t_max = strlen(prelude) + strlen(prog_start) + strlen(prog_end) + 3;
	/* sanity check */
	for (size_t i = 0; i < 2; i++) {
		if (!(*prgm)[i].funcs || !(*prgm)[i].body || !(*prgm)[i].total) {
			free_buffers(vlist, tlist, ilist, prgm, ln);
			cleanup();
			ERR("prgm[2] calloc()");
		}
	}
	/* no memcpy for prgm[0].funcs */
	strmv(0, (*prgm)[0].body, prog_start_user);
	strmv(0, (*prgm)[1].funcs, prelude);
	strmv(0, (*prgm)[1].body, prog_start);
	/* init source history and flag lists */
	for (size_t i = 0; i < 2; i++) {
		init_list(&(*prgm)[i].lines, "FOOBARTHISVALUEDOESNTMATTERTROLLOLOLOL");
		init_list(&(*prgm)[i].hist, "FOOBARTHISVALUEDOESNTMATTERTROLLOLOLOL");
		init_flag_list(&(*prgm)[i].flags);
	}
	init_vlist(vlist);
}

size_t resize_buffer(char **buf, size_t *buf_sz, size_t *b_max, size_t off, struct var_list *vlist, enum var_type **tlist, struct str_list *ilist, struct prog_src (*prgm)[], char **ln)
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
			ERR("resize_buffer()");
		}
		*buf = tmp;
		return alloc_sz;
	}
	*buf_sz += alloc_sz;
	/* realloc only if b_max is less than current size */
	if (*buf_sz < *b_max)
		return 0;
	/* check if size too large */
	if (*buf_sz > MAX)
		ERRX("*buf_sz > (SIZE_MAX / 2 - 1)");
	/* double until size is reached */
	while ((*b_max *= 2) < *buf_sz);
	/* current length + line length + extra characters + \0 */
	if (!(tmp = realloc(*buf, *b_max))) {
		free_buffers(vlist, tlist, ilist, prgm, ln);
		cleanup();
		ERR("resize_buffer()");
	}
	*buf = tmp;
	return *buf_sz;
}

void pop_history(struct prog_src *prgm)
{
	switch(prgm->flags.list[--prgm->flags.cnt]) {
	case NOT_IN_MAIN:
		prgm->hist.cnt = prgm->lines.cnt = prgm->flags.cnt;
		strmv(0, prgm->funcs, prgm->hist.list[prgm->hist.cnt]);
		free(prgm->hist.list[prgm->hist.cnt]);
		free(prgm->lines.list[prgm->lines.cnt]);
		prgm->hist.list[prgm->hist.cnt] = prgm->lines.list[prgm->lines.cnt] = NULL;
		break;
	case IN_MAIN:
		prgm->hist.cnt = prgm->lines.cnt = prgm->flags.cnt;
		strmv(0, prgm->body, prgm->hist.list[prgm->hist.cnt]);
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
void dedup_history(char **ln)
{
	/* return early on empty input */
	if (!ln || !*ln)
		return;
	/* strip leading whitespace */
	char *strip = *ln;
	strip += strspn(strip, " \t");
	/* search forward and backward in history */
	int cur_hist = where_history();
	for (int i = -1; i < 2; i += 2) {
		/* seek backwords or forwards */
		HIST_ENTRY *(*seek_hist)(void) = (i < 0) ? &previous_history : &next_history;
		while (history_search_prefix(strip, i) != -1) {
			/* if this ln is already in the history, remove the earlier entry */
			HIST_ENTRY *ent = current_history();
			/* skip if NULL or not a complete match */
			if (!ent || !ent->line || strcmp(*ln, ent->line)) {
				/* break if at end of list */
				if (!seek_hist())
					break;
				continue;
			}
			/* remove and free data */
			remove_history(where_history());
			/* free application data */
			histdata_t data = free_history_entry(ent);
			if (data)
				free(data);
		}
		history_set_pos(cur_hist);
	}
	/* reset history position and add the ln */
	history_set_pos(cur_hist);
	add_history(strip);
}

void build_body(struct prog_src (*prgm)[], char *ln)
{
	/* sanity check */
	if (!prgm || !ln) {
		WARNX("NULL pointer passed to build_body()");
		return;
	}
	for (size_t i = 0; i < 2; i++) {
		append_str(&(*prgm)[i].lines, ln, 0);
		append_str(&(*prgm)[i].hist, (*prgm)[i].body, 0);
		append_flag(&(*prgm)[i].flags, IN_MAIN);
		strmv(CONCAT, (*prgm)[i].body, "\t");
		strmv(CONCAT, (*prgm)[i].body, ln);
	}
}

void build_funcs(struct prog_src (*prgm)[], char *ln)
{
	/* sanity check */
	if (!prgm || !ln) {
		WARNX("NULL pointer passed to build_funcs()");
		return;
	}
	for (size_t i = 0; i < 2; i++) {
		append_str(&(*prgm)[i].lines, ln, 0);
		append_str(&(*prgm)[i].hist, (*prgm)[i].funcs, 0);
		append_flag(&(*prgm)[i].flags, NOT_IN_MAIN);
		/* generate function buffers */
		strmv(CONCAT, (*prgm)[i].funcs, ln);
	}
}

void build_final(struct prog_src (*prgm)[], struct var_list *vlist, char *argv[])
{
	/* sanity check */
	if (!prgm || !argv) {
		WARNX("NULL pointer passed to build_final()");
		return;
	}
	/* finish building current iteration of source code */
	for (size_t i = 0; i < 2; i++) {
		strmv(0, (*prgm)[i].total, (*prgm)[i].funcs);
		strmv(CONCAT, (*prgm)[i].total, (*prgm)[i].body);
		/* print variable values */
		if (track_flag && i == 1)
			print_vars(vlist, (*prgm)[i].total, cc_argv, argv);
		strmv(CONCAT, (*prgm)[i].total, prog_end);
	}
}
