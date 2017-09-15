/*
 * readline.c - readline functions
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#include <err.h>
#include <stdlib.h>
#include <string.h>
#include "errors.h"
#include "parseopts.h"
#include "readline.h"

/* default completion list */
char *comp_arg_list[] = {
	"auto", "break", "case", "char", "const", "continue", "default",
	"do", "double", "else", "enum", "extern", "float", "for", "goto",
	"if", "inline", "int", "long", "register", "restrict", "return",
	"short", "signed", "size_t", "sizeof", "static", "struct", "switch",
	"ptrdiff_t", "typedef", "union", "unsigned", "void", "volatile", "while",
	"_Alignas", "_Alignof", "_Atomic", "_Bool", "_Complex", "_Generic",
	"_Imaginary", "_Noreturn", "_Static_assert", "_Thread_local", "__asm__",
	"#pragma", "#include", "#define", "#if", "#ifdef", "#else", "#endif",
	"bool", "true", "false", "free(", "malloc(", "realloc(", "calloc(",
	"system(", "fork(", "pipe(", "execl(", "execv(", "kill(",
	"signal(", "printf(", "fprintf(", "dprintf(", "sprintf(",
	"open(", "close(", "read(", "write(", "fopen(", "fclose(",
	"scanf(", "fscanf(", "mmap(", "munmap(", "syscall(",
	"fread(", "fwrite(", "memcpy(", "memset(", "memcmp(",
	"puts(", "strspn(", "strlen(", "strcat(", "strtok(",
	";function", ";parse", ";quit", ";reset", ";tracking", ";warnings", NULL
};
/* global completion list struct */
struct str_list comp_list;

char *generator(char const *text, int state)
{
	static size_t list_index, len;
	char *name, *buf;
	/* if no generated completions use the defaults */
	char **completions = comp_list.list ? comp_list.list : comp_arg_list;
	if (!state) {
		list_index = 0;
		len = strlen(text);
	}
	while ((name = completions[list_index++])) {
		if (strncmp(name, text, len) == 0) {
			if (!(buf = calloc(1, strlen(name) + 1))) {
				WARN("error allocating generator string");
				return NULL;
			}
			memcpy(buf, name, strlen(name) + 1);
			return buf;
		}
	}
	return NULL;
}
