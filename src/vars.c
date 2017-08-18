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
	regex_t reg;
	regmatch_t match[6];
	/* return early if passed NULL pointers */
	if (!line || !id)
		return T_OTHER;
	/* first/fourth captures are ignored */
	char *regex, *type;
	char beg[] = "(^|.*[\\(\\{\\;[:blank:]]+)"
		"(struct|union|char|double|float|int|long|short|unsigned|void)"
		"(.*)[[:blank:]](";
	char end[] = ")(\\[*)";

	/* append identifier to regex */
	if ((regex = malloc(strlen(id) + sizeof beg + 5)) == NULL)
		err(EXIT_FAILURE, "%s", "failed to allocate space for regex");
	memset(regex, 0, strlen(id) + sizeof beg + 5);
	memcpy(regex, beg, sizeof beg - 1);
	memcpy(regex + sizeof beg - 1, id, strlen(id));
	memcpy(regex + sizeof beg - 1 + strlen(id), end, sizeof end);

	if (regcomp(&reg, regex, REG_EXTENDED|REG_ICASE|REG_NEWLINE))
		err(EXIT_FAILURE, "%s %d", "failed to compile regex at", __LINE__);

	/* non-zero return or -1 value in rm_so means no captures */
	if (regexec(&reg, line, 6, match, 0) || match[5].rm_so == -1) {
		free(regex);
		return T_OTHER;
	}
	if ((type = malloc(match[3].rm_eo - match[2].rm_so + match[5].rm_eo - match[5].rm_so + 1)) == NULL)
		err(EXIT_FAILURE, "%s", "failed to allocate space for captured type");

	/* copy matched string */
	memset(type, 0, match[3].rm_eo - match[2].rm_so + match[5].rm_eo - match[5].rm_so + 1);
	memcpy(type, line + match[2].rm_so, match[3].rm_eo - match[2].rm_so);
	memcpy(type + match[3].rm_eo - match[2].rm_so, line + match[5].rm_so, match[5].rm_eo - match[5].rm_so);

	/* string */
	if (regcomp(&reg, "char \\*", REG_EXTENDED|REG_NOSUB|REG_NEWLINE))
		err(EXIT_FAILURE, "%s %d", "failed to compile regex at", __LINE__);
	if (!regexec(&reg, type, 1, 0, 0)) {
		free(regex);
		free(type);
		return T_STR;
	}

	/* pointer */
	if (regcomp(&reg, "(\\*|\\[)", REG_EXTENDED|REG_NOSUB|REG_NEWLINE))
		err(EXIT_FAILURE, "%s %d", "failed to compile regex at", __LINE__);
	if (!regexec(&reg, type, 1, 0, 0)) {
		free(regex);
		free(type);
		return T_PTR;
	}

	/* char */
	if (regcomp(&reg, "char", REG_EXTENDED|REG_NOSUB|REG_NEWLINE))
		err(EXIT_FAILURE, "%s %d", "failed to compile regex at", __LINE__);
	if (!regexec(&reg, type, 1, 0, 0)) {
		free(regex);
		free(type);
		return T_CHR;
	}

	/* long double */
	if (regcomp(&reg, "long double", REG_EXTENDED|REG_NOSUB|REG_NEWLINE))
		err(EXIT_FAILURE, "%s %d", "failed to compile regex at", __LINE__);
	if (!regexec(&reg, type, 1, 0, 0)) {
		free(regex);
		free(type);
		return T_LDBL;
	}

	/* double */
	if (regcomp(&reg, "(float|double)", REG_EXTENDED|REG_NOSUB|REG_NEWLINE))
		err(EXIT_FAILURE, "%s %d", "failed to compile regex at", __LINE__);
	if (!regexec(&reg, type, 1, 0, 0)) {
		free(regex);
		free(type);
		return T_DBL;
	}

	/* unsigned integral */
	if (regcomp(&reg, "unsigned", REG_EXTENDED|REG_NOSUB|REG_NEWLINE))
		err(EXIT_FAILURE, "%s %d", "failed to compile regex at", __LINE__);
	if (!regexec(&reg, type, 1, 0, 0)) {
		free(regex);
		free(type);
		return T_UINT;
	}

	/* signed integral */
	if (regcomp(&reg, "(short|int|long)", REG_EXTENDED|REG_NOSUB|REG_NEWLINE))
		err(EXIT_FAILURE, "%s %d", "failed to compile regex at", __LINE__);
	if (!regexec(&reg, type, 1, 0, 0)) {
		free(regex);
		free(type);
		return T_INT;
	}

	/* return fallback type */
	free(regex);
	free(type);
	return T_OTHER;
}

