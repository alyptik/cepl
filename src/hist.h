/*
 * hist.h - history handling functions
 *
 * AUTHOR: Joey Pabalinas <joeypabalinas@gmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#if !defined(HIST_H)
#define HIST_H 1

#include "parseopts.h"
#include "readline.h"
#include "vars.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

/* prototypes */
void cleanup(struct program *restrict prog);
int write_asm(struct program *restrict prog, char *const *restrict cc_args);
void write_files(struct program *restrict prog);
void free_buffers(struct program *restrict prog);
void init_buffers(struct program *restrict prog);
size_t resize_sect(struct program *restrict prog, struct source_section *restrict sect, size_t off);
void pop_history(struct program *restrict prog);
void build_body(struct program *restrict prog);
void build_funcs(struct program *restrict prog);
void build_final(struct program *restrict prog, char **argv);

/* look for current line in readline history */
static inline void dedup_history_add(char *const *restrict line)
{
	/* return early on empty input */
	if (!line || !*line)
		return;
	/* strip leading whitespace */
	char *strip = *line;
	strip += strspn(strip, " \t");
	/* don't add empty or single character lines (invalid syntax) */
	if (strlen(strip) < 2)
		return;
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
			free(data);
		}
		history_set_pos(hpos);
	}
	history_set_pos(hpos);
	add_history(strip);
}

#endif /* !defined(HIST_H) */
