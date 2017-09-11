/*
 * cepl.c - REPL translation unit
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

/* silence linter */
#ifndef _GNU_SOURCE
#	define _GNU_SOURCE
#endif
/* TODO: change history filename to a non-hardcoded string */
#define HIST_NAME "./.cepl_history"

#include <sys/stat.h>
#include <sys/types.h>
#include "compile.h"
#include "defs.h"
#include "errors.h"
#include "parseopts.h"
#include "readline.h"
#include "vars.h"

/* source file includes template */
static char const prelude[] = "#define _BSD_SOURCE\n"
	"#define _DEFAULT_SOURCE\n"
	"#define _GNU_SOURCE\n"
	"#define _POSIX_C_SOURCE 200809L\n"
	"#define _SVID_SOURCE\n"
	"#define _XOPEN_SOURCE 700\n\n"
	"#include <assert.h>\n"
	"#include <ctype.h>\n"
	"#include <err.h>\n"
	"#include <errno.h>\n"
	"#include <fcntl.h>\n"
	"#include <limits.h>\n"
	"#include <math.h>\n"
	"#include <regex.h>\n"
	"#include <signal.h>\n"
	"#include <stdalign.h>\n"
	"#include <stdarg.h>\n"
	"#include <stdbool.h>\n"
	"#include <stddef.h>\n"
	"#include <stdint.h>\n"
	"#include <stdio.h>\n"
	"#include <stdlib.h>\n"
	"#include <stdnoreturn.h>\n"
	"#include <string.h>\n"
	"#include <strings.h>\n"
	"#include <time.h>\n"
	"#include <uchar.h>\n"
	"#include <wchar.h>\n"
	"#include <unistd.h>\n"
	"#include <linux/memfd.h>\n"
	"#include <sys/mman.h>\n"
	"#include <sys/types.h>\n"
	"#include <sys/syscall.h>\n"
	"#include <sys/wait.h>\n\n"
	"extern char **environ;\n"
	"#line 1\n";
/* compiler pre-program */
static char const prog_start[] = "\n\nint main(int argc, char *argv[]) "
	"{(void)argc; (void)argv;\n"
	"\n";
/* pre-program shown to user */
static char const prog_start_user[] = "\n\nint main(int argc, char *argv[])\n"
	"{\n";
static char const prog_end[] = "\n\treturn 0;\n}\n";
/* line and token buffers */
static char *line, *tok_buf;
/* compiler arg array */
static char **cc_argv;
/* readline history variables */
static char *hist_file;
/* output file */
static FILE volatile *ofile;
/* struct definition for generated program sources */
static struct prog_src user, actual;
/* var list */
static struct var_list vars;
static struct str_list ids;
static enum var_type *types;
static bool has_hist = false;
/* completion list of generated symbols */
extern struct str_list comp_list;
/* toggle flag for warnings and completions */
extern bool warn_flag, parse_flag, track_flag, out_flag;

static inline void write_file(void) {
	/* return early if no file open */
	if (!ofile)
		return;
	/* write out program to file */
	FILE *output = (FILE *)ofile;
	fwrite(actual.final, strlen(actual.final), 1, output);
	fputc('\n', output);
	fflush(output);
	fclose(output);
	ofile = NULL;
}

static inline void free_buffers(void)
{
	write_file();

	if (line)
		free(line);
	if (user.funcs)
		free(user.funcs);
	if (actual.funcs)
		free(actual.funcs);
	if (user.body)
		free(user.body);
	if (actual.body)
		free(actual.body);
	if (user.final)
		free(user.final);
	if (actual.final)
		free(actual.final);
	if (user.flags.list)
		free(user.flags.list);
	if (actual.flags.list)
		free(actual.flags.list);
	if (types)
		free(types);

	/* free vectors */
	if (cc_argv)
		free_argv(cc_argv);
	if (vars.list) {
		for (size_t i = 0; i < vars.cnt; i++) {
			if (vars.list[i].key)
				free(vars.list[i].key);
		}
		free(vars.list);
	}
	free_str_list(&user.hist);
	free_str_list(&actual.hist);
	free_str_list(&user.lines);
	free_str_list(&actual.lines);
	free_str_list(&ids);

	/* set pointers to NULL */
	user.funcs = NULL;
	user.body = NULL;
	user.final = NULL;
	user.flags.list = NULL;
	actual.funcs = NULL;
	actual.body = NULL;
	actual.final = NULL;
	actual.flags.list = NULL;
	vars.list = NULL;
	cc_argv = NULL;
	types = NULL;
	line = NULL;
}