size_t extract_id(char const *line, char **id, size_t *offset)
{
	regex_t reg;
	regmatch_t match[2];
	/* second capture is ignored */
	char regex[] = ".*[^[:alnum:]]+"
		"([[:alpha:]_][[:alnum:]_]*)"
		"([^[:alnum:]=!<>]*=|[^[:alnum:]=!<>]*[<>]{2}=*)"
		"[^=]*";

	/* return early if passed NULL pointers */
	if (!line || !id || !offset)
		return 0;
	if (regcomp(&reg, regex, REG_EXTENDED|REG_ICASE|REG_NEWLINE))
		err(EXIT_FAILURE, "%s", "failed to compile regex");
	/* non-zero return or -1 value in rm_so means no captures */
	if (regexec(&reg, line, 2, match, 0) || match[1].rm_so == -1)
		return 0;
	if ((*id= malloc(match[1].rm_eo - match[1].rm_so + 1)) == NULL)
		err(EXIT_FAILURE, "%s", "failed to allocate space for captured id");

	/* set the output parameter and return the offset */
	memset(*id, 0, match[1].rm_eo - match[1].rm_so + 1);
	memcpy(*id, line + match[1].rm_so, match[1].rm_eo - match[1].rm_so);
	*offset = match[1].rm_so;
	return match[1].rm_so;
}

static inline void append_var(struct var_list *list_struct, size_t size, size_t nmemb, char const *key, enum var_type type, void **val)
{
	void *tmp;
	if (!list_struct || size < 1 || nmemb < 1 || !key || !val)
		err(EXIT_FAILURE, "%s %d", "invalid arguments passed to append_var() at", __LINE__);
	if ((tmp = realloc(list_struct->list, (sizeof *list_struct->list) * ++list_struct->cnt)) == NULL)
		err(EXIT_FAILURE, "%s %d %s", "error during var_list (cnt = ", list_struct->cnt, ") realloc()");
	list_struct->list = tmp;
	list_struct->list[list_struct->cnt - 1].size = size;
	list_struct->list[list_struct->cnt - 1].nmemb = nmemb;
	list_struct->list[list_struct->cnt - 1].key = key;
	list_struct->list[list_struct->cnt - 1].type = type;
	switch (type) {
	case T_CHR: /* fallthrough */
	case T_INT:
		list_struct->list[list_struct->cnt - 1].int_val = *((long long *)*val);
		break;
	case T_UINT:
		list_struct->list[list_struct->cnt - 1].uint_val = *((unsigned long long *)*val);
		break;
	case T_DBL:
		list_struct->list[list_struct->cnt - 1].dbl_val = *((double *)*val);
		break;
	case T_LDBL:
		list_struct->list[list_struct->cnt - 1].ldbl_val = *((long double *)*val);
		break;
	case T_STR: /* fallthrough */
	case T_PTR:
		list_struct->list[list_struct->cnt - 1].ptr_val = (void *)*val;
		break;
	case T_OTHER: /* fallthrough */
	/* for the default case only track the address of the object */
	default:
		list_struct->list[list_struct->cnt - 1].ptr_val = val;
	}
}

int find_vars(struct var_list *list, char const *src, char *const cc_args[], char *const exec_args[])
{

	return 1;
}
