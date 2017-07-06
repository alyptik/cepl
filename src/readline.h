/*
 * readline.h - readline functions
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE file for copyright and license details.
 */

#ifndef READLINE_H
#define READLINE_H 1

#include <stdio.h>
#include <readline/history.h>
#include <readline/readline.h>

#define UNUSED __attribute__ ((unused))

char *generator(char const *text, int state);

static inline char **completer(char const *text, int start UNUSED, int end UNUSED)
{
	/* always list completions */
	rl_bind_key('\t', &rl_complete);
	/* don't append space after completions */
	rl_completion_append_character = '\0';
	return rl_completion_matches(text, &generator);
}

#endif