static inline void cleanup(void)
{
	/* free generated completions */
	free_str_list(&comp_list);
	/* append history to history file */
	if (has_hist) {
		if (write_history(hist_file)) {
			char hist_pre[] = "error writing history to ";
			char hist_full[sizeof hist_pre + strlen(hist_file)];
			char *hist_ptr = hist_full;
			memcpy(hist_ptr, hist_pre, sizeof hist_pre - 1);
			memcpy(hist_ptr + sizeof hist_pre - 1, hist_file, strlen(hist_file) + 1);
			WARN(hist_ptr);
		}
	}
	if (hist_file) {
		free(hist_file);
		hist_file = NULL;
	}
	if (isatty(STDIN_FILENO))
		printf("\n%s\n\n", "Terminating program.");
}

static inline void init_buffers(void)
{
	/* user is truncated source for display */
	user.funcs = calloc(1, 1);
	/* actual is source passed to compiler */
	actual.funcs = calloc(1, strlen(prelude) + 1);
	user.body = calloc(1, strlen(prog_start_user) + 1);
	actual.body = calloc(1, strlen(prog_start) + 1);
	user.final = calloc(1, strlen(prelude) + strlen(prog_start_user) + strlen(prog_end) + 3);
	actual.final = calloc(1, strlen(prelude) + strlen(prog_start) + strlen(prog_end) + 3);
	/* sanity check */
	if (!user.funcs || !actual.funcs || !user.body || !actual.body || !user.final || !actual.final) {
		free_buffers();
		cleanup();
		ERR("initial pointer allocation");
	}
	/* no memcpy for user.funcs */
	memcpy(actual.funcs, prelude, strlen(prelude) + 1);
	memcpy(user.body, prog_start_user, strlen(prog_start_user) + 1);
	memcpy(actual.body, prog_start, strlen(prog_start) + 1);
	/* init source history and flag lists */
	init_list(&user.lines, "FOOBARTHISVALUEDOESNTMATTERTROLLOLOLOL");
	init_list(&actual.lines, "FOOBARTHISVALUEDOESNTMATTERTROLLOLOLOL");
	init_list(&user.hist, "FOOBARTHISVALUEDOESNTMATTERTROLLOLOLOL");
	init_list(&actual.hist, "FOOBARTHISVALUEDOESNTMATTERTROLLOLOLOL");
	init_flag_list(&user.flags);
	init_flag_list(&actual.flags);
	init_var_list(&vars);
}

static inline void resize_buffer(char **buf, size_t offset)
{
	/* sanity check */
	if (!buf)
		return;
	char *tmp;
	size_t alloc_sz = strlen(*buf) + strlen(line) + offset + 1;
	/* current length + line length + extra characters + \0 */
	if ((tmp = realloc(*buf, alloc_sz * 2)) == NULL) {
		free_buffers();
		cleanup();
		ERRGEN("resize_buffer()");
	}
	*buf = tmp;
}

static inline void build_funcs(void)
{
	append_str(&user.lines, tok_buf, 0);
	append_str(&actual.lines, tok_buf, 0);
	append_str(&user.hist, user.funcs, 0);
	append_str(&actual.hist, actual.funcs, 0);
	append_flag(&user.flags, NOT_IN_MAIN);
	append_flag(&actual.flags, NOT_IN_MAIN);
	/* generate function buffers */
	strcat(user.funcs, tok_buf);
	strcat(actual.funcs, tok_buf);
	strcat(user.funcs, "\n");
	strcat(actual.funcs, "\n");
}

static inline void build_body(void)
{
	append_str(&user.lines, line, 0);
	append_str(&actual.lines, line, 0);
	append_str(&user.hist, user.body, 0);
	append_str(&actual.hist, actual.body, 0);
	append_flag(&user.flags, IN_MAIN);
	append_flag(&actual.flags, IN_MAIN);
	strcat(user.body, "\t");
	strcat(actual.body, "\t");
	strcat(user.body, line);
	strcat(actual.body, line);
}

static inline void build_final(char *argv[])
{
	/* finish building current iteration of source code */
	memcpy(user.final, user.funcs, strlen(user.funcs) + 1);
	memcpy(actual.final, actual.funcs, strlen(actual.funcs) + 1);
	strcat(user.final, user.body);
	strcat(actual.final, actual.body);
	/* print variable values */
	if (!track_flag)
		print_vars(&vars, actual.final, cc_argv, argv);
	strcat(user.final, prog_end);
	strcat(actual.final, prog_end);
}

