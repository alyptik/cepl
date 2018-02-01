/*
 * parseopts.h - option parsing
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#ifndef PARSEOPTS_H
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
void read_syms(struct str_list *restrict tokens, char const *restrict elf_file);
void parse_libs(struct str_list *restrict symbols, char **restrict libs);
char **parse_opts(struct program *restrict prog, int argc, char **argv, char const *optstring);

#endif
