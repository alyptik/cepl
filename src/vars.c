/*
 * vars.c - variable tracking
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#include "compile.h"
#include "vars.h"

enum var_type extract_type(char *const line)
{

	return T_INT;
}

char *extract_id(char *const line)
{
	regex_t reg;
	regmatch_t match[2];
	char *capture;
	char regex[] = ".*[^[:alnum:]]+([[:alpha:]_][[:alnum:]_]*)([^[:alnum:]=!<>]*=|[^[:alnum:]=!<>]*[<>]{2}=*)[^=]*";

	if (regcomp(&reg, regex, REG_EXTENDED|REG_ICASE|REG_NEWLINE))
		err(EXIT_FAILURE, "%s", "failed to compile regex");
	/* non-zero means no match */
	if (regexec(&reg, line, 2, match, 0))
		return NULL;
	if ((capture = malloc(match[1].rm_eo - match[1].rm_so + 1)) == NULL)
		err(EXIT_FAILURE, "%s", "failed to allocate captured string");
	memset(capture, 0, match[1].rm_eo - match[1].rm_so + 1);
	memcpy(capture, line + match[1].rm_so, match[1].rm_eo - match[1].rm_so);
	return capture;
}

int append_var(struct var_list *list, enum var_type type, char const *key)
{

	return 1;
}

int find_vars(struct var_list *list, char const *src, char *const cc_args[], char *const exec_args[])
{

	return 1;
}
