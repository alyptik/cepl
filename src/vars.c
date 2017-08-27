/*
 * vars.c - variable tracking
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#include "compile.h"
#include "vars.h"

/* fallback linker arg array */
static char *const ld_alt_list[] = {
	"gcc", "-xassembler", "/dev/stdin",
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
	char beg[] = "(^|.*[\\(\\{\\;[:blank:]]+)"
		"(bool|_Bool|_Complex|_Imaginary|struct|union|char|double|float|int|long|short|unsigned|void)"
		"(.*)(";
	char end[] = ")(\\[*)";

	/* append identifier to regex */
	if ((regex = malloc(strlen(id) + sizeof beg + sizeof end - 1)) == NULL)
		ERR("failed to allocate space for regex");
	memset(regex, 0, strlen(id) + sizeof beg + 5);
	memcpy(regex, beg, sizeof beg - 1);
	memcpy(regex + sizeof beg - 1, id, strlen(id));
	memcpy(regex + sizeof beg - 1 + strlen(id), end, sizeof end);

	if (regcomp(&reg, regex, REG_EXTENDED|REG_NEWLINE))
		ERR("failed to compile regex");

	/* non-zero return or -1 value in rm_so means no captures */
	if (regexec(&reg, line, 6, match, 0) || match[3].rm_so == -1) {
		free(regex);
		regfree(&reg);
		return T_ERR;
	}
	if ((type = malloc(match[3].rm_eo - match[2].rm_so + match[5].rm_eo - match[5].rm_so + 1)) == NULL)
		ERR("failed to allocate space for captured type");

	/* copy matched string */
	memset(type, 0, match[3].rm_eo - match[2].rm_so + match[5].rm_eo - match[5].rm_so + 1);
	memcpy(type, line + match[2].rm_so, match[3].rm_eo - match[2].rm_so);
	memcpy(type + match[3].rm_eo - match[2].rm_so, line + match[5].rm_so, match[5].rm_eo - match[5].rm_so);
	regfree(&reg);

	/* string */
	if (regcomp(&reg, "char (const|) \\*$", REG_EXTENDED|REG_NOSUB|REG_NEWLINE))
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
	if (regcomp(&reg, "long double", REG_EXTENDED|REG_NOSUB|REG_NEWLINE))
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
	if (regcomp(&reg, "unsigned", REG_EXTENDED|REG_NOSUB|REG_NEWLINE))
		ERR("failed to compile regex");
	if (!regexec(&reg, type, 1, 0, 0)) {
		free(regex);
		free(type);
		regfree(&reg);
		return T_UINT;
	}
	regfree(&reg);

	/* signed integral */
	if (regcomp(&reg, "(bool|_Bool|short|int|long)", REG_EXTENDED|REG_NOSUB|REG_NEWLINE))
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
	regmatch_t match[2];
	/* second/third capture is ignored */
	char regex[] = ".*[^[:alnum:]_]+"
		"([[:alpha:]_][[:alnum:]_]*)"
		"([^[:alnum:]_=!<>]*[=;]|[^[:alnum:]_=!<>]*[<>]{2}[=;])";

	/* return early if passed NULL pointers */
	if (!line || !id || !offset)
		ERRX("NULL pointer passed to extract_id()");
	if (regcomp(&reg, regex, REG_EXTENDED|REG_ICASE|REG_NEWLINE))
		ERR("failed to compile regex");
	/* non-zero return or -1 value in rm_so means no captures */
	if (regexec(&reg, line, 2, match, 0) || match[1].rm_so == -1) {
		regfree(&reg);
		return 0;
	}
	if ((*id = malloc(match[1].rm_eo - match[1].rm_so + 1)) == NULL)
		ERR("failed to allocate space for captured id");

	/* set the output parameter and return the offset */
	memset(*id, 0, match[1].rm_eo - match[1].rm_so + 1);
	memcpy(*id, line + match[1].rm_so, match[1].rm_eo - match[1].rm_so);
	regfree(&reg);
	*offset = match[1].rm_so;
	return match[1].rm_so;
}

int find_vars(char const *line, struct str_list *id_list, enum var_type **type_list)
{
	size_t off;
	char *line_tmp[2], *id_tmp;

	if (!line || !id_list || !type_list)
		ERRX("NULL pointer passed to find_vars()");
	if ((line_tmp[0] = malloc(strlen(line) + 1)) == NULL)
		ERR("error allocating line_tmp");

	/* initialize lists */
	if (id_list->list)
		free_str_list(id_list);
	init_list(id_list, NULL);

	ssize_t count = id_list->cnt;
	memcpy(line_tmp[0], line, strlen(line) + 1);
	line_tmp[1] = line_tmp[0];
	/* extract all identifiers from the line */
	while (extract_id(line_tmp[0], &id_tmp, &off) != 0) {
		append_str(id_list, id_tmp, 0);
		free(id_tmp);
		id_tmp = NULL;
		line_tmp[0] += off;
		count++;
	}

	/* return early if nothing to do */
	if (count - 1 == -1) {
		free(line_tmp[1]);
		return 0;
	}

	/* get the type of each identifier */
	line_tmp[0] = line_tmp[1];
	enum var_type type_tmp[id_list->cnt];
	for (ssize_t i = count - 1; i < id_list->cnt; i++) {
		if ((type_tmp[i] = extract_type(line_tmp[0], id_list->list[i])) == T_ERR)
			WARNXGEN(id_list->list[i]);
	}

	/* copy it into the output parameter */
	if ((*type_list = malloc(sizeof type_tmp)) == NULL)
		ERR("failed to allocate memory for type_list");
	memcpy(*type_list, type_tmp, sizeof type_tmp);
	free(line_tmp[1]);
	return id_list->cnt;
}

