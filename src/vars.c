/*
 * vars.c - variable tracking
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#include "vars.h"

/* fallback linker arg array */
static char *const ld_alt_list[] = {
	"gcc",
	"-O0", "-pipe",
	"-fPIC", "-std=c11",
	"-Wno-unused-parameter",
	"-xassembler", "/dev/stdin",
	"-o/dev/stdout", NULL
};

/* global linker arguments struct */
extern struct str_list ld_list;
extern char **environ;

/* silence linter */
long syscall(long number, ...);
int fexecve(int mem_fd, char *const argv[], char *const envp[]);
size_t strnlen(char const *s, size_t maxlen);
void *mmap(void *addr, size_t len, int prot, int flags, int fildes, off_t off);

enum var_type extract_type(char const *line, char const *id)
{
	regex_t reg;
	regmatch_t match[6];
	/* return early if passed NULL pointers */
	if (!line || !id)
		ERRX("NULL pointer passed to extract_type()");

	/* first/fourth captures are ignored */
	char *regex, *type;
	char const beg[] = "(^|.*[({;[:blank:]]*)"
		"(bool|_Bool|_Complex|_Imaginary|struct|union|char|double|float|int|long|short|unsigned|void)"
		"(.*)(";
	char const end[] = ")(\\[*)";

	/* append identifier to regex */
	if ((regex = calloc(1, strlen(id) + sizeof beg + sizeof end - 1)) == NULL)
		ERR("failed to allocate space for regex");
	memcpy(regex, beg, sizeof beg - 1);
	memcpy(regex + sizeof beg - 1, id, strlen(id));
	memcpy(regex + sizeof beg - 1 + strlen(id), end, sizeof end);

	if (regcomp(&reg, regex, REG_EXTENDED|REG_NEWLINE))
		ERR("failed to compile regex");

	/* non-zero return or -1 value in rm_so means no captures */
	if (regexec(&reg, line, 6, match, 0) || match[2].rm_so == -1) {
		free(regex);
		regfree(&reg);
		return T_ERR;
	}
	if ((type = calloc(1, match[3].rm_eo - match[2].rm_so + match[5].rm_eo - match[5].rm_so + 1)) == NULL)
		ERR("failed to allocate space for captured type");

	/* copy matched string */
	memcpy(type, line + match[2].rm_so, match[3].rm_eo - match[2].rm_so);
	memcpy(type + match[3].rm_eo - match[2].rm_so, line + match[5].rm_so, match[5].rm_eo - match[5].rm_so);
	regfree(&reg);

	/* string */
	if (regcomp(&reg, "char[[:blank:]]*(const[[:blank:]]*|)\\*[[:blank:]]*", REG_EXTENDED|REG_NOSUB|REG_NEWLINE))
		ERR("failed to compile regex");
	if (!regexec(&reg, type, 1, 0, 0)) {
		free(regex);
		free(type);
		regfree(&reg);
		return T_STR;
	}
	regfree(&reg);

	/* pointer */
	if (regcomp(&reg, "(\\*|\\[)", REG_EXTENDED|REG_NOSUB|REG_NEWLINE))
		ERR("failed to compile regex");
	if (!regexec(&reg, type, 1, 0, 0)) {
		free(regex);
		free(type);
		regfree(&reg);
		return T_PTR;
	}
	regfree(&reg);

	/* char */
	if (regcomp(&reg, "char", REG_EXTENDED|REG_NOSUB|REG_NEWLINE))
		ERR("failed to compile regex");
	if (!regexec(&reg, type, 1, 0, 0)) {
		free(regex);
		free(type);
		regfree(&reg);
		return T_CHR;
	}
	regfree(&reg);

	/* long double */
	if (regcomp(&reg, "long[[:blank:]]*double", REG_EXTENDED|REG_NOSUB|REG_NEWLINE))
		ERR("failed to compile regex");
	if (!regexec(&reg, type, 1, 0, 0)) {
		free(regex);
		free(type);
		regfree(&reg);
		return T_LDBL;
	}
	regfree(&reg);

	/* double */
	if (regcomp(&reg, "(float|double)", REG_EXTENDED|REG_NOSUB|REG_NEWLINE))
		ERR("failed to compile regex");
	if (!regexec(&reg, type, 1, 0, 0)) {
		free(regex);
		free(type);
		regfree(&reg);
		return T_DBL;
	}
	regfree(&reg);

	/* unsigned integral */
	if (regcomp(&reg, "(size_t|unsigned)", REG_EXTENDED|REG_NOSUB|REG_NEWLINE))
		ERR("failed to compile regex");
	if (!regexec(&reg, type, 1, 0, 0)) {
		free(regex);
		free(type);
		regfree(&reg);
		return T_UINT;
	}
	regfree(&reg);