static inline void pop_history(struct prog_src *prog)
{
	switch(prog->flags.list[prog->flags.cnt - 1]) {
	case NOT_IN_MAIN:
		prog->flags.cnt--;
		prog->hist.cnt--;
		prog->lines.cnt--;
		memcpy(prog->funcs, prog->hist.list[prog->hist.cnt], strlen(prog->hist.list[prog->hist.cnt]) + 1);
		free(prog->hist.list[prog->hist.cnt]);
		free(prog->lines.list[prog->lines.cnt]);
		prog->hist.list[prog->hist.cnt] = NULL;
		prog->lines.list[prog->lines.cnt] = NULL;
		break;
	case IN_MAIN:
		prog->flags.cnt--;
		prog->hist.cnt--;
		prog->lines.cnt--;
		memcpy(prog->body, prog->hist.list[prog->hist.cnt], strlen(prog->hist.list[prog->hist.cnt]) + 1);
		free(prog->hist.list[prog->hist.cnt]);
		free(prog->lines.list[prog->lines.cnt]);
		prog->hist.list[prog->hist.cnt] = NULL;
		prog->lines.list[prog->lines.cnt] = NULL;
		break;
	case EMPTY: /* fallthrough */
	default:; /* noop */
	}
}

static inline void sig_handler(int sig)
{
	free_buffers();
	cleanup();
	exit(sig);
}

/* signal handlers to make sure that history is written and cleanup is done */
static inline void reg_handlers(void)
{
	if (signal(SIGHUP, &sig_handler) == SIG_ERR)
		WARN("unable to register SIGHUP handler");
	if (signal(SIGINT, &sig_handler) == SIG_ERR)
		WARN("unable to register SIGINT handler");
	if (signal(SIGQUIT, &sig_handler) == SIG_ERR)
		WARN("unable to register SIGQUIT handler");
	if (signal(SIGILL, &sig_handler) == SIG_ERR)
		WARN("unable to register SIGILL handler");
	if (signal(SIGABRT, &sig_handler) == SIG_ERR)
		WARN("unable to register SIGABRT handler");
	if (signal(SIGFPE, &sig_handler) == SIG_ERR)
		WARN("unable to register SIGFPE handler");
	if (signal(SIGSEGV, &sig_handler) == SIG_ERR)
		WARN("unable to register SIGSEGV handler");
	if (signal(SIGPIPE, &sig_handler) == SIG_ERR)
		WARN("unable to register SIGPIPE handler");
	if (signal(SIGALRM, &sig_handler) == SIG_ERR)
		WARN("unable to register SIGALRM handler");
	if (signal(SIGTERM, &sig_handler) == SIG_ERR)
		WARN("unable to register SIGTERM handler");
	if (atexit(&cleanup))
		WARNGEN("atexit(&cleanup)");
	if (atexit(&free_buffers))
		WARNGEN("atexit(&free_buffers)");
	if (at_quick_exit(&cleanup))
		WARNGEN("at_quick_exit(&cleanup)");
	if (at_quick_exit(&free_buffers))
		WARNGEN("at_quick_exit(&free_buffers)");
}

static inline char *read_line(void)
{
	if (line) {
		free(line);
		line = NULL;
	}
	/* use an empty prompt if stdin is a pipe */
	if (isatty(STDIN_FILENO)) {
		line = readline("\n>>> ");
	} else {
		size_t cnt = 0;
		if (getline(&line, &cnt, stdin) == -1)
			line = NULL;
	}
	return line;
}

