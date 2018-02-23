/*
 * hist.h - history handling functions
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#ifndef HIST_H
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
void write_file(struct program *restrict prog);
void free_buffers(struct program *restrict prog);
void init_buffers(struct program *restrict prog);
size_t rsz_buf(struct program *restrict prog, char **buf_str, size_t *buf_sz, size_t *buf_max, size_t off);
void dedup_history(char **restrict line);
void pop_history(struct source *restrict src);
void build_body(struct program *restrict prog);
void build_funcs(struct program *restrict prog);
void build_final(struct program *restrict prog, char **argv);

#endif
