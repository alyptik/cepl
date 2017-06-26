/*
 * readline.c - readline functions
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE file for copyright and license details.
 */

#include <err.h>
#include <stdlib.h>
#include <string.h>
#include "readline.h"

/* TODO: generate larger list of useful completions */
static char *const comps[] = {
	"auto", "break", "case", "char", "const", "continue", "default",
	"do", "double", "else", "enum", "extern", "float", "for", "goto",
	"if", "inline", "int", "long", "register", "restrict", "return",
	"short", "signed", "sizeof", "static", "struct", "switch",
	"typedef", "union", "unsigned", "void", "volatile", "while",
	"_Alignas", "_Alignof", "_Atomic", "_Bool", "_Complex", "_Generic",
	"_Imaginary", "_Noreturn", "_Static_assert", "_Thread_local",
	"#pragma", "#include", "#define", "#if", "#ifdef", "#else", "#endif",
	"bool", "true", "false", "free", "malloc(", "printf(", "puts(",
	";reset", NULL,
};

char *generator(const char *text, int state)
{
	static int list_index, len;
	char *name, *buf;
	if (!state) {
		list_index = 0;
		len = strlen(text);
	}
	while ((name = comps[list_index++])) {
		if (strncmp(name, text, len) == 0) {
			if ((buf = malloc(strlen(name) + 1)) == NULL)
				err(EXIT_FAILURE, "%s", "error allocating generator string");
			memcpy(buf, name, strlen(name) + 1);
			return buf;
		}
	}
	return NULL;
}
