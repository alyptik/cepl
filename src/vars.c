/*
 * vars.c - variable tracking
 *
 * AUTHOR: Joey Pabalinas <joeypabalinas@gmail.com>
 * See LICENSE.md file for copyright and license details.
 */

/* silence linter */
#ifndef _GNU_SOURCE
#	define _GNU_SOURCE
#endif

#include "vars.h"

/* fallback linker arg array */
static char *const ld_alt_list[] = {
	"gcc",
	"-O0", "-pipe", "-fPIC",
	"-xassembler", "-",
	"-lm", "-o", "/dev/stdout",
	NULL
};

extern char const *prelude, *prog_start, *prog_start_user, *prog_end;
/* global linker arguments struct */
extern struct str_list ld_list;
extern char **environ;

/* silence linter */
long syscall(long __sysno, ...);
int fexecve(int __fd, char *const __argv[], char *const __envp[]);
void *mmap(void *__addr, size_t __len, int __prot, int __flags, int __fd, off_t __offset);
void sync(void);

enum var_type extract_type(char const *restrict ln, char const *restrict id)
{
	regex_t reg;
	regmatch_t matches[7];
	/* return early if passed NULL pointers */
	if (!ln || !id)
		ERRX("%s", "NULL pointer passed to extract_type()");

	/* strip parentheses */
	for (char *lparens = strchr(id, '('); lparens; (lparens = strchr(id, '(')))
		*lparens = '.';
	for (char *rparens = strchr(id, ')'); rparens; (rparens = strchr(id, ')')))
		*rparens = '.';

	/* first/third/fourth captures are ignored */
	char *regex, *type_str;
	char const beg_regex[] =
			"(^[[:blank:]]*|[^,;]*[(){};[:blank:]]*)"
			"(struct[^=]+|struct|union[^=]+|union|"
			"_?[Bb]ool|[rs]?size_t|u?int[0-9]+_t|"
			"ptrdiff_t|intptr_t|intmax_t|uintmax_t|"
			"wchar_t|char[0-9]+_t|char|double|float|"
			"int|long|short|unsigned|void)[[:blank:]]+"
			"([^;]*,[^&,;=]*|[^&;]*)(";
	char const end_regex[] = ")(\\[*)";
	size_t regex_sz[2] = {
		strlen(id) + sizeof beg_regex + sizeof end_regex - 1,
		0,
	};

	/* append identifier to regex */
	xcalloc(char, &regex, 1, regex_sz[0], "extract_type()");
	strmv(0, regex, beg_regex);
	strmv(CONCAT, regex, id);
	strmv(CONCAT, regex, end_regex);

	if (regcomp(&reg, regex, REG_EXTENDED|REG_NEWLINE))
		ERR("%s", "failed to compile regex");

	/* non-zero return or -1 value in rm_so means no captures */
	if (regexec(&reg, ln, 6, matches, 0) || matches[2].rm_so == -1) {
		free(regex);
		regfree(&reg);
		return T_ERR;
	}

	/* regex capture offsets */
	size_t match_sz[2] = {
		/* from beginning of second capture to end of third capture */
		matches[3].rm_eo - matches[2].rm_so,
		/* from beginning of fifth capture to end of fifth capture */
		matches[5].rm_eo - matches[5].rm_so,
	};
	/* sum length of relevant capture lengths */
	regex_sz[1] = matches[3].rm_eo - matches[2].rm_so + matches[5].rm_eo - matches[5].rm_so + 1;

	/* allocate space for second regex */
	xcalloc(char, &type_str, 1, regex_sz[1], "extract_type()");
	/* copy matched string */
	memcpy(type_str, ln + matches[2].rm_so, match_sz[0]);
	memcpy(type_str + match_sz[0], ln + matches[5].rm_so, match_sz[1]);
	regfree(&reg);

	/* string `char[]` */
	if (regcomp(&reg, "char[[:blank:]]*[^*\\*]+\\[", REG_EXTENDED|REG_NOSUB))
		ERR("%s", "failed to compile regex");
	if (!regexec(&reg, type_str, 1, 0, 0)) {
		free(regex);
		free(type_str);
		regfree(&reg);
		return T_STR;
	}
	regfree(&reg);

