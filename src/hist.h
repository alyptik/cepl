/*
 * hist.h - history handling functions
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#ifndef HIST_H
#define HIST_H

#include "parseopts.h"
#include "readline.h"
#include "vars.h"

/* prototypes */
void cleanup(void);
void write_file(FILE *volatile *out_file, struct prog_src (*restrict prgm)[]);
void free_buffers(struct var_list *restrict vlist, struct type_list *restrict tlist, struct str_list *restrict ilist, struct prog_src (*restrict prgm)[], char **restrict ln);
void init_buffers(struct var_list *restrict vlist, struct type_list *restrict tlist, struct str_list *restrict ilist, struct prog_src (*restrict prgm)[], char **restrict ln);
size_t rsz_buf(char **restrict buf, size_t *restrict buf_sz, size_t *restrict b_max, size_t off, struct var_list *restrict vlist, struct type_list *restrict tlist, struct str_list *restrict ilist, struct prog_src (*restrict prgm)[], char **restrict ln);
void dedup_history(char **restrict ln);
void pop_history(struct prog_src *restrict prgm);
void build_body(struct prog_src (*restrict prgm)[], char *restrict ln);
void build_funcs(struct prog_src (*restrict prgm)[], char *restrict ln);
void build_final(struct prog_src (*restrict prgm)[], struct var_list *restrict vlist, char *argv[]);

#endif
