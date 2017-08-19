/*
 * errors.h - error-handling wrappers
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#ifndef ERRORS_H
#define ERRORS_H

/* use specific message */
#define ERR(X) err(EXIT_FAILURE, "%s %s %d", (X), "at line", __LINE__)
#define ERRX(X) errx(EXIT_FAILURE, "%s %s %d", (X), "at line", __LINE__)
#define WARN(X) warn("%s %s %d", (X), "at line", __LINE__)
#define WARNX(X) warnx("%s %s %d", (X), "at line", __LINE__)

/* general case */
#define ERRGEN(X) err(EXIT_FAILURE, "%s %s %s %d", "error during", (X), "at line", __LINE__)
#define ERRXGEN(X) errx(EXIT_FAILURE, "%s %s %s %d", "error during", (X), "at line", __LINE__)
#define WARNGEN(X) warn("%s %s %s %d", "error during", (X), "at line", __LINE__)
#define WARNXGEN(X) warnx("%s %s %s %d", "error during", (X), "at line", __LINE__)

/* array case */
#define ERRARR(X, Y) err(EXIT_FAILURE, "%s %s[%d] %s %d", "error allocating", (X), (Y), "at line", __LINE__)
#define ERRXARR(X, Y) err(EXIT_FAILURE, "%s %s[%d] %s %d", "error allocating", (X), (Y), "at line", __LINE__)
#define WARNARR(X, Y) err("%s %s[%d] %s %d", "error allocating", (X), (Y), "at line", __LINE__)
#define WARNXARR(X, Y) err("%s %s[%d] %s %d", "error allocating", (X), (Y), "at line", __LINE__)

#endif
