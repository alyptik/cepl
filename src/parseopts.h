/*
 * parseopts.h - option parsing
 *
 * AUTHOR: Joey Pabalinas <joeypabalinas@gmail.com>
 * See LICENSE file for copyright and license details.
 */

#if !defined(PARSEOPTS_H)
#define PARSEOPTS_H 1

#include "defs.h"
#include "errs.h"
#include <fcntl.h>
#include <gelf.h>
#include <libelf.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/* prototypes */
void read_syms(struct str_list *tokens, char const *elf_file);
void parse_libs(struct str_list *symbols, char **libs);
char **parse_opts(struct program *prog, int argc, char **argv, char const *optstring);

#endif /* !defined(PARSEOPTS_H) */