	/* string */
	if (regcomp(&reg, "char[[:blank:]]*(|const)[[:blank:]]*\\*$", REG_EXTENDED|REG_NOSUB))
		ERR("%s", "failed to compile regex");
	if (!regexec(&reg, type_str, 1, 0, 0)) {
		free(regex);
		free(type_str);
		regfree(&reg);
		return T_STR;
	}
	regfree(&reg);

	/* struct/union */
	if (regcomp(&reg, "(struct|union)[^\\*\\[]+", REG_EXTENDED|REG_NOSUB))
		ERR("%s", "failed to compile regex");
	if (!regexec(&reg, type_str, 1, 0, 0)) {
		free(regex);
		free(type_str);
		regfree(&reg);
		return T_OTHER;
	}
	regfree(&reg);

	/* pointer */
	if (regcomp(&reg, "(\\*|\\[)", REG_EXTENDED|REG_NOSUB))
		ERR("%s", "failed to compile regex");
	if (!regexec(&reg, type_str, 1, 0, 0)) {
		free(regex);
		free(type_str);
		regfree(&reg);
		return T_PTR;
	}
	regfree(&reg);

	/* char */
	if (regcomp(&reg, "^char([[:blank:]]+|)$", REG_EXTENDED|REG_NOSUB))
		ERR("%s", "failed to compile regex");
	if (!regexec(&reg, type_str, 1, 0, 0)) {
		free(regex);
		free(type_str);
		regfree(&reg);
		return T_CHR;
	}
	regfree(&reg);

	/* double */
	if (regcomp(&reg, "(float|double)", REG_EXTENDED|REG_NOSUB))
		ERR("%s", "failed to compile regex");
	if (!regexec(&reg, type_str, 1, 0, 0)) {
		free(regex);
		free(type_str);
		regfree(&reg);
		return T_DBL;
	}
	regfree(&reg);

	/* unsigned integral */
	if (regcomp(&reg, "^(_?[Bb]ool|unsigned|char[0-9]+|wchar|uint|r?size)", REG_EXTENDED|REG_NOSUB))
		ERR("%s", "failed to compile regex");
	if (!regexec(&reg, type_str, 1, 0, 0)) {
		free(regex);
		free(type_str);
		regfree(&reg);
		return T_UINT;
	}
	regfree(&reg);

	/* signed integral */
	if (regcomp(&reg, "(short|int|long|ptrdiff|ssize)", REG_EXTENDED|REG_NOSUB))
		ERR("%s", "failed to compile regex");
	if (!regexec(&reg, type_str, 1, 0, 0)) {
		free(regex);
		free(type_str);
		regfree(&reg);
		return T_INT;
	}
	regfree(&reg);

	/* return fallback type */
	free(regex);
	free(type_str);
	return T_OTHER;
}

