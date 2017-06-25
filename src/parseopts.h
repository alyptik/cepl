/*
 * parseopts.h - option parsing
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE file for copyright and license details.
 */

#ifndef PARSEOPTS_H
#define PARSEOPTS_H

#define CEPL_VERSION "CEPL v0.1.4"

char *const *parse_opts(int argc, char *argv[], char *optstring);

#endif
