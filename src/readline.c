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
char *comps[] = {
	"auto", "break", "case", "char", "const", "continue", "default",
	"do", "double", "else", "enum", "extern", "float", "for", "goto",
	"if", "inline", "int", "long", "register", "restrict", "return",
	"short", "signed", "sizeof", "static", "struct", "switch",
	"typedef", "union", "unsigned", "void", "volatile", "while",
	"_Alignas", "_Alignof", "_Atomic", "_Bool", "_Complex", "_Generic",
	"_Imaginary", "_Noreturn", "_Static_assert", "_Thread_local",
	"#pragma", "#include", "#define", "#if", "#ifdef", "#else", "#endif",
	"bool", "true", "false", "free(", "malloc(", "realloc(", "calloc(",
	"printf(", "open(", "close(", "read(", "write(", "fopen(", "fclose(",
	"fread(", "fwrite(", "memcpy(", "memset(", "strcpy(", "strlen(",
	"strcat(", "strtok(", "puts(", "putc(", "getc", "putchar(", "getchar(",
	";reset", NULL
};
char **comp_list = NULL;


char *generator(const char *text, int state)
{
	static size_t list_index, len;
	char *name, *buf;
	char **completions = NULL;
	if (comp_list)
		completions = comp_list;
	else
		completions = comps;

	if (!state) {
		list_index = 0;
		len = strlen(text);
	}
	while ((name = completions[list_index++])) {
		if (memcmp(name, text, len) == 0) {
			if ((buf = malloc(strlen(name) + 1)) == NULL) {
				warn("%s", "error allocating generator string");
				return NULL;
			}
			memcpy(buf, name, strlen(name) + 1);
			return buf;
		}
	}
	return NULL;
}