size_t extract_id(char const *restrict ln, char **restrict id, size_t *restrict off)
{
	regex_t reg;
	regmatch_t matches[5];
	/* second capture is ignored */
	char const initial_regex[] =
		"^[^,(){};&|=]+[^,({;&|=[:alnum:]_]+"
		"([[:alpha:]_][[:alnum:]_]*)[[:blank:]]*"
		"(=[^,(){;'\"_=!<>[:alnum:]]+"
		"|<>{2}=[^,({;'\"_=!<>[:alnum:]]*|[^=,;]+=)";

	/* return early if passed NULL pointers */
	if (!ln || !id || !off)
		ERRX("%s", "NULL pointer passed to extract_id()");

#ifdef _DEBUG
	if (strlen(ln))
		DPRINTF("extract_id(): \"%s\"\n", ln);
#endif

	if (regcomp(&reg, initial_regex, REG_EXTENDED|REG_NEWLINE))
		ERR("%s", "failed to compile regex");
	/* non-zero return or -1 value in rm_so means no captures */
	if (regexec(&reg, ln, 3, matches, 0) || matches[1].rm_so == -1) {
		/* fallback branch */
		regfree(&reg);
		/* first/second/fourth capture is ignored */
		char const middle_regex[] =
			"(^|^[^;,]*;+[[:blank:]]*|^[^=,(){};&|'\"]+)"
			"(struct[^=]+|struct|union[^=]+|union|"
			"_?[Bb]ool|[rs]?size_t|u?int[0-9]+_t|"
			"ptrdiff_t|intptr_t|intmax_t|uintmax_t|"
			"wchar_t|char[0-9]+_t|char|double|float|"
			"int|long|short|unsigned|void)"
			"[^=,(){};&|'\"[:alpha:]]+[[:blank:]]*\\**[[:blank:]]*"
			"([[:alpha:]_][[:alnum:]_]*)[[:blank:]]*"
			"([^=,(){};&|'\"[:alnum:][:blank:]]+$|[^;]*,|$|\\[|,)";

		if (regcomp(&reg, middle_regex, REG_EXTENDED|REG_NEWLINE))
			ERR("%s", "failed to compile regex");
		if (regexec(&reg, ln, 5, matches, 0) || matches[3].rm_so == -1) {
			regfree(&reg);
			/* first/second capture is ignored */
			char const final_regex[] =
				"(^[^,;]+\\{[^}]*\\}[^,;]*|[^,(){};|]+)"
				"(|struct[^=]+|struct|union[^=]+|union|"
				"_?[Bb]ool|[rs]?size_t|u?int[0-9]+_t|"
				"ptrdiff_t|intptr_t|intmax_t|uintmax_t|"
				"wchar_t|char[0-9]+_t|char|double|float|"
				"int|long|short|unsigned|void)"
				",[[:blank:]]*\\**[[:blank:]]*"
				"([[:alpha:]_][[:alnum:]_]*)";

			if (regcomp(&reg, final_regex, REG_EXTENDED|REG_NEWLINE))
				ERR("%s", "failed to compile regex");
			if (regexec(&reg, ln, 4, matches, 0) || matches[3].rm_so == -1) {
				regfree(&reg);
				return 0;
			}

			xcalloc(char, id, 1, matches[3].rm_eo - matches[3].rm_so + 1, "extract_id()");
			/* set the output parameter and return the offset */
			memcpy(*id, ln + matches[3].rm_so, matches[3].rm_eo - matches[3].rm_so);
			regfree(&reg);
			*off = matches[3].rm_eo;
#ifdef _DEBUG
			DPRINTF("regex [3]: %s\n", *id);
#endif
			return matches[3].rm_eo;
		}

		xcalloc(char, id, 1, matches[3].rm_eo - matches[3].rm_so + 1, "extract_id()");
		/* set the output parameter and return the offset */
		memcpy(*id, ln + matches[3].rm_so, matches[3].rm_eo - matches[3].rm_so);
		regfree(&reg);
		*off = matches[3].rm_eo;
#ifdef _DEBUG
		DPRINTF("regex [2]: %s\n", *id);
#endif
		return matches[3].rm_eo;
	}

	xcalloc(char, id, 1, matches[1].rm_eo - matches[1].rm_so + 1, "extract_id()");
	/* set the output parameter and return the offset */
	memcpy(*id, ln + matches[1].rm_so, matches[1].rm_eo - matches[1].rm_so);
	regfree(&reg);
	*off = matches[1].rm_eo;
#ifdef _DEBUG
	DPRINTF("regex [1]: %s\n", *id);
#endif
	return matches[1].rm_eo;
}

int find_vars(struct program *restrict prog, char const *restrict code)
{
	size_t off;
	char *line_tmp[2], *id_tmp = NULL;

	/* sanity checks */
	if (!prog || !code)
		return -1;
	xcalloc(char, &line_tmp[0], 1, strlen(code) + 1, "find_vars()");
	line_tmp[1] = line_tmp[0];

	/* initialize lists */
	if (prog->type_list.list)
		free(prog->type_list.list);
	if (prog->id_list.list)
		free_str_list(&prog->id_list);
	init_type_list(&prog->type_list);
	init_str_list(&prog->id_list, NULL);
	strmv(0, line_tmp[1], code);

	/* extract all identifiers from the line */
	size_t count = prog->id_list.cnt;
	while (line_tmp[1] && extract_id(line_tmp[1], &id_tmp, &off) != 0) {
		append_str(&prog->id_list, id_tmp, 0);
		free(id_tmp);
		id_tmp = NULL;
		line_tmp[1] += off;
		count++;
	}
	if (id_tmp) {
		free(id_tmp);
		id_tmp = NULL;
	}

	/* second pass */
	while (line_tmp[1] && (line_tmp[1] = strpbrk(line_tmp[1], ";"))) {
		for (line_tmp[1]++; extract_id(line_tmp[1], &id_tmp, &off); count++) {
			append_str(&prog->id_list, id_tmp, 0);
			free(id_tmp);
			id_tmp = NULL;
			line_tmp[1] += off;
		}
		if (id_tmp) {
			free(id_tmp);
			id_tmp = NULL;
		}
	}

	/* return early if nothing to do */
	if (!count) {
		free(id_tmp);
		free(line_tmp[0]);
		return 0;
	}
	if (prog->id_list.cnt < 1)
		return 0;

	/* get the type of each identifier */
	line_tmp[1] = line_tmp[0];
	for (size_t i = 0; i < prog->id_list.cnt; i++) {
		enum var_type type_tmp;
		type_tmp = extract_type(line_tmp[1], prog->id_list.list[i]);
		append_type(&prog->type_list, type_tmp);
	}
	free(id_tmp);
	free(line_tmp[0]);

	return count;
}

