/*
 * error.h - error-checking wrappers
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#ifndef ERROR_H
#define ERROR_H 1

#include <err.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* general */
#define ERR(X) err(EXIT_FAILURE, "%s %s %s %d", "error during", X, "at line", __LINE__)
#define ERRX(X) errx(EXIT_FAILURE, "%s %s %s %d", "error during", X, "at line", __LINE__)
#define WARN(X) warn("%s %s %s %d", "error during", X, "at line", __LINE__)
#define WARNX(X) warnx("%s %s %s %d", "error during", X, "at line", __LINE__)

/* arrays */
#define ERRARR(X, Y) err(EXIT_FAILURE, "%s %s[%d] %s %d", "error allocating", X, Y, "at line", __LINE__)
#define ERRXARR(X, Y) err(EXIT_FAILURE, "%s %s[%d] %s %d", "error allocating", X, Y, "at line", __LINE__)
#define WARNARR(X, Y) err("%s %s[%d] %s %d", "error allocating", X, Y, "at line", __LINE__)
#define WARNXARR(X, Y) err("%s %s[%d] %s %d", "error allocating", X, Y, "at line", __LINE__)

#endif