int main(int argc, char *argv[])
{
	struct stat hist_stat;
	size_t len = 0;
	size_t buf_sz = strlen(HIST_NAME) + 1;
	FILE *make_hist = NULL;
	char const optstring[] = "hptvwc:l:I:o:";
	char const *const home_env = getenv("HOME");

	/* add length of “$HOME/” if home_env is non-NULL */
	if (home_env && strcmp(home_env, "") != 0)
		buf_sz += strlen(home_env) + 1;
	/* prepend "~/" to history filename ("~/.cepl_history" by default) */
	if ((hist_file = calloc(1, buf_sz)) == NULL)
		ERRGEN("hist_file malloc()");
	/* check if home_env is non-NULL */
	if (home_env && strcmp(home_env, "") != 0) {
		len = strlen(home_env);
		memcpy(hist_file, home_env, len);
		hist_file[len++] = '/';
	}
	/* build history filename */
	memcpy(hist_file + len, HIST_NAME, strlen(HIST_NAME) + 1);

	/* initialize source buffers */
	init_buffers();
	/* initiatalize compiler arg array */
	cc_argv = parse_opts(argc, argv, optstring, &ofile);
	/* initialize user.final and actual.final then print version */
	build_final(argv);
	if (isatty(STDIN_FILENO))
		printf("\n%s\n", VERSION_STRING);
	/* enable completion */
	rl_completion_entry_function = &generator;
	rl_attempted_completion_function = &completer;
	rl_basic_word_break_characters = " \t\n\"\\'`@$><=|&{}()[]";
	rl_completion_suppress_append = 1;
	rl_bind_key('\t', &rl_complete);

	/* initialize history sesssion */
	using_history();
	/* create history file if it doesn't exsit */
	if (!(make_hist = fopen(hist_file, "a"))) {
		WARN("error creating history file with fopen()");
	} else {
		fclose(make_hist);
		has_hist = true;
	}
	/* read hist_file if size is non-zero */
	stat(hist_file, &hist_stat);
	if (hist_stat.st_size > 0) {
		if (read_history(hist_file)) {
			char hist_pre[] = "error reading history from ";
			char hist_full[sizeof hist_pre + strlen(hist_file)];
			char *hist_ptr = hist_full;
			memcpy(hist_ptr, hist_pre, sizeof hist_pre - 1);
			memcpy(hist_ptr + sizeof hist_pre - 1, hist_file, strlen(hist_file) + 1);
			WARN(hist_ptr);
		}
	}
	reg_handlers();

	/* loop readline() until EOF is read */
	while ((line = read_line()) && *line) {
		/* flush output streams */
		fflush(NULL);
		/* strip newlines */
		if ((tok_buf = strpbrk(line, "\f\r\n")) != NULL)
			tok_buf[0] = '\0';

		/* add and dedup history */
		if (line[0]) {
			/* search backward */
			while (history_search(line, -1) != -1) {
				/* this line is already in the history, remove the earlier entry */
				HIST_ENTRY *removed = remove_history(where_history());
				/* according to history docs we are supposed to free the stuff */
				if (removed->line)
					free(removed->line);
				if (removed->data)
					free(removed->data);
				free(removed);
			}
			/* search forward */
			while (history_search(line, 0) != -1) {
				/* this line is already in the history, remove the earlier entry */
				HIST_ENTRY *removed = remove_history(where_history());
				/* according to history docs we are supposed to free the stuff */
				if (removed->line)
					free(removed->line);
				if (removed->data)
					free(removed->data);
				free(removed);
			}
			add_history(line);
		}

		/* re-enable completion if disabled */
		rl_bind_key('\t', &rl_complete);
		/* re-allocate enough memory for line + '\t' + ';' + '\n' + '\0' */
		resize_buffer(&user.body, 3);
		resize_buffer(&actual.body, 3);
		resize_buffer(&user.final, 3);
		resize_buffer(&actual.final, 3);

		/* control sequence and preprocessor directive parsing */
		switch (line[0]) {
		case ';':
			switch(line[1]) {
			/* clean up and exit program */
			case 'q':
				free_buffers();
				cleanup();
				exit(EXIT_SUCCESS);
				/* unused break */
				break;

			/* toggle output file writing */
			case 'o':
				/* toggle global warning flag */
				out_flag ^= true;
				/* if file was open, close it and break early */
				if (!out_flag) {
					write_file();
					break;
				}
				tok_buf = strpbrk(line, " \t");
				/* break if file name empty */
				if (!tok_buf || strspn(tok_buf, " \t") == strlen(tok_buf)) {
					/* reset flag */
					out_flag ^= true;
					break;
				}
				/* increment pointer to start of definition */
				tok_buf += strspn(tok_buf, " \t");
				FILE *out_tmp = NULL;
				/* output file flag */
				if (out_flag && ((out_tmp = fopen(tok_buf, "w")) == NULL)) {
					free_buffers();
					cleanup();
					ERR("failed to create output file");
				}
				ofile = (FILE volatile *)out_tmp;
				break;

			/* toggle library parsing */
			case 'p':
				free_buffers();
				init_buffers();
				/* toggle global parse flag */
				parse_flag ^= true;
				/* re-initiatalize compiler arg array */
				cc_argv = parse_opts(argc, argv, optstring, &ofile);
				break;

			/* toggle variable tracking */
			case 't':
				free_buffers();
				init_buffers();
				/* toggle global parse flag */
				track_flag ^= true;
				/* re-initiatalize compiler arg array */
				cc_argv = parse_opts(argc, argv, optstring, &ofile);
				break;

			/* toggle warnings */
			case 'w':
				free_buffers();
				init_buffers();
				/* toggle global warning flag */
				warn_flag ^= true;
				/* re-initiatalize compiler arg array */
				cc_argv = parse_opts(argc, argv, optstring, &ofile);
				break;

			/* reset state */
			case 'r':
				free_buffers();
				init_buffers();
				/* re-initiatalize compiler arg array */
				cc_argv = parse_opts(argc, argv, optstring, &ofile);
				break;

			/* define an include/macro/function */
			case 'i': /* fallthrough */
			case 'm': /* fallthrough */
			case 'f':
				tok_buf = strpbrk(line, " \t");
				/* break if function definition empty */
				if (!tok_buf || strspn(tok_buf, " \t") == strlen(tok_buf))
					break;
				/* increment pointer to start of definition */
				tok_buf += strspn(tok_buf, " \t");
				/* re-allocate enough memory for line + '\n' + '\n' + '\0' */
				resize_buffer(&user.funcs, strlen(tok_buf) + 3);
				resize_buffer(&actual.funcs, strlen(tok_buf) + 3);
				build_funcs();
				/* TODO: find a workaround for var tracking */
				/* getting in the way of functions */
				/* if (!track_flag) { */
				/*         if (find_vars(tok_buf, &ids, &types)) */
				/*                 gen_var_list(&vars, &ids, &types); */
				/* } */
				break;

			/* show usage information */
			case 'h':
				fprintf(stderr, "%s %s %s\n", "Usage:", argv[0], USAGE_STRING);
				break;

			/* pop last history statement */
			case 'u':
				/* break early if no history to pop */
				if (user.flags.cnt < 2 || actual.flags.cnt < 2)
					break;
				pop_history(&user);
				pop_history(&actual);
				/* break early if tracking disabled */
				if (track_flag)
					break;
				/* re-init vars */
				if (types)
					free(types);
				if (vars.list) {
					for (size_t i = 0; i < vars.cnt; i++) {
						if (vars.list[i].key)
							free(vars.list[i].key);
					}
					free(vars.list);
				}
				free_str_list(&ids);
				types = NULL;
				vars.list = NULL;
				init_var_list(&vars);
				/* add vars from previous lines */
				for (size_t i = 1; i < user.lines.cnt; i++) {
					if (user.lines.list[i] && user.flags.list[i] == IN_MAIN) {
						if (find_vars(user.lines.list[i], &ids, &types))
							gen_var_list(&vars, &ids, &types);
					}
				}
				break;

			/* unknown command becomes a noop */
			default:;
			}
			break;

		/* dont append ';' for preprocessor directives */
		case '#':
			/* remove trailing ' ' and '\t' */
			for (size_t i = strlen(line) - 1; line[i] == ' ' || line[i] == '\t'; i--)
				line[i] = '\0';
			/* start building program source */
			build_body();
			strcat(user.body, "\n");
			strcat(actual.body, "\n");
			break;

		default:
			/* remove trailing ' ' and '\t' */
			for (size_t i = strlen(line) - 1; line[i] == ' ' || line[i] == '\t'; i--)
				line[i] = '\0';
			switch(line[strlen(line) - 1]) {
			case '{': /* fallthough */
			case '}': /* fallthough */
			case ';': /* fallthough */
			case '\\':
				build_body();
				/* remove extra trailing ';' */
				for (size_t i = strlen(user.body) - 1; user.body[i - 1] == ';'; i--) {
					if (user.body[i] == ';')
						user.body[i] = '\0';
				}
				for (size_t i = strlen(actual.body) - 1; actual.body[i - 1] == ';'; i--) {
					if (actual.body[i] == ';')
						actual.body[i] = '\0';
				}
				strcat(user.body, "\n");
				strcat(actual.body, "\n");
				/* extract identifiers and types */
				if (!track_flag) {
					if (find_vars(line, &ids, &types))
						gen_var_list(&vars, &ids, &types);
				}
				break;
			default:
				/* append ';' if no trailing '}', ';', or '\' */
				build_body();
				strcat(user.body, ";\n");
				strcat(actual.body, ";\n");
				/* extract identifiers and types */
				if (!track_flag) {
					if (find_vars(line, &ids, &types))
						gen_var_list(&vars, &ids, &types);
				}
			}
		}
		build_final(argv);

		/* print generated source code unless stdin is a pipe */
		if (isatty(STDIN_FILENO))
			printf("\n%s:\n==========\n%s\n==========\n", argv[0], user.final);
		/* print output and exit code */
		printf("exit status: %d\n", compile(actual.final, cc_argv, argv));
	}

	return 0;
}