int print_vars(struct program *restrict prog, char *const *restrict cc_args, char **exec_args)
{
	int status, mem_fd, null_fd;
	int pipe_cc[2], pipe_ld[2], pipe_exec[2];
	char *src_tmp;
	char *term = getenv("TERM");
	bool has_color = term
		&& isatty(STDOUT_FILENO)
		&& isatty(STDERR_FILENO)
		&& strcmp(term, "")
		&& strcmp(term, "dumb");
	char *p_beg = has_color
		? "\n\tfprintf(stderr, \"" YELLOW "__s = \\\"____\\\", " RST "\", \""
		: "\n\tfprintf(stderr, \"__s = \\\"____\\\", \",\"";
	char *pln_beg = has_color
		? "\n\tfprintf(stderr, \"" YELLOW "__s = \\\"____\\\"\\n" RST "\", \""
		: "\n\tfprintf(stderr, \"__s = \\\"____\\\"\\n\", \"";
	char print_beg[strlen(p_beg) + 1], println_beg[strlen(pln_beg) + 1];
	char print_end[] = ");";
	/* space for <name>, <name> */
	size_t psz = sizeof print_beg + sizeof print_end + 16;
	size_t plnsz = sizeof println_beg + sizeof print_end + 16;

	/* inititalize arrays */
	strmv(0, print_beg, p_beg);
	strmv(0, println_beg, pln_beg);
	/* sanity checks */
	if (strlen(prog->src[1].total.buf) < 2)
		ERRX("%s", "empty source string passed to print_prog->var_list()");
	/* return early if nothing to do */
	if (!prog->src[1].total.buf || !cc_args || !exec_args || prog->var_list.cnt == 0)
		return -1;
	/* bit bucket */
	if ((null_fd = open("/dev/null", O_WRONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) == -1)
		ERR("%s", "`null_fd` open()");
	/* copy source buffer */
	xcalloc(char, &src_tmp, 1, strlen(prog->src[1].total.buf) + 1, "src_tmp realloc()");
	strmv(0, src_tmp, prog->src[1].total.buf);
	size_t off = strlen(prog->src[1].total.buf);

	/* build variable tracking source instance */
	for (size_t i = 0; i < prog->var_list.cnt; i++) {
		/* skip erroneous types */
		enum var_type cur_type = prog->var_list.list[i].type_spec;
		if (cur_type == T_ERR)
			continue;

		/* populate buffers */
		size_t printf_sz = (i < prog->var_list.cnt - 1) ? psz : plnsz;
		size_t arr_sz = (i < prog->var_list.cnt - 1) ? sizeof print_beg : sizeof println_beg;
		size_t cur_sz = strlen(prog->var_list.list[i].id) * 2;
		char print_tmp[arr_sz];

		/* add newline on last iteration */
		if (i < prog->var_list.cnt - 1)
			strmv(0, print_tmp, print_beg);
		else
			strmv(0, print_tmp, println_beg);
		xrealloc(char, &src_tmp, strlen(src_tmp) + cur_sz + printf_sz, "src_tmp realloc()");

		/* build format string */
		switch (cur_type) {
		case T_ERR:
			/* should never hit this branch */
			ERR("%s", prog->var_list.list[i].id);
			break;
		case T_CHR:
			strchrnul(print_tmp, '_')[0] = '%';
			strchrnul(print_tmp, '_')[0] = '1';
			strchrnul(print_tmp, '_')[0] = '%';
			strchrnul(print_tmp, '_')[0] = '-';
			strchrnul(print_tmp, '_')[0] = '1';
			strchrnul(print_tmp, '_')[0] = 'c';
			break;
		case T_STR:
			strchrnul(print_tmp, '_')[0] = '%';
			strchrnul(print_tmp, '_')[0] = '1';
			strchrnul(print_tmp, '_')[0] = '%';
			strchrnul(print_tmp, '_')[0] = '-';
			strchrnul(print_tmp, '_')[0] = '1';
			strchrnul(print_tmp, '_')[0] = 's';
			break;
		case T_INT:
			strchrnul(print_tmp, '_')[0] = '%';
			strchrnul(print_tmp, '_')[0] = '1';
			strchrnul(print_tmp, '_')[0] = '%';
			strchrnul(print_tmp, '_')[0] = 'l';
			strchrnul(print_tmp, '_')[0] = 'l';
			strchrnul(print_tmp, '_')[0] = 'd';
			break;
		case T_UINT:
			strchrnul(print_tmp, '_')[0] = '%';
			strchrnul(print_tmp, '_')[0] = '1';
			strchrnul(print_tmp, '_')[0] = '%';
			strchrnul(print_tmp, '_')[0] = 'l';
			strchrnul(print_tmp, '_')[0] = 'l';
			strchrnul(print_tmp, '_')[0] = 'u';
			break;
		case T_DBL:
			strchrnul(print_tmp, '_')[0] = '%';
			strchrnul(print_tmp, '_')[0] = '1';
			strchrnul(print_tmp, '_')[0] = '%';
			strchrnul(print_tmp, '_')[0] = '1';
			strchrnul(print_tmp, '_')[0] = 'L';
			strchrnul(print_tmp, '_')[0] = 'f';
			break;
		case T_PTR:
			strchrnul(print_tmp, '_')[0] = '*';
			strchrnul(print_tmp, '_')[0] = '%';
			strchrnul(print_tmp, '_')[0] = '%';
			strchrnul(print_tmp, '_')[0] = '-';
			strchrnul(print_tmp, '_')[0] = '-';
			strchrnul(print_tmp, '_')[0] = 'p';
			break;
		case T_OTHER: /* fallthrough */
		default:
			strchrnul(print_tmp, '_')[0] = '&';
			strchrnul(print_tmp, '_')[0] = '%';
			strchrnul(print_tmp, '_')[0] = '%';
			strchrnul(print_tmp, '_')[0] = '-';
			strchrnul(print_tmp, '_')[0] = '1';
			strchrnul(print_tmp, '_')[0] = 'p';
		}

		/* copy format string */
		strmv(off, src_tmp, print_tmp);
		off += arr_sz - 1;
		strmv(off, src_tmp, prog->var_list.list[i].id);
		off += strlen(prog->var_list.list[i].id);

		/* handle other variable types */
		char const *tmp_buf;
		switch (cur_type) {
		case T_ERR:
			/* should never hit this branch */
			ERR("%s", prog->var_list.list[i].id);
			break;
		case T_INT:
			/* cast integral type to long long */
			tmp_buf = "\", (long long)";
			strmv(off, src_tmp, tmp_buf);
			off += strlen(tmp_buf);
			break;
		case T_DBL:
			/* cast floating type to long double */
			tmp_buf = "\", (long double)";
			strmv(off, src_tmp, tmp_buf);
			off += strlen(tmp_buf);
			break;
		case T_OTHER:
			/* take the address of variable if type unknown */
			tmp_buf = "\", &";
			strmv(off, src_tmp, tmp_buf);
			off += strlen(tmp_buf);
			break;
		case T_CHR: /* fallthrough */
		case T_STR: /* fallthrough */
		case T_UINT: /* fallthrough */
		case T_PTR: /* fallthrough */
		default:
			tmp_buf = "\", ";
			strmv(off, src_tmp, tmp_buf);
			off += strlen(tmp_buf);
		}

		/* copy final part of printf */
		strmv(off, src_tmp, prog->var_list.list[i].id);
		off += strlen(prog->var_list.list[i].id);
		strmv(off, src_tmp, print_end);
		off += sizeof print_end - 1;
	}

	/* copy final source into buffer */
	char final[off + strlen(prog_end) + 1], *tok_buf;
	memset(final, 0, sizeof final);
	strmv(0, final, src_tmp);
	strmv(off, final, prog_end);
	/* remove NULL bytes */
	while ((tok_buf = memchr(final, 0, sizeof final)))
		tok_buf[0] = '\n';
	free(src_tmp);

	/* create pipes */
	if (pipe(pipe_cc) == -1)
		ERR("%s", "error making pipe_cc pipe");
	if (pipe(pipe_ld) == -1)
		ERR("%s", "error making pipe_ld pipe");
	if (pipe(pipe_exec) == -1)
		ERR("%s", "error making pipe_exec pipe");
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
		ERR("%s", "error forking compiler");
		break;

	/* child */
	case 0:
		dup2(null_fd, STDERR_FILENO);
		dup2(pipe_ld[1], STDOUT_FILENO);
		dup2(pipe_cc[0], STDIN_FILENO);
		execvp(cc_args[0], cc_args);
		/* execvp() should never return */
		ERR("%s", "error forking compiler");
		break;

	/* parent */
	default:
		close(pipe_cc[0]);
		close(pipe_ld[1]);
		if (write(pipe_cc[1], final, sizeof final) == -1)
			ERR("%s", "error writing to pipe_cc[1]");
		close(pipe_cc[1]);
		wait(&status);
		/* convert 255 to -1 since WEXITSTATUS() only returns the low-order 8 bits */
		if (WIFEXITED(status) && WEXITSTATUS(status)) {
			/* WARNX("compiler returned non-zero exit code"); */
			return (WEXITSTATUS(status) != 0xff) ? WEXITSTATUS(status) : -1;
		}
	}

	/* fork linker */
	switch (fork()) {
	/* error */
	case -1:
		close(pipe_ld[0]);
		close(pipe_exec[0]);
		close(pipe_exec[1]);
		ERR("%s", "error forking linker");
		break;

	/* child */
	case 0:
		dup2(null_fd, STDERR_FILENO);
		dup2(pipe_exec[1], STDOUT_FILENO);
		dup2(pipe_ld[0], STDIN_FILENO);
		if (ld_list.list)
			execvp(ld_list.list[0], ld_list.list);
		/* fallback linker exec */
		execvp(ld_alt_list[0], ld_alt_list);
		/* execvp() should never return */
		ERR("%s", "error forking linker");
		break;

	/* parent */
	default:
		close(pipe_ld[0]);
		close(pipe_exec[1]);
		wait(&status);
		/* convert 255 to -1 since WEXITSTATUS() only returns the low-order 8 bits */
		if (WIFEXITED(status) && WEXITSTATUS(status)) {
			/* WARNX("linker returned non-zero exit code"); */
			return (WEXITSTATUS(status) != 0xff) ? WEXITSTATUS(status) : -1;
		}
	}

	/* fork executable */
	switch (fork()) {
	/* error */
	case -1:
		close(pipe_exec[0]);
		ERR("%s", "error forking executable");
		break;

	/* child */
	case 0:
		reset_handlers();
		if ((mem_fd = syscall(SYS_memfd_create, "cepl_memfd", MFD_CLOEXEC)) == -1)
			ERR("%s", "error creating mem_fd");
		pipe_fd(pipe_exec[0], mem_fd);
		/* redirect stdout/stdin to /dev/null */
		if (!(null_fd = open("/dev/null", O_WRONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)))
			ERR("%s", "open()");
		dup2(null_fd, STDIN_FILENO);
		dup2(null_fd, STDOUT_FILENO);
		fexecve(mem_fd, exec_args, environ);
		/* fexecve() should never return */
		ERR("%s", "error forking executable");
		break;

	/* parent */
	default:
		close(pipe_exec[0]);
		close(null_fd);
		wait(&status);
		/* convert 255 to -1 since WEXITSTATUS() only returns the low-order 8 bits */
		if (WIFEXITED(status) && WEXITSTATUS(status)) {
			/* WARNX("executable returned non-zero exit code"); */
			return (WEXITSTATUS(status) != 0xff) ? WEXITSTATUS(status) : -1;
		}
	}

	return 0;
}
