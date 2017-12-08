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

#if RL_VERSION_MAJOR < 7
	#define xrl_clear_visible_line() do {} while(0)
#else
	#define xrl_clear_visible_line() rl_clear_visible_line()
#endif

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
