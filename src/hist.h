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
void write_file(FILE volatile **out_file, struct prog_src (*prgm)[]);
void free_buffers(struct var_list *vlist, struct type_list *tlist, struct str_list *ilist, struct prog_src (*prgm)[], char **ln);
void init_buffers(struct var_list *vlist, struct type_list *tlist, struct str_list *ilist, struct prog_src (*prgm)[], char **ln);
size_t rsz_buf(char **buf, size_t *buf_sz, size_t *b_max, size_t off, struct var_list *vlist, struct type_list *tlist, struct str_list *ilist, struct prog_src (*prgm)[], char **ln);
void dedup_history(char **ln);
void pop_history(struct prog_src *prgm);
void build_body(struct prog_src (*prgm)[], char *ln);
void build_funcs(struct prog_src (*prgm)[], char *ln);
void build_final(struct prog_src (*prgm)[], struct var_list *vlist, char *argv[]);

#endif
