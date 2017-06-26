/*
 * readline.h - readline functions
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE file for copyright and license details.
 */

#ifndef READLINE_H
#define READLINE_H

#include <stdio.h>
#include <readline/history.h>
#include <readline/readline.h>

#define UNUSED __attribute__ ((unused))

char *generator(const char *text, int state);

static inline char **completer(const char *text, int start UNUSED, int end UNUSED)
{
	char **matches;
	/* don't append space after completions */
	rl_completion_append_character = '\0';
	/* always list completions */
	rl_bind_key('\t', &rl_complete);
	matches = rl_completion_matches(text, &generator);
	return matches;
}

#endif