	/* signed integral */
	if (regcomp(&reg, "(bool|_Bool|short|int|long|ptrdiff_t)", REG_EXTENDED|REG_NOSUB|REG_NEWLINE))
		ERR("failed to compile regex");
	if (!regexec(&reg, type, 1, 0, 0)) {
		free(regex);
		free(type);
		regfree(&reg);
		return T_INT;
	}
	regfree(&reg);

	/* return fallback type */
	free(regex);
	free(type);
	return T_OTHER;
}

size_t extract_id(char const *line, char **id, size_t *offset)
{
	regex_t reg;
	regmatch_t match[4];
	/* second capture is ignored */
	char const initial_regex[] = "^[^,({;&|]*[^,({;&|[:alnum:]_]+"
		"([[:alpha:]_][[:alnum:]_]*)[[:blank:]]*"
		"(=[^,({;&|'\"_=!<>[:alnum:]]+|<>{2}=[^,({;&|'\"_=!<>[:alnum:]]*|=[^=]+)";

	/* return early if passed NULL pointers */
	if (!line || !id || !offset)
		ERRX("NULL pointer passed to extract_id()");

	if (regcomp(&reg, initial_regex, REG_EXTENDED|REG_NEWLINE))
		ERR("failed to compile regex");
	/* non-zero return or -1 value in rm_so means no captures */
	if (regexec(&reg, line, 3, match, 0) || match[1].rm_so == -1) {
		/* fallback branch */
		regfree(&reg);
		/* first/second/fourth capture is ignored */
		char const middle_regex[] =
			"(^|[^t][^y][^p][^e][^d][^e][^,({;&|'\"f]+[[:blank:]]+)"
			"(bool|_Bool|_Complex|_Imaginary|struct|union|"
			"char|double|float|int|long|short|unsigned|void)"
			"[^,({;&|'\"[:alpha:]]+[[:blank:]]*\\**[[:blank:]]*"
			"([[:alpha:]_][[:alnum:]_]*)[[:blank:]]*"
			"([^({;&|'\"[:alnum:][:blank:]]+$|$|,)";

		if (regcomp(&reg, middle_regex, REG_EXTENDED|REG_NEWLINE))
			ERR("failed to compile regex");
		if (regexec(&reg, line, 5, match, 0) || match[3].rm_so == -1) {
			regfree(&reg);
			/* first/second/fourth capture is ignored */
			char const final_regex[] =
				"(^|[^,({;&|'\"]+)"
				"(|bool|_Bool|_Complex|_Imaginary|struct|union|"
				"char|double|float|int|long|short|unsigned|void)"
				",[[:blank:]]*\\**[[:blank:]]*"
				"([[:alpha:]_][[:alnum:]_]*)";

			if (regcomp(&reg, final_regex, REG_EXTENDED|REG_NEWLINE))
				ERR("failed to compile regex");
			if (regexec(&reg, line, 4, match, 0) || match[3].rm_so == -1) {
				regfree(&reg);
				return 0;
			}

			if ((*id = calloc(1, match[3].rm_eo - match[3].rm_so + 1)) == NULL)
				ERR("failed to allocate space for captured id");
			/* set the output parameter and return the offset */
			memcpy(*id, line + match[3].rm_so, match[3].rm_eo - match[3].rm_so);
			regfree(&reg);
			*offset = match[3].rm_so;
			return match[3].rm_so;
		}

		if ((*id = calloc(1, match[3].rm_eo - match[3].rm_so + 1)) == NULL)
			ERR("failed to allocate space for captured id");
		/* set the output parameter and return the offset */
		memcpy(*id, line + match[3].rm_so, match[3].rm_eo - match[3].rm_so);
		regfree(&reg);
		*offset = match[3].rm_so;
		return match[3].rm_so;
	}

	/* normal branch */
	if ((*id = calloc(1, match[1].rm_eo - match[1].rm_so + 1)) == NULL)
		ERR("failed to allocate space for captured id");
	/* set the output parameter and return the offset */
	memcpy(*id, line + match[1].rm_so, match[1].rm_eo - match[1].rm_so);
	regfree(&reg);
	*offset = match[1].rm_so;
	return match[1].rm_so;
}

