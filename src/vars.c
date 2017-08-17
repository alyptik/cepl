/*
 * vars.c - variable tracking
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#include "compile.h"
#include "vars.h"

static inline enum var_type extract_type(char const *line, char const *id)
{

	return T_OTHER;
}

static inline size_t extract_id(char const *line, char **id, size_t *offset)
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

static inline void append_var(struct var_list *list_struct, bool is_arr, size_t size, size_t nmemb, char const *key, enum var_type type, void **val)
{
	void *tmp;
	if (!list_struct || size < 1 || nmemb < 1 || !key || !val)
		err(EXIT_FAILURE, "%s %d", "invalid arguments passed to append_var() at", __LINE__);
	if ((tmp = realloc(list_struct->list, (sizeof *list_struct->list) * ++list_struct->cnt)) == NULL)
		err(EXIT_FAILURE, "%s %d %s", "error during var_list (cnt = ", list_struct->cnt, ") realloc()");
	list_struct->list = tmp;
	list_struct->list[list_struct->cnt - 1].is_arr = is_arr;
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
