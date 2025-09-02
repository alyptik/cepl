/*
 * compile.h - compile the generated source code
 *
 * AUTHOR: Joey Pabalinas <joeypabalinas@gmail.com>
 * See LICENSE file for copyright and license details.
 */

#if !defined(COMPILE_H)
#define COMPILE_H 1

#include "defs.h"
#include "errs.h"

/* prototypes */
int compile(char const *restrict src, char *const cc_args[], bool show_errors);

#endif /* !defined(COMPILE_H) */