int find_vars(char const *line, struct str_list *id_list, enum var_type **type_list)
{
	size_t off;
	char *line_tmp[2], *id_tmp = NULL;

	/* sanity checks */
	if (!line || !id_list || !type_list)
		return 0;
	if ((line_tmp[0] = calloc(1, strlen(line) + 1)) == NULL)
		ERR("error allocating line_tmp");
	line_tmp[1] = line_tmp[0];

	/* initialize lists */
	if (*type_list)
		free(*type_list);
	*type_list = NULL;
	if (id_list->list)
		free_str_list(id_list);
	init_list(id_list, NULL);

	size_t count = id_list->cnt;
	memcpy(line_tmp[1], line, strlen(line) + 1);
	/* extract all identifiers from the line */
	while (line_tmp[1] && extract_id(line_tmp[1], &id_tmp, &off) != 0) {
		append_str(id_list, id_tmp, 0);
		free(id_tmp);
		id_tmp = NULL;
		line_tmp[1] += off;
		count++;
	}
	/* second pass */
	if (id_tmp) {
		free(id_tmp);
		id_tmp = NULL;
	}
	if ((line_tmp[1] = strpbrk(line_tmp[0], ";")) != NULL) {
		while (strpbrk(line_tmp[1], ";") != NULL)
			line_tmp[1] = strpbrk(line_tmp[1], ";") + 1;
		while (line_tmp[1] && extract_id(line_tmp[1], &id_tmp, &off) != 0) {
			append_str(id_list, id_tmp, 0);
			free(id_tmp);
			id_tmp = NULL;
			line_tmp[1] += off;
			count++;
		}
	}

	/* return early if nothing to do */
	if (count == 0) {
		if (id_tmp)
			free(id_tmp);
		if (line_tmp[0])
			free(line_tmp[0]);
		return 0;
	}

	/* get the type of each identifier */
	line_tmp[1] = line_tmp[0];
	enum var_type type_tmp[id_list->cnt];
	for (size_t i = 0; i < id_list->cnt; i++) {
		if ((type_tmp[i] = extract_type(line_tmp[1], id_list->list[i])) == T_ERR)
			WARNX("unable to find type of variable");
	}

	/* copy it into the output parameter */
	if ((*type_list = calloc(1, sizeof type_tmp)) == NULL)
		ERR("failed to allocate memory for type_list");
	memcpy(*type_list, type_tmp, sizeof type_tmp);
	if (id_tmp)
		free(id_tmp);
	if (line_tmp[0])
		free(line_tmp[0]);
	return id_list->cnt;
}

