/*
 * readline.c - readline functions
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#include "readline.h"
#include <stdlib.h>
#include <string.h>

/* default completion list */
char *comp_arg_list[] = {
	"auto", "bool", "break", "case", "char", "const", "continue", "default",
	"do", "double", "else", "enum", "extern", "float", "for", "goto",
	"if", "inline", "int", "long", "register", "restrict", "return",
	"short", "signed", "size_t", "sizeof", "static", "struct", "switch",
	"typedef", "union", "unsigned", "void", "volatile", "while",
	"true", "false", "intptr_t", "ptrdiff_t", "intmax_t", "uintmax_t",
	"uint8_t", "uint16_t", "uint32_t", "uint64_t", "char16_t", "char32_t",
	"wchar_t", "alignas", "alignof", "_Atomic", "_Complex", "_Imaginary",
	"_Generic", "_Noreturn", "_Static_assert", "_Thread_local",
	"#pragma", "#include", "#define", "#if", "#ifdef", "#else", "#endif",
	"__VA_ARGS__", "__asm__(", "__attribute__(", "malloc(", "calloc(",
	"free(", "memcpy(", "memset(", "memcmp(", "fread(", "fwrite(",
	"strcat(", "strtok(", "strcpy(", "strlen(", "puts(", "system(",
	"fopen(", "fclose(", "sprintf(", "printf(", "scanf(",
	";att", ";function", ";help", ";intel", ";macro", ";output",
	";parse", ";quit", ";reset", ";tracking", ";undo", ";warnings", NULL
};
/* global completion list struct */
STR_LIST comp_list;

char *generator(char const *text, int state)
{
	static size_t list_index, len;
	char *name, *buf;
	/* if no generated completions use the defaults */
	char **completions = DEFAULT(comp_list.list, comp_arg_list);
	if (!state) {
		list_index = 0;
		len = strlen(text);
	}
	while ((name = completions[list_index++])) {
		if (strncmp(name, text, len) == 0) {
			if (!(buf = calloc(1, strlen(name) + 1))) {
				WARN("%s", "error allocating generator string");
				return NULL;
			}
			memcpy(buf, name, strlen(name) + 1);
			return buf;
		}
	}
	return NULL;
}
