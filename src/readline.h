/*
 * readline.h - readline functions
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#ifndef READLINE_H
#define READLINE_H 1

#include "parseopts.h"
#include <readline/history.h>
#include <readline/readline.h>

/* prototypes */
char *generator(char const *text, int state);

static inline char **completer(char const *text, int start, int end)
{
	/* silence -Wunused-parameter warning */
	(void)start, (void)end;
	/* always list completions */
	rl_bind_key('\t', &rl_complete);
	/* don't append space after completions */
	rl_completion_append_character = '\0';
	return rl_completion_matches(text, &generator);
}

#endif
