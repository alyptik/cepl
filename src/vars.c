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
long syscall(long __sysno, ...);
int fexecve(int __fd, char *const __argv[], char *const __envp[]);
void *mmap(void *__addr, size_t __len, int __prot, int __flags, int __fd, off_t __offset);

enum var_type extract_type(char const *ln, char const *id)
{
	regex_t reg;
	regmatch_t match[7];
	/* return early if passed NULL pointers */
	if (!ln || !id)
		ERRX("NULL pointer passed to extract_type()");

	/* first/third/fourth captures are ignored */
	char *regex, *type;
	char const beg_regex[] =
			"(^|.*[({;[:blank:]]*)"
			"(struct|union|bool|_Bool|s?size_t|u?int[0-9]|ptrdiff_t|"
			"char|double|float|int|long|short|unsigned|void)[[:blank:]]+"
			"([^&;=]*)(";
	char const end_regex[] = ")(\\[*)";

	/* append identifier to regex */
	if (!(regex = calloc(1, strlen(id) + sizeof beg_regex + sizeof end_regex - 1)))
		ERR("failed to allocate space for regex");
	strmv(0, regex, beg_regex);
	strmv(CONCAT, regex, id);
	strmv(CONCAT, regex, end_regex);

	if (regcomp(&reg, regex, REG_EXTENDED|REG_NEWLINE))
		ERR("failed to compile regex");

	/* non-zero return or -1 value in rm_so means no captures */
	if (regexec(&reg, ln, 6, match, 0) || match[2].rm_so == -1) {
		free(regex);
		regfree(&reg);
		return T_ERR;
	}

	/* allocate space for second regex */
	if (!(type = calloc(1, match[3].rm_eo - match[2].rm_so + match[5].rm_eo - match[5].rm_so + 1)))
		ERR("failed to allocate space for captured type");
	/* copy matched string */
	memcpy(type, ln + match[2].rm_so, match[3].rm_eo - match[2].rm_so);
	memcpy(type + match[3].rm_eo - match[2].rm_so, ln + match[5].rm_so, match[5].rm_eo - match[5].rm_so);
	regfree(&reg);

	/* string `char[]` */
	if (regcomp(&reg, "char[[:blank:]]*[^*\\*]+\\[", REG_EXTENDED|REG_NOSUB|REG_NEWLINE))
		ERR("failed to compile regex");
	if (!regexec(&reg, type, 1, 0, 0)) {
		free(regex);
		free(type);
		regfree(&reg);
		return T_STR;
	}
	regfree(&reg);

	/* string */
	if (regcomp(&reg, "char[[:blank:]]*(|const)[[:blank:]]*\\*$", REG_EXTENDED|REG_NOSUB|REG_NEWLINE))
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
	if (regcomp(&reg, "(_?[Bb]ool|size_t|uint[0-9]+_t|unsigned)", REG_EXTENDED|REG_NOSUB|REG_NEWLINE))
		ERR("failed to compile regex");
	if (!regexec(&reg, type, 1, 0, 0)) {
		free(regex);
		free(type);
		regfree(&reg);
		return T_UINT;
	}
	regfree(&reg);

