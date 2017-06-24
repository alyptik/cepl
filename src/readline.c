/*
 * readline.c - readline functions
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE file for copyright and license details.
 */

/* TODO: implement completion */
/* void rl_completor(void) {} */

#include <err.h>
#include <stdlib.h>
#include <string.h>
#include "readline.h"

/* TODO: generate useful completion list */
static char *cmd[] ={ "foo", "bar", "baz", ";reset", " " };

char *generator(const char *text, int state)
{
	static int list_index, len;
	char *name;
	char *buf;

	if (!state) {
		list_index = 0;
		len = strlen(text);
	}

	while ((name = cmd[list_index])) {
		list_index++;
		if (strncmp(name, text, len) == 0) {
			if ((buf = malloc(strlen(name) + 1)) == NULL)
				err(EXIT_FAILURE, "%s", "error allocating generator string");
			memcpy(buf, name, strlen(name) + 1);
			return buf;
		}
	}

	return NULL;
}
