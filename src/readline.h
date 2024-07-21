/*
 * readline.h - readline functions
 *
 * AUTHOR: Joey Pabalinas <joeypabalinas@gmail.com>
 * See LICENSE file for copyright and license details.
 */

#if !defined(READLINE_H)
#define READLINE_H 1

#include "defs.h"
#include "parseopts.h"
#include <readline/history.h>
#include <readline/readline.h>

/* stubs */
#if RL_VERSION_MAJOR < 7
# define rl_clear_visible_line() do { /*no-op*/ } while (0)
#endif
#if !defined(RL_STATE_ISEARCH)
# define RL_STATE_ISEARCH 0
#endif
#if !defined(RL_STATE_NSEARCH)
# define RL_STATE_BSEARCH 0
#endif
#if !defined(RL_STATE_VIMOTION)
# define RL_STATE_VIMOTION 0
#endif
#if !defined(RL_STATE_NUMERICARG)
# define RL_STATE_NUMERICARG 0
#endif
#if !defined(RL_STATE_MULTIKEY)
# define RL_STATE_MULTIKEY 0
#endif

/* prototypes */
char *generator(char const *text, int state);

static inline char **completer(char const *text, int start, int end)
{
	/* silence -Wunused-parameter warning */
	(void)start, (void)end;
	/* always list completions */
	rl_bind_key('\t', rl_complete);
	/* don't append space after completions */
	rl_completion_append_character = '\0';
	return rl_completion_matches(text, generator);
}

#endif /* !defined(READLINE_H) */
