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
void cleanup(void);
int write_asm(PROG_SRC (*restrict prgm)[], char *const cc_args[]);
void write_file(FILE **out_file, PROG_SRC (*restrict prgm)[]);
void free_buffers(VAR_LIST *restrict vlist, TYPE_LIST *restrict tlist, STR_LIST *restrict ilist, PROG_SRC (*restrict prgm)[], char **restrict ln);
void init_buffers(VAR_LIST *restrict vlist, TYPE_LIST *restrict tlist, STR_LIST *restrict ilist, PROG_SRC (*restrict prgm)[], char **restrict ln);
size_t rsz_buf(char **restrict buf_str, size_t *restrict buf_sz, size_t *restrict b_max, size_t off, char **restrict ln);
void dedup_history(char **restrict ln);
void pop_history(PROG_SRC *restrict prgm);
void build_body(PROG_SRC (*restrict prgm)[], char *restrict ln);
void build_funcs(PROG_SRC (*restrict prgm)[], char *restrict ln);
void build_final(PROG_SRC (*restrict prgm)[], VAR_LIST *restrict vlist, char *argv[]);

#endif