int print_vars(struct var_list *vars, char const *src, char *const cc_args[], char *const exec_args[])
{
	int mem_fd, status, null;
	int pipe_cc[2], pipe_ld[2], pipe_exec[2];
	char src_buffer[strnlen(src, COUNT) + 1];
	char newline[] = "\n\tfprintf(stderr, \"\\n\");";
	char prog_end[] = "\n\treturn 0;\n}\n";
	char print_beg[] = "\n\tfprintf(stderr, \"%s = %___, \", \"";
	char println_beg[] = "\n\tfprintf(stderr, \"%s = %___\\n \", \"";
	char print_end[] = ");";
	char *src_tmp = NULL;
	size_t off = 0;
	/* space for <name>, <name> */
	size_t psz = sizeof print_beg + sizeof print_end + 5;
	size_t plnsz = sizeof println_beg + sizeof print_end + 5;

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
	if ((src_tmp = malloc(sizeof src_buffer)) == NULL)
		ERRGEN("src_tmp malloc()");
	memset(src_tmp, 0, sizeof src_buffer);
	memcpy(src_tmp, src_buffer, sizeof src_buffer);
	off = sizeof src_buffer - 1;
	if ((src_tmp = realloc(src_tmp, strlen(src_tmp) + sizeof newline)) == NULL)
		ERRGEN("src_tmp malloc()");
	memcpy(src_tmp + off, newline, sizeof newline);
	off += sizeof newline - 1;

	/* build var-tracking source */
	for (register int i = 0; i < vars->cnt - 1; i++) {
		if ((src_tmp = realloc(src_tmp, strlen(src_tmp) + (strlen(vars->list[i].key) * 2) + psz)) == NULL)
			ERRGEN("src_tmp malloc()");
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
		case T_ERR:
			strchr(print_tmp, '_')[0] = '0';
			strchr(print_tmp, '_')[0] = '0';
			strchr(print_tmp, '_')[0] = 'p';
		}

		/* copy format string */
		memcpy(src_tmp + off, print_tmp, sizeof print_tmp - 1);
		off += sizeof print_tmp - 1;
		memcpy(src_tmp + off, vars->list[i].key, strlen(vars->list[i].key));
		off += strlen(vars->list[i].key);

		/* handle other variable types */
		switch (vars->list[i].type) {
		case T_OTHER: /* fallthrough */
		case T_ERR:
			memcpy(src_tmp + off, "\", &", strlen("\", &"));
			off += strlen("\", &");
			break;
		default:
			memcpy(src_tmp + off, "\", ", strlen("\", "));
			off += strlen("\", ");
		}

		/* copy final part of printf */
		memcpy(src_tmp + off, vars->list[i].key, strlen(vars->list[i].key));
		off += strlen(vars->list[i].key);
		memcpy(src_tmp + off, print_end, sizeof print_end);
		off += sizeof print_end - 1;
	}

	/* finish source */
	if ((src_tmp = realloc(src_tmp, strlen(src_tmp) + (strlen(vars->list[vars->cnt - 1].key) * 2) + plnsz)) == NULL)
		ERRGEN("src_tmp malloc()");
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
	memcpy(src_tmp + off, print_tmp, sizeof print_tmp - 1);
	off += sizeof print_tmp - 1;
	memcpy(src_tmp + off, vars->list[vars->cnt - 1].key, strlen(vars->list[vars->cnt - 1].key));
	off += strlen(vars->list[vars->cnt - 1].key);

	/* handle other variable types */
	switch (vars->list[vars->cnt - 1].type) {
	case T_OTHER: /* fallthrough */
	case T_ERR:
		memcpy(src_tmp + off, "\", &", strlen("\", &"));
		off += strlen("\", &");
		break;
	default:
		memcpy(src_tmp + off, "\", ", strlen("\", "));
		off += strlen("\", ");
	}

	/* copy final part of printf */
	memcpy(src_tmp + off, vars->list[vars->cnt - 1].key, strlen(vars->list[vars->cnt - 1].key));
	off += strlen(vars->list[vars->cnt - 1].key);
	memcpy(src_tmp + off, print_end, sizeof print_end);
	off += sizeof print_end - 1;

	/* copy final source into buffer */
	char final[off + sizeof prog_end + 1];
	memset(final, 0, sizeof final);
	memcpy(final, src_tmp, off);
	memcpy(final + off, prog_end, sizeof prog_end);
	puts(final);
	/* remove NULL bytes */
	while (memchr(final, 0, sizeof final) != NULL)
		((char *)memchr(final, 0, sizeof final))[0] = '\n';
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

	return 1;
}
