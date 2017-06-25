/*
 * parseopts.h - option parsing
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE file for copyright and license details.
 */

#ifndef PARSEOPTS_H
#define PARSEOPTS_H

#include <stdio.h>
#define CEPL_VERSION "CEPL v0.1.5"

char *const *parse_opts(int argc, char *argv[], char *optstring, FILE **ofile);
int free_cc_argv(char **cc_argv);

#endif
