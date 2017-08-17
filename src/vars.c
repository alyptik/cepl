/*
 * vars.c - variable tracking
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#include "compile.h"
#include "vars.h"

enum var_type extract_type(char const *line, char const *id)
{

	return T_INT;
}

size_t extract_id(char const *line, char **id, size_t *offset)
{
	regex_t reg;
	regmatch_t match[2];
	/* second capture is ignored */
	char regex[] = ".*[^[:alnum:]]+([[:alpha:]_][[:alnum:]_]*)([^[:alnum:]=!<>]*=|[^[:alnum:]=!<>]*[<>]{2}=*)[^=]*";

	/* return early if passed NULL pointers */
	if (!id || !offset)
		return 0;
	if (regcomp(&reg, regex, REG_EXTENDED|REG_ICASE|REG_NEWLINE))
		err(EXIT_FAILURE, "%s", "failed to compile regex");
	/* non-zero return or -1 value in rm_so means no captures */
	if (regexec(&reg, line, 2, match, 0) || match[1].rm_so == -1)
		return 0;
	if ((*id= malloc(match[1].rm_eo - match[1].rm_so + 1)) == NULL)
		err(EXIT_FAILURE, "%s", "failed to allocate space for captured string");

	/* set the output parameter and return the offset */
	memset(*id, 0, match[1].rm_eo - match[1].rm_so + 1);
	memcpy(*id, line + match[1].rm_so, match[1].rm_eo - match[1].rm_so);
	*offset = match[1].rm_so;
	return match[1].rm_so;
}

int append_var(struct var_list *list, enum var_type type, char const *key)
{

	return 1;
}

int find_vars(struct var_list *list, char const *src, char *const cc_args[], char *const exec_args[])
{

	return 1;
}
