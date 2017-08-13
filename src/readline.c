/*
 * readline.c - readline functions
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#include <err.h>
#include <stdlib.h>
#include <string.h>
#include "readline.h"
#include "parseopts.h"

/* default completion list */
char *comp_arg_list[] = {
	"asm", "auto", "break", "case", "char", "const", "continue", "default",
	"do", "double", "else", "enum", "extern", "float", "for", "goto",
	"if", "inline", "int", "long", "register", "restrict", "return",
	"short", "signed", "sizeof", "static", "struct", "switch",
	"typedef", "union", "unsigned", "void", "volatile", "while",
	"_Alignas", "_Alignof", "_Atomic", "_Bool", "_Complex", "_Generic",
	"_Imaginary", "_Noreturn", "_Static_assert", "_Thread_local",
	"#pragma", "#include", "#define", "#if", "#ifdef", "#else", "#endif",
	"bool", "true", "false", "free(", "malloc(", "realloc(", "calloc(",
	"system(", "fork(", "pipe(", "execl(", "execv(", "kill(", "signal(",
	"printf(", "fprintf(", "dprintf(", "sprintf(", "snprintf(",
	"open(", "close(", "read(", "write(", "fopen(", "fclose(",
	"scanf(", "fscanf(", "mmap(", "munmap(", "syscall(",
	"fread(", "fwrite(", "memcpy(", "memset(", "memcmp(", "getline(",
	"puts(", "strspn(", "strlen(", "strcat(", "strtok(", "stpcpy(",
	";function", ";parse", ";quit", ";reset", ";warnings", NULL
};
/* global completion list struct */
struct str_list comp_list = { .cnt = 0, .list = NULL };

char *generator(char const *text, int state)
{
	static size_t list_index, len;
	char *name, *buf;
	char **completions = comp_list.list ? comp_list.list : comp_arg_list;
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
