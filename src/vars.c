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
	/* regex_t *reg; */
	if (regcomp(&reg, "[[:space::]]+[[:alpha:]_][[:alnum:]_]*[[:space::]]+=", REG_EXTENDED|REG_ICASE|REG_NEWLINE))
		err(EXIT_FAILURE, "%s", "failed to compile regex");

	/* regfree(reg); */
	return NULL;
}

int append_var(struct var_list *list, enum var_type type, char const *key)
{

	return 1;
}

int find_vars(struct var_list *list, char const *src, char *const cc_args[], char *const exec_args[])
{

	return 1;
}
