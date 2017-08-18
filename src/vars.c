/*
 * vars.c - variable tracking
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#include "compile.h"
#include "vars.h"

/* silence linter */
long syscall(long number, ...);
int fexecve(int mem_fd, char *const argv[], char *const envp[]);
size_t strnlen(char const *s, size_t maxlen);
int regcomp(regex_t *preg, char const *regex, int cflags);
int regexec(regex_t const *preg, char const *string, size_t nmatch, regmatch_t pmatch[], int eflags);
size_t regerror(int errcode, regex_t const *preg, char *errbuf, size_t errbuf_size);
void regfree(regex_t *preg);
void *mmap(void *addr, size_t len, int prot, int flags, int fildes, off_t off);

enum var_type extract_type(char const *line, char const *id)
{
	regex_t reg;
	regmatch_t match[6];
	/* return early if passed NULL pointers */
	if (!line || !id)
		return T_ERR;
	/* first/fourth captures are ignored */
	char *regex, *type;
	char beg[] = "(^|.*[\\(\\{\\;[:blank:]]+)"
		"(bool|_Bool|_Complex|_Imaginary|struct|union|char|double|float|int|long|short|unsigned|void)"
		"(.*)[[:blank:]](";
	char end[] = ")(\\[*)";

	/* append identifier to regex */
	if ((regex = malloc(strlen(id) + sizeof beg + sizeof end - 1)) == NULL)
		err(EXIT_FAILURE, "%s", "failed to allocate space for regex");
	memset(regex, 0, strlen(id) + sizeof beg + 5);
	memcpy(regex, beg, sizeof beg - 1);
	memcpy(regex + sizeof beg - 1, id, strlen(id));
	memcpy(regex + sizeof beg - 1 + strlen(id), end, sizeof end);

	if (regcomp(&reg, regex, REG_EXTENDED|REG_NEWLINE))
		err(EXIT_FAILURE, "%s %d", "failed to compile regex at", __LINE__);

	/* non-zero return or -1 value in rm_so means no captures */
	if (regexec(&reg, line, 6, match, 0) || match[3].rm_so == -1) {
		free(regex);
		return T_ERR;
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
	if (regcomp(&reg, "(bool|_Bool|short|int|long)", REG_EXTENDED|REG_NOSUB|REG_NEWLINE))
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

int find_vars(char const *src, char *const cc_args[], char *const exec_args[])
{
	if (!src || !cc_args || !exec_args)
		errx(EXIT_FAILURE, "%s", "NULL pointer passed to find_vars()");

	int mem_fd, status;
	int pipe_cc[2], pipe_ld[2], pipe_exec[2];
	char src_buffer[strnlen(src, COUNT) + 1];
	char prog_end[] = "\n\treturn 0;\n}\n";
	char print_beg[] = "printf(\"%s = %p\\n\",";
	char print_end[] = ");";
	char *final = NULL, *src_tmp = NULL, *id_tmp = NULL;
	size_t off = 0;
	struct var_list list = {0, NULL};

	if (sizeof src_buffer < 2)
		errx(EXIT_FAILURE, "%s", "empty source string passed to find_vars()");

	init_var_list(&list);
	/* add trailing '\n' */
	memcpy(src_buffer, src, sizeof src_buffer);
	src_buffer[sizeof src_buffer - 1] = '\n';
	src_tmp = src_buffer;

	while (extract_id(src_tmp, &id_tmp, &off) != 0) {
		src_tmp += off;
		append_var(&list, 16, 1, id_tmp, extract_type(src_tmp, id_tmp), (void **)&id_tmp);
	}

	if ((final = malloc(sizeof src_buffer + sizeof prog_end + (list.cnt * 16))) == NULL)
		errx(EXIT_FAILURE, "%s", "empty allocating source buffer in find_vars()");

	/* create pipes */
	if (pipe(pipe_cc) == -1)
		err(EXIT_FAILURE, "%s", "error making pipe_cc pipe");
	if (pipe(pipe_ld) == -1)
		err(EXIT_FAILURE, "%s", "error making pipe_ld pipe");
	if (pipe(pipe_exec) == -1)
		err(EXIT_FAILURE, "%s", "error making pipe_exec pipe");
	/* set close-on-exec for pipe fds */
	set_cloexec(pipe_cc);
	set_cloexec(pipe_ld);
	set_cloexec(pipe_exec);

	/* fork compiler */
	switch (fork()) {
	/* error */
	case -1:
		close(pipe_cc[0]);
		close(pipe_cc[1]);
		close(pipe_ld[0]);
		close(pipe_ld[1]);
		close(pipe_exec[0]);
		close(pipe_exec[1]);
		err(EXIT_FAILURE, "%s", "error forking compiler");
		break;

	/* child */
	case 0:
		dup2(pipe_cc[0], 0);
		dup2(pipe_ld[1], 1);
		execvp(cc_args[0], cc_args);
		/* execvp() should never return */
		err(EXIT_FAILURE, "%s", "error forking compiler");
		break;

	/* parent */
	default:
		close(pipe_cc[0]);
		close(pipe_ld[1]);
		if (write(pipe_cc[1], src_buffer, sizeof src_buffer) == -1)
			err(EXIT_FAILURE, "%s", "error writing to pipe_cc[1]");
		close(pipe_cc[1]);
		wait(&status);
		if (WIFEXITED(status) && WEXITSTATUS(status)) {
			warnx("%s", "compiler returned non-zero exit code");
			free(final);
			free(list.list);
			return WEXITSTATUS(status);
		}
	}

	free(final);
	free(list.list);
	return 1;
}