int print_vars(struct var_list *vars, char const *src, char *const cc_args[], char *const exec_args[])
{
	int mem_fd, status, null;
	int pipe_cc[2], pipe_ld[2], pipe_exec[2];
	char src_buffer[strnlen(src, COUNT) + 1];
	char newline[] = "\n\tfprintf(stderr, \"\\n\");";
	char prog_end[] = "\n\treturn 0;\n}\n";
	char print_beg[] = "\n\tfprintf(stderr, \"%s = “%___”, \", \"";
	char println_beg[] = "\n\tfprintf(stderr, \"%s = “%___”\\n \", \"";
	char print_end[] = ");";
	char *src_tmp = NULL, *tmp_ptr = NULL;
	/* space for <name>, <name> */
	size_t psz = sizeof print_beg + sizeof print_end + 7;
	size_t plnsz = sizeof println_beg + sizeof print_end + 7;
	size_t off = 0;

	/* sanity checks */
	if (!vars || !src || !cc_args || !exec_args)
		ERRX("NULL pointer passed to print_vars()");
	if (sizeof src_buffer < 2)
		ERRX("empty source string passed to print_vars()");
	/* return early if nothing to do */
	if (vars->cnt == 0)
		return -1;

	/* copy source buffer */
	memcpy(src_buffer, src, sizeof src_buffer);
	if ((src_tmp = calloc(1, sizeof src_buffer)) == NULL)
		ERRGEN("src_tmp calloc()");
	memcpy(src_tmp, src_buffer, sizeof src_buffer);
	off = sizeof src_buffer - 1;
	if (!(tmp_ptr = realloc(src_tmp, strlen(src_tmp) + sizeof newline))) {
		free(src_tmp);
		ERRGEN("src_tmp realloc()");
	}
	src_tmp = tmp_ptr;
	memcpy(src_tmp + off, newline, sizeof newline);
	off += sizeof newline - 1;

	/* build var-tracking source */
	for (size_t i = 0; i < vars->cnt - 1; i++) {
		size_t cur_sz = (strlen(vars->list[i].key) + 1) * 2;
		if (!(tmp_ptr = realloc(src_tmp, strlen(src_tmp) + cur_sz + psz))) {
			free(src_tmp);
			ERRGEN("src_tmp realloc()");
		}
		src_tmp = tmp_ptr;
		char print_tmp[sizeof print_beg];
		memcpy(print_tmp, print_beg, sizeof print_beg);

		/* build format string */
		switch (vars->list[i].type) {
		case T_CHR:
			strchr(print_tmp, '_')[0] = '0';
			strchr(print_tmp, '_')[0] = '0';
			strchr(print_tmp, '_')[0] = 'c';
			break;
		case T_STR:
			strchr(print_tmp, '_')[0] = '0';
			strchr(print_tmp, '_')[0] = '0';
			strchr(print_tmp, '_')[0] = 's';
			break;
		case T_INT:
			strchr(print_tmp, '_')[0] = 'l';
			strchr(print_tmp, '_')[0] = 'l';
			strchr(print_tmp, '_')[0] = 'd';
			break;
		case T_UINT:
			strchr(print_tmp, '_')[0] = 'l';
			strchr(print_tmp, '_')[0] = 'l';
			strchr(print_tmp, '_')[0] = 'u';
			break;
		case T_DBL:
			strchr(print_tmp, '_')[0] = '0';
			strchr(print_tmp, '_')[0] = 'l';
			strchr(print_tmp, '_')[0] = 'f';
			break;
		case T_LDBL:
			strchr(print_tmp, '_')[0] = '0';
			strchr(print_tmp, '_')[0] = 'L';
			strchr(print_tmp, '_')[0] = 'f';
			break;
		case T_PTR: /* fallthrough */
		case T_OTHER: /* fallthrough */
		case T_ERR: /* fallthrough */
		default:
			strchr(print_tmp, '_')[0] = '0';
			strchr(print_tmp, '_')[0] = '0';
			strchr(print_tmp, '_')[0] = 'p';
		}

		/* copy format string */
		memcpy(src_tmp + off, print_tmp, sizeof print_tmp);
		off += sizeof print_tmp - 1;
		memcpy(src_tmp + off, vars->list[i].key, strlen(vars->list[i].key));
		off += strlen(vars->list[i].key);

		/* handle other variable types */
		switch (vars->list[i].type) {
		case T_OTHER: /* fallthrough */
		case T_ERR:
			memcpy(src_tmp + off, "\", &", 5);
			off += 4;
			break;
		default:
			memcpy(src_tmp + off, "\", ", 4);
			off += 3;
		}

		/* copy final part of printf */
		memcpy(src_tmp + off, vars->list[i].key, strlen(vars->list[i].key));
		off += strlen(vars->list[i].key);
		memcpy(src_tmp + off, print_end, sizeof print_end);
		off += sizeof print_end - 1;
	}

	/* finish source */
	size_t key_sz = (strlen(vars->list[vars->cnt - 1].key) + 1) * 2;
	if (!(tmp_ptr = realloc(src_tmp, strlen(src_tmp) + key_sz + plnsz))) {
		free(src_tmp);
		ERRGEN("src_tmp realloc()");
	}
	src_tmp = tmp_ptr;
	char print_tmp[sizeof println_beg];
	memcpy(print_tmp, println_beg, sizeof println_beg);

	/* build format string */
	switch (vars->list[vars->cnt - 1].type) {
	case T_CHR:
		strchr(print_tmp, '_')[0] = '0';
		strchr(print_tmp, '_')[0] = '0';
		strchr(print_tmp, '_')[0] = 'c';
		break;
	case T_STR:
		strchr(print_tmp, '_')[0] = '0';
		strchr(print_tmp, '_')[0] = '0';
		strchr(print_tmp, '_')[0] = 's';
		break;
	case T_INT:
		strchr(print_tmp, '_')[0] = 'l';
		strchr(print_tmp, '_')[0] = 'l';
		strchr(print_tmp, '_')[0] = 'd';
		break;
	case T_UINT:
		strchr(print_tmp, '_')[0] = 'l';
		strchr(print_tmp, '_')[0] = 'l';
		strchr(print_tmp, '_')[0] = 'u';
		break;
	case T_DBL:
		strchr(print_tmp, '_')[0] = '0';
		strchr(print_tmp, '_')[0] = 'l';
		strchr(print_tmp, '_')[0] = 'f';
		break;
	case T_LDBL:
		strchr(print_tmp, '_')[0] = '0';
		strchr(print_tmp, '_')[0] = 'L';
		strchr(print_tmp, '_')[0] = 'f';
		break;
	case T_PTR: /* fallthrough */
	case T_OTHER: /* fallthrough */
	case T_ERR:
		strchr(print_tmp, '_')[0] = '0';
		strchr(print_tmp, '_')[0] = '0';
		strchr(print_tmp, '_')[0] = 'p';
	}

	/* copy format string */
	memcpy(src_tmp + off, print_tmp, sizeof print_tmp);
	off += sizeof print_tmp - 1;
	memcpy(src_tmp + off, vars->list[vars->cnt - 1].key, strlen(vars->list[vars->cnt - 1].key));
	off += strlen(vars->list[vars->cnt - 1].key);

	/* handle other variable types */
	switch (vars->list[vars->cnt - 1].type) {
	case T_OTHER: /* fallthrough */
	case T_ERR:
		memcpy(src_tmp + off, "\", &", 5);
		off += 4;
		break;
	default:
		memcpy(src_tmp + off, "\", ", 4);
		off += 3;
	}

	/* copy final part of printf */
	memcpy(src_tmp + off, vars->list[vars->cnt - 1].key, strlen(vars->list[vars->cnt - 1].key));
	off += strlen(vars->list[vars->cnt - 1].key);
	memcpy(src_tmp + off, print_end, sizeof print_end - 1);
	off += sizeof print_end - 1;

	/* copy final source into buffer */
	char final[off + sizeof prog_end + 1], *tok_buf;
	memset(final, 0, sizeof final);
	memcpy(final, src_tmp, off);
	memcpy(final + off, prog_end, sizeof prog_end);
	/* remove NULL bytes */
	while ((tok_buf = memchr(final, 0, sizeof final)) != NULL)
		tok_buf[0] = '\n';
	free(src_tmp);

	/* create pipes */
	if (pipe(pipe_cc) == -1)
		ERR("error making pipe_cc pipe");
	if (pipe(pipe_ld) == -1)
		ERR("error making pipe_ld pipe");
	if (pipe(pipe_exec) == -1)
		ERR("error making pipe_exec pipe");
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
		ERR("error forking compiler");
		break;

	/* child */
	case 0:
		dup2(pipe_cc[0], 0);
		dup2(pipe_ld[1], 1);
		execvp(cc_args[0], cc_args);
		/* execvp() should never return */
		ERR("error forking compiler");
		break;

	/* parent */
	default:
		close(pipe_cc[0]);
		close(pipe_ld[1]);
		if (write(pipe_cc[1], final, sizeof final) == -1)
			ERR("error writing to pipe_cc[1]");
		close(pipe_cc[1]);
		wait(&status);
		if (WIFEXITED(status) && WEXITSTATUS(status)) {
			WARNX("compiler returned non-zero exit code");
			return WEXITSTATUS(status);
		}
	}

	/* fork linker */
	switch (fork()) {
	/* error */
	case -1:
		close(pipe_ld[0]);
		close(pipe_exec[0]);
		close(pipe_exec[1]);
		ERR("error forking linker");
		break;

	/* child */
	case 0:
		dup2(pipe_ld[0], 0);
		dup2(pipe_exec[1], 1);
		if (ld_list.list)
			execvp(ld_list.list[0], ld_list.list);
		/* fallback linker exec */
		execvp(ld_alt_list[0], ld_alt_list);
		/* execvp() should never return */
		ERR("error forking linker");
		break;

	/* parent */
	default:
		close(pipe_ld[0]);
		close(pipe_exec[1]);
		wait(&status);
		if (WIFEXITED(status) && WEXITSTATUS(status)) {
			WARNX("linker returned non-zero exit code");
			return WEXITSTATUS(status);
		}
	}

	/* fork executable */
	switch (fork()) {
	/* error */
	case -1:
		close(pipe_exec[0]);
		ERR("error forking executable");
		break;

	/* child */
	case 0:
		if ((mem_fd = syscall(SYS_memfd_create, "cepl_memfd", MFD_CLOEXEC)) == -1)
			ERR("error creating mem_fd");
		pipe_fd(pipe_exec[0], mem_fd);
		/* redirect stdout to /dev/null */
		if (!(null = open("/dev/null", O_WRONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)))
		err(EXIT_FAILURE, "%s", "open() error!");
		dup2(null, 1);
		fexecve(mem_fd, exec_args, environ);
		/* fexecve() should never return */
		ERR("error forking executable");
		break;

	/* parent */
	default:
		close(pipe_exec[0]);
		wait(&status);
		/* don't overwrite non-zero exit status from compiler */
		if (WIFEXITED(status) && WEXITSTATUS(status)) {
			WARNX("executable returned non-zero exit code");
			return WEXITSTATUS(status);
		}
	}

	return 0;
}