	/* signed integral */
	if (regcomp(&reg, "(short|int|long|int[0-9]+_t|ptrdiff_t|ssize_t)", REG_EXTENDED|REG_NOSUB|REG_NEWLINE))
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

size_t extract_id(char const *ln, char **id, size_t *offset)
{
	regex_t reg;
	regmatch_t match[4];
	/* second capture is ignored */
	char const initial_regex[] =
		"^[^,({;&|=]+[^,({;&|=[:alnum:]_]+"
		"([[:alpha:]_][[:alnum:]_]*)[[:blank:]]*"
		"(=[^,({;'\"_=!<>[:alnum:]]+|<>{2}=[^,({;'\"_=!<>[:alnum:]]*|[^=]+=)";

	/* return early if passed NULL pointers */
	if (!ln || !id || !offset)
		ERRX("NULL pointer passed to extract_id()");

	if (regcomp(&reg, initial_regex, REG_EXTENDED|REG_NEWLINE))
		ERR("failed to compile regex");
	/* non-zero return or -1 value in rm_so means no captures */
	if (regexec(&reg, ln, 3, match, 0) || match[1].rm_so == -1) {
		/* fallback branch */
		regfree(&reg);
		/* first/second/fourth capture is ignored */
		char const middle_regex[] =
			"(^[[:blank:]]*|^[^,({;&|'\"]+)"
			"(struct|union|bool|_Bool|s?size_t|u?int[0-9]|ptrdiff_t|"
			"char|double|float|int|long|short|unsigned|void)"
			"[^,(){};&|'\"[:alpha:]]+[[:blank:]]*\\**[[:blank:]]*"
			"([[:alpha:]_][[:alnum:]_]*)[[:blank:]]*"
			"([^(){};&|'\"[:alnum:][:blank:]]+$|;*$|\\[|,)";

		if (regcomp(&reg, middle_regex, REG_EXTENDED|REG_NEWLINE))
			ERR("failed to compile regex");
		if (regexec(&reg, ln, 5, match, 0) || match[3].rm_so == -1) {
			regfree(&reg);
			/* first/second/fourth capture is ignored */
			char const final_regex[] =
				"(^|[^,({;|]+)"
				"(|struct|union|bool|_Bool|s?size_t|u?int[0-9]|ptrdiff_t|"
				"char|double|float|int|long|short|unsigned|void)"
				",[[:blank:]]*\\**[[:blank:]]*"
				"([[:alpha:]_][[:alnum:]_]*)";

			if (regcomp(&reg, final_regex, REG_EXTENDED|REG_NEWLINE))
				ERR("failed to compile regex");
			if (regexec(&reg, ln, 4, match, 0) || match[3].rm_so == -1) {
				regfree(&reg);
				return 0;
			}

			/* puts("match three"); */
			if (!(*id = calloc(1, match[3].rm_eo - match[3].rm_so + 1)))
				ERR("failed to allocate space for captured id");
			/* set the output parameter and return the offset */
			memcpy(*id, ln + match[3].rm_so, match[3].rm_eo - match[3].rm_so);
			regfree(&reg);
			*offset = match[3].rm_eo;
			return match[3].rm_eo;
		}

		/* puts("match two"); */
		if (!(*id = calloc(1, match[3].rm_eo - match[3].rm_so + 1)))
			ERR("failed to allocate space for captured id");
		/* set the output parameter and return the offset */
		memcpy(*id, ln + match[3].rm_so, match[3].rm_eo - match[3].rm_so);
		regfree(&reg);
		*offset = match[3].rm_eo;
		return match[3].rm_eo;
	}

	/* puts("match one"); */
	/* normal branch */
	if (!(*id = calloc(1, match[1].rm_eo - match[1].rm_so + 1)))
		ERR("failed to allocate space for captured id");
	/* set the output parameter and return the offset */
	memcpy(*id, ln + match[1].rm_so, match[1].rm_eo - match[1].rm_so);
	regfree(&reg);
	*offset = match[1].rm_eo;
	return match[1].rm_eo;
}

int find_vars(char const *ln, struct str_list *ilist, struct type_list *tlist)
{
	size_t off;
	char *line_tmp[2], *id_tmp = NULL;

	/* sanity checks */
	if (!ln || !ilist || !tlist)
		return 0;
	if (!(line_tmp[0] = calloc(1, strlen(ln) + 1)))
		ERR("error allocating line_tmp");
	line_tmp[1] = line_tmp[0];

	/* initialize lists */
	if (tlist->list)
		free(tlist->list);
	if (ilist->list)
		free_str_list(ilist);
	init_tlist(tlist);
	init_list(ilist, NULL);

	size_t count = ilist->cnt;
	strmv(0, line_tmp[1], ln);
	/* extract all identifiers from the line */
	while (line_tmp[1] && extract_id(line_tmp[1], &id_tmp, &off) != 0) {
		append_str(ilist, id_tmp, 0);
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
	while (line_tmp[1] && (line_tmp[1] = strpbrk(line_tmp[1], ";"))) {
		line_tmp[1]++;
		while (extract_id(line_tmp[1], &id_tmp, &off)) {
			append_str(ilist, id_tmp, 0);
			free(id_tmp);
			id_tmp = NULL;
			line_tmp[1] += off;
			count++;
		}
		if (id_tmp) {
			free(id_tmp);
			id_tmp = NULL;
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
	/* if no keys found return early */
	if (ilist->cnt < 1)
		return 0;
	/* copy it into the output parameter */
	for (size_t i = 0; i < ilist->cnt; i++) {
		enum var_type type_tmp;
		type_tmp = extract_type(line_tmp[1], ilist->list[i]);
		append_type(tlist, type_tmp);
	}
	if (id_tmp)
		free(id_tmp);
	if (line_tmp[0])
		free(line_tmp[0]);
	return ilist->cnt;
}

int print_vars(struct var_list *vlist, char const *src, char *const cc_args[], char *const exec_args[])
{
	char *term = getenv("TERM");
	bool has_color = true;
	/* toggle flag to `false` if `TERM` is NULL, empty, or matches `dumb` */
	if (!term || !strcmp(term, "") || !strcmp(term, "dumb"))
		has_color = false;

	int mem_fd, status, null;
	int pipe_cc[2], pipe_ld[2], pipe_exec[2];
	char newline[] = "\n\tfprintf(stderr, \"\\n\");";
	char *p_beg = (has_color && isatty(STDIN_FILENO)) ?
		"\n\tfprintf(stderr, \"" YELLOW "__s = \\\"____\\\", " RST " \", \"" :
		"\n\tfprintf(stderr, \"__s = \\\"____\\\", \", \"";
	char *pln_beg = (has_color && isatty(STDIN_FILENO)) ?
		"\n\tfprintf(stderr, \"" YELLOW "__s = \\\"____\\\"\\n " RST " \", \"" :
		"\n\tfprintf(stderr, \"__s = \\\"____\\\"\\n \", \"";
	char print_beg[strlen(p_beg) + 1], println_beg[strlen(pln_beg) + 1];
	char print_end[] = ");";
	char *src_tmp, *tmp_ptr;
	size_t off;
	/* space for <name>, <name> */
	size_t psz = sizeof print_beg + sizeof print_end + 16;
	size_t plnsz = sizeof println_beg + sizeof print_end + 16;

	/* inititalize arrays */
	strmv(0, print_beg, p_beg);
	strmv(0, println_beg, pln_beg);

	/* sanity checks */
	if (!vlist)
		ERRX("NULL `var_list *` passed to print_vlist()");
	if (strlen(src) < 2)
		ERRX("empty source string passed to print_vlist()");
	/* return early if nothing to do */
	if (!src || !cc_args || !exec_args || vlist->cnt == 0)
		return -1;
	/* bit bucket */
	if (!(null = open("/dev/null", O_WRONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)))
		ERR("open()");

	/* copy source buffer */
	if (!(src_tmp = calloc(1, strlen(src) + 1)))
		ERR("src_tmp calloc()");
	strmv(0, src_tmp, src);
	off = strlen(src);
	if (!(tmp_ptr = realloc(src_tmp, strlen(src_tmp) + sizeof newline))) {
		free(src_tmp);
		ERR("src_tmp realloc()");
	}
	src_tmp = tmp_ptr;
	strmv(off, src_tmp, newline);
	off += sizeof newline - 1;

	/* build var-tracking source */
	for (size_t i = 0; i < vlist->cnt; i++) {
		enum var_type cur_type = vlist->list[i].type;
		/* skip erroneous types */
		if (cur_type == T_ERR)
			continue;
		/* populate buffers */
		size_t printf_sz = (i < vlist->cnt - 1) ? psz : plnsz;
		size_t arr_sz = (i < vlist->cnt - 1) ? sizeof print_beg : sizeof println_beg;
		size_t cur_sz = strlen(vlist->list[i].key) * 2;
		char (*arr_ptr)[printf_sz] = (i < vlist->cnt - 1) ? &print_beg : &println_beg;

		if (!(tmp_ptr = realloc(src_tmp, strlen(src_tmp) + cur_sz + printf_sz))) {
			free(src_tmp);
			ERR("src_tmp realloc()");
		}
		src_tmp = tmp_ptr;
		char print_tmp[arr_sz];
		strmv(0, print_tmp, *arr_ptr);

		/* build format string */
		switch (cur_type) {
		case T_ERR:
			/* should never hit this branch */
			ERR(vlist->list[i].key);
			break;
		case T_CHR:
			strchr(print_tmp, '_')[0] = '%';
			strchr(print_tmp, '_')[0] = '0';
			strchr(print_tmp, '_')[0] = '%';
			strchr(print_tmp, '_')[0] = '0';
			strchr(print_tmp, '_')[0] = '0';
			strchr(print_tmp, '_')[0] = 'c';
			break;
		case T_STR:
			strchr(print_tmp, '_')[0] = '%';
			strchr(print_tmp, '_')[0] = '0';
			strchr(print_tmp, '_')[0] = '%';
			strchr(print_tmp, '_')[0] = '0';
			strchr(print_tmp, '_')[0] = '0';
			strchr(print_tmp, '_')[0] = 's';
			break;
		case T_INT:
			strchr(print_tmp, '_')[0] = '%';
			strchr(print_tmp, '_')[0] = '0';
			strchr(print_tmp, '_')[0] = '%';
			strchr(print_tmp, '_')[0] = 'l';
			strchr(print_tmp, '_')[0] = 'l';
			strchr(print_tmp, '_')[0] = 'd';
			break;
		case T_UINT:
			strchr(print_tmp, '_')[0] = '%';
			strchr(print_tmp, '_')[0] = '0';
			strchr(print_tmp, '_')[0] = '%';
			strchr(print_tmp, '_')[0] = 'l';
			strchr(print_tmp, '_')[0] = 'l';
			strchr(print_tmp, '_')[0] = 'u';
			break;
		case T_DBL:
			strchr(print_tmp, '_')[0] = '%';
			strchr(print_tmp, '_')[0] = '0';
			strchr(print_tmp, '_')[0] = '%';
			strchr(print_tmp, '_')[0] = '0';
			strchr(print_tmp, '_')[0] = 'L';
			strchr(print_tmp, '_')[0] = 'f';
			break;
		case T_PTR:
			strchr(print_tmp, '_')[0] = '*';
			strchr(print_tmp, '_')[0] = '%';
			strchr(print_tmp, '_')[0] = '%';
			strchr(print_tmp, '_')[0] = '0';
			strchr(print_tmp, '_')[0] = '0';
			strchr(print_tmp, '_')[0] = 'p';
			break;
		case T_OTHER: /* fallthrough */
		default:
			strchr(print_tmp, '_')[0] = '&';
			strchr(print_tmp, '_')[0] = '%';
			strchr(print_tmp, '_')[0] = '%';
			strchr(print_tmp, '_')[0] = '0';
			strchr(print_tmp, '_')[0] = '0';
			strchr(print_tmp, '_')[0] = 'p';
		}

		/* copy format string */
		strmv(off, src_tmp, print_tmp);
		off += arr_sz - 1;
		strmv(off, src_tmp, vlist->list[i].key);
		off += strlen(vlist->list[i].key);

		/* handle other variable types */
		switch (cur_type) {
		case T_ERR:
			/* should never hit this branch */
			ERR(vlist->list[i].key);
			break;
		case T_INT:
			/* cast integral type to long long */
			strmv(off, src_tmp, "\", (long long)");
			off += 14;
			break;
		case T_DBL:
			/* cast floating type to long double */
			strmv(off, src_tmp, "\", (long double)");
			off += 16;
			break;
		case T_OTHER:
			/* take the address of variable if type unknown */
			strmv(off, src_tmp, "\", &");
			off += 4;
			break;
		case T_CHR: /* fallthrough */
		case T_STR: /* fallthrough */
		case T_UINT: /* fallthrough */
		case T_PTR: /* fallthrough */
		default:
			strmv(off, src_tmp, "\", ");
			off += 3;
		}

		/* copy final part of printf */
		strmv(off, src_tmp, vlist->list[i].key);
		off += strlen(vlist->list[i].key);
		strmv(off, src_tmp, print_end);
		off += sizeof print_end - 1;
	}

	/* copy final source into buffer */
	char final[off + sizeof prog_end + 1], *tok_buf;
	memset(final, 0, sizeof final);
	strmv(0, final, src_tmp);
	strmv(off, final, prog_end);
	/* remove NULL bytes */
	while ((tok_buf = memchr(final, 0, sizeof final)))
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
		/* redirect stderr to /dev/null */
		dup2(null, STDERR_FILENO);
		dup2(pipe_ld[1], STDOUT_FILENO);
		dup2(pipe_cc[0], STDIN_FILENO);
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
		ERR("error forking linker");
		break;

	/* child */
	case 0:
		/* redirect stderr to /dev/null */
		dup2(null, STDERR_FILENO);
		dup2(pipe_exec[1], STDOUT_FILENO);
		dup2(pipe_ld[0], STDIN_FILENO);
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
		ERR("error forking executable");
		break;

	/* child */
	case 0:
		if ((mem_fd = syscall(SYS_memfd_create, "cepl_memfd", MFD_CLOEXEC)) == -1)
			ERR("error creating mem_fd");
		pipe_fd(pipe_exec[0], mem_fd);
		/* redirect stdout/stdin to /dev/null */
		if (!(null = open("/dev/null", O_WRONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)))
			ERR("open()");
		dup2(null, STDIN_FILENO);
		dup2(null, STDOUT_FILENO);
		fexecve(mem_fd, exec_args, environ);
		/* fexecve() should never return */
		ERR("error forking executable");
		break;

	/* parent */
	default:
		close(pipe_exec[0]);
		close(null);
		wait(&status);
		/* convert 255 to -1 since WEXITSTATUS() only returns the low-order 8 bits */
		if (WIFEXITED(status) && WEXITSTATUS(status)) {
			/* WARNX("executable returned non-zero exit code"); */
			return (WEXITSTATUS(status) != 0xff) ? WEXITSTATUS(status) : -1;
		}
	}

	return 0;
}
