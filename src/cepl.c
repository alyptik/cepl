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

#include <sys/stat.h>
#include <sys/types.h>
#include "compile.h"
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
	"#include <error.h>\n"
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
/* TODO: change history filename to a non-hardcoded string */
static char hist_name[] = "./.cepl_history";
/* line and token buffers */
static char *line, *tok_buf;
/* compiler arg array */
static char **cc_argv;
/* readline history variables */
static char *hist_file;
/* output file */
static FILE volatile *ofile;
/* program source strucs (prog[0] is truncated for interactive printing) */
static struct prog_src prog[2];
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
	fwrite(prog[1].total, strlen(prog[1].total), 1, output);
	fputc('\n', output);
	fflush(output);
	fclose(output);
	ofile = NULL;
}

static inline void free_buffers(void)
{
	/* write out history before freeing buffers */
	write_file();
	free_str_list(&ids);
	/* clean up user data */
	if (line)
		free(line);
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
	/* free program structs */
	for (size_t i = 0; i < 2; i++) {
		if (prog[i].funcs)
			free(prog[i].funcs);
		if (prog[i].body)
			free(prog[i].body);
		if (prog[i].total)
			free(prog[i].total);
		if (prog[i].flags.list)
			free(prog[i].flags.list);
		free_str_list(&prog[i].hist);
		free_str_list(&prog[i].lines);
		prog[i].b_sz = prog[i].f_sz = prog[i].t_sz = 0;
		prog[i].b_max = prog[i].f_max = prog[i].t_max = 1;
		/* `(void *)` casts needed to chain diff ptr types */
		prog[i].body = prog[i].funcs = prog[i].total = (void *)(prog[i].flags.list = NULL);
	}
	/* `(void *)` casts needed to chain diff ptr types */
	vars.list = (void *)(line = (void *)(cc_argv = (void *)(types = NULL)));
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
			memcpy(hist_ptr, hist_pre, sizeof hist_pre);
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
	prog[0].funcs = calloc(1, 1);
	prog[0].body = calloc(1, strlen(prog_start_user) + 1);
	prog[0].total = calloc(1, strlen(prelude) + strlen(prog_start_user) + strlen(prog_end) + 3);
	prog[0].f_sz = prog[0].f_max = 1;
	prog[0].b_sz = prog[0].b_max = strlen(prog_start_user) + 1;
	prog[0].t_sz = prog[0].t_max = strlen(prelude) + strlen(prog_start_user) + strlen(prog_end) + 3;
	/* actual is source passed to compiler */
	prog[1].funcs = calloc(1, strlen(prelude) + 1);
	prog[1].body = calloc(1, strlen(prog_start) + 1);
	prog[1].total = calloc(1, strlen(prelude) + strlen(prog_start) + strlen(prog_end) + 3);
	prog[1].f_sz = prog[1].f_max = strlen(prelude) + 1;
	prog[1].b_sz = prog[1].b_max = strlen(prog_start) + 1;
	prog[1].t_sz = prog[1].t_max = strlen(prelude) + strlen(prog_start) + strlen(prog_end) + 3;
	/* sanity check */
	if (!prog[0].funcs || !prog[0].body || !prog[0].total || !prog[1].funcs || !prog[1].body || !prog[1].total) {
		free_buffers();
		cleanup();
		ERR("initial pointer allocation");
	}
	/* no memcpy for prog[0].funcs */
	memcpy(prog[0].body, prog_start_user, strlen(prog_start_user) + 1);
	memcpy(prog[1].funcs, prelude, strlen(prelude) + 1);
	memcpy(prog[1].body, prog_start, strlen(prog_start) + 1);
	/* init source history and flag lists */
	for (size_t i = 0; i < 2; i++) {
		init_list(&prog[i].lines, "FOOBARTHISVALUEDOESNTMATTERTROLLOLOLOL");
		init_list(&prog[i].hist, "FOOBARTHISVALUEDOESNTMATTERTROLLOLOLOL");
		init_flag_list(&prog[i].flags);
	}
	init_var_list(&vars);
}

static inline void resize_buffer(char **buf, size_t *buf_sz, size_t *buf_max, size_t offset)
{
	/* sanity check */
	if (!buf || !*buf)
		return;
	char *tmp;
	size_t alloc_sz = strlen(*buf) + strlen(line) + offset + 1;
	if (!buf_sz || !buf_max) {
		/* current length + line length + extra characters + \0 */
		if (!(tmp = realloc(*buf, alloc_sz))) {
			free_buffers();
			cleanup();
			ERR("resize_buffer()");
		}
		*buf = tmp;
	} else {
		*buf_sz += alloc_sz;
		/* realloc only if buf_max is less than current size */
		if (*buf_sz < *buf_max)
			return;
		/* double until size is reached */
		while ((*buf_max *= 2) < *buf_sz);
		/* current length + line length + extra characters + \0 */
		if (!(tmp = realloc(*buf, *buf_max))) {
			free_buffers();
			cleanup();
			ERR("resize_buffer()");
		}
		*buf = tmp;
	}
}

static inline void build_funcs(void)
{
	for (size_t i = 0; i < 2; i++) {
		append_str(&prog[i].lines, tok_buf, 0);
		append_str(&prog[i].hist, prog[i].funcs, 0);
		append_flag(&prog[i].flags, NOT_IN_MAIN);
		/* generate function buffers */
		strcat(prog[i].funcs, tok_buf);
		strcat(prog[i].funcs, "\n");
	}
}

static inline void build_body(void)
{
	for (size_t i = 0; i < 2; i++) {
		append_str(&prog[i].lines, line, 0);
		append_str(&prog[i].hist, prog[i].body, 0);
		append_flag(&prog[i].flags, IN_MAIN);
		strcat(prog[i].body, "\t");
		strcat(prog[i].body, line);
	}
}

static inline void build_final(char *argv[])
{
	/* finish building current iteration of source code */
	for (size_t i = 0; i < 2; i++) {
		memcpy(prog[i].total, prog[i].funcs, strlen(prog[i].funcs) + 1);
		strcat(prog[i].total, prog[i].body);
		/* print variable values */
		if (track_flag && i == 1)
			print_vars(&vars, prog[i].total, cc_argv, argv);
		strcat(prog[i].total, prog_end);
	}
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

/* look for current line in readline history */
static inline void dedup_history(void)
{
	if (line && *line) {
		/* strip leading whitespace */
		char *strip = line;
		strip += strspn(line, " \t");
		/* search forward and backward in history */
		int cur_hist = where_history();
		for (int i = -1; i < 2; i += 2) {
			/* seek backwords or forwards */
			HIST_ENTRY *(*seek_hist)(void) = (i < 0) ? &previous_history : &next_history;
			while (history_search_prefix(strip, i) != -1) {
				/* if this line is already in the history, remove the earlier entry */
				HIST_ENTRY *ent = current_history();
				/* skip if NULL or not a complete match */
				if (!ent || !ent->line || strcmp(line, ent->line) != 0) {
					/* break if at end of list */
					if (!seek_hist())
						break;
					continue;
				}
				/* remove and free data */
				remove_history(where_history());
				/* free application data */
				histdata_t data = free_history_entry(ent);
				if (data)
					free(data);
			}
			history_set_pos(cur_hist);
		}
		/* reset history position and add the line */
		history_set_pos(cur_hist);
		add_history(strip);
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
	if (signal(SIGUSR1, &sig_handler) == SIG_ERR)
		WARN("unable to register SIGUSR1 handler");
	if (signal(SIGUSR2, &sig_handler) == SIG_ERR)
		WARN("unable to register SIGUSR2 handler");
	if (signal(SIGBUS, &sig_handler) == SIG_ERR)
		WARN("unable to register SIGBUS handler");
	if (signal(SIGPOLL, &sig_handler) == SIG_ERR)
		WARN("unable to register SIGPOLL handler");
	if (signal(SIGPROF, &sig_handler) == SIG_ERR)
		WARN("unable to register SIGPROF handler");
	if (signal(SIGSYS, &sig_handler) == SIG_ERR)
		WARN("unable to register SIGSYS handler");
	if (signal(SIGTRAP, &sig_handler) == SIG_ERR)
		WARN("unable to register SIGTRAP handler");
	if (signal(SIGVTALRM, &sig_handler) == SIG_ERR)
		WARN("unable to register SIGVTALRM handler");
	if (signal(SIGXCPU, &sig_handler) == SIG_ERR)
		WARN("unable to register SIGXCPU handler");
	if (signal(SIGXFSZ, &sig_handler) == SIG_ERR)
		WARN("unable to register SIGXFSZ handler");
	if (atexit(&cleanup))
		WARN("atexit(&cleanup)");
	if (atexit(&free_buffers))
		WARN("atexit(&free_buffers)");
	if (at_quick_exit(&cleanup))
		WARN("at_quick_exit(&cleanup)");
	if (at_quick_exit(&free_buffers))
		WARN("at_quick_exit(&free_buffers)");
}

static inline char *read_line(void)
{
	/* cleanup old buffer */
	if (line)
		free(line);
	/* use an empty prompt if stdin is a pipe */
	if (isatty(STDIN_FILENO)) {
		line = readline("\n>>> ");
	} else {
		/* redirect stdout to /dev/null */
		FILE *null;
		if (!(null = fopen("/dev/null", "r+b")))
			ERR("read_line() fopen()");
		rl_outstream = null;
		line = readline(NULL);
		rl_outstream = NULL;
		fclose(null);
	}
	/* flush output streams */
	fflush(NULL);
	return line;
}

int main(int argc, char *argv[])
{
	struct stat hist_stat;
	size_t len = 0;
	size_t buf_sz = sizeof hist_name;
	FILE *make_hist = NULL;
	char const optstring[] = "hptvwc:l:I:o:";
	char const *const home_env = getenv("HOME");

	/* add length of “$HOME/” if home_env is non-NULL */
	if (home_env && strcmp(home_env, "") != 0)
		buf_sz += strlen(home_env) + 1;
	/* prepend "~/" to history filename ("~/.cepl_history" by default) */
	if (!(hist_file = calloc(1, buf_sz)))
		ERR("hist_file malloc()");
	/* check if home_env is non-NULL */
	if (home_env && strcmp(home_env, "") != 0) {
		len = strlen(home_env);
		memcpy(hist_file, home_env, len);
		hist_file[len++] = '/';
	}
	/* build history filename */
	memcpy(hist_file + len, hist_name, sizeof hist_name);

	/* initialize source buffers */
	init_buffers();
	/* initiatalize compiler arg array */
	cc_argv = parse_opts(argc, argv, optstring, &ofile);
	/* initialize prog[0].total and prog[1].total then print version */
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
	if (!(make_hist = fopen(hist_file, "ab"))) {
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
		/* strip newlines */
		if ((tok_buf = strpbrk(line, "\f\r\n")))
			tok_buf[0] = '\0';
		/* add and dedup history */
		dedup_history();
		/* re-enable completion if disabled */
		rl_bind_key('\t', &rl_complete);
		/* re-allocate enough memory for line + '\t' + ';' + '\n' + '\0' */
		for (size_t i = 0; i < 2; i++) {
			resize_buffer(&prog[i].body, &prog[i].b_sz, &prog[i].b_max, 3);
			resize_buffer(&prog[i].total, &prog[i].t_sz, &prog[i].t_max, 3);
		}

		/* strip leading whitespace */
		char *strip = line;
		strip += strspn(line, " \t");
		/* control sequence and preprocessor directive parsing */
		switch (strip[0]) {
		case ';':
			switch(strip[1]) {
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
				tok_buf = strpbrk(strip, " \t");
				/* break if file name empty */
				if (!tok_buf || strspn(tok_buf, " \t") == strlen(tok_buf)) {
					/* reset flag */
					out_flag ^= true;
					break;
				}
				/* increment pointer to start of definition */
				tok_buf += strspn(tok_buf, " \t");
				/* output file flag */
				if (out_flag && !(ofile = fopen(tok_buf, "wb"))) {
					free_buffers();
					cleanup();
					ERR("failed to create output file");
				}
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
				tok_buf = strpbrk(strip, " \t");
				/* break if function definition empty */
				if (!tok_buf || strspn(tok_buf, " \t") == strlen(tok_buf))
					break;
				/* increment pointer to start of definition */
				tok_buf += strspn(tok_buf, " \t");
				/* re-allocate enough memory for strip + '\n' + '\n' + '\0' */
				size_t tok_sz = strlen(tok_buf) + 3;
				for (size_t i = 0; i < 2; i++)
					resize_buffer(&prog[i].funcs, &prog[i].f_sz, &prog[i].f_max, tok_sz);
				build_funcs();
				/* TODO: find a workaround for var tracking getting in the way of functions */
				/* if (track_flag && find_vars(tok_buf, &ids, &types)) */
				/*         gen_var_list(&vars, &ids, &types); */
				break;

			/* show usage information */
			case 'h':
				fprintf(stderr, "%s %s %s\n", "Usage:", argv[0], USAGE_STRING);
				break;

			/* pop last history statement */
			case 'u':
				/* break early if no history to pop */
				if (prog[0].flags.cnt < 2 || prog[1].flags.cnt < 2)
					break;
				for (size_t i = 0; i < 2; i++)
					pop_history(&prog[i]);
				/* break early if tracking disabled */
				if (!track_flag)
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
				types = NULL;
				vars.list = NULL;
				free_str_list(&ids);
				init_var_list(&vars);
				/* add vars from previous lines */
				for (size_t i = 1; i < prog[0].lines.cnt; i++) {
					if (prog[0].lines.list[i] && prog[0].flags.list[i] == IN_MAIN) {
						if (find_vars(prog[0].lines.list[i], &ids, &types))
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
			for (size_t i = strlen(strip) - 1; i > 0 && (strip[i] == ' ' || strip[i] == '\t'); i--)
				strip[i] = '\0';
			/* start building program source */
			build_body();
			for (size_t i = 0; i < 2; i++)
				strcat(prog[i].body, "\n");
			break;

		default:
			/* remove trailing ' ' and '\t' */
			for (size_t i = strlen(strip) - 1; i > 0 && (strip[i] == ' ' || strip[i] == '\t'); i--)
				strip[i] = '\0';
			switch(strip[strlen(strip) - 1]) {
			case '{': /* fallthough */
			case '}': /* fallthough */
			case ';': /* fallthough */
			case '\\':
				build_body();
				for (size_t i = 0; i < 2; i++) {
					/* remove extra trailing ';' */
					size_t len = strlen(prog[i].body) - 1;
					for (size_t j = len; i > 0 && prog[i].body[j - 1] == ';'; j--) {
						if (prog[i].body[j] == ';')
							prog[i].body[j] = '\0';
					}
					strcat(prog[i].body, "\n");
				}
				/* extract identifiers and types */
				if (track_flag && find_vars(strip, &ids, &types))
					gen_var_list(&vars, &ids, &types);
				break;
			default:
				/* append ';' if no trailing '}', ';', or '\' */
				build_body();
				for (size_t i = 0; i < 2; i++)
					strcat(prog[i].body, ";\n");
				/* extract identifiers and types */
				if (track_flag && find_vars(strip, &ids, &types))
					gen_var_list(&vars, &ids, &types);
			}
		}

		/* finalize source */
		build_final(argv);
		/* print generated source code unless stdin is a pipe */
		if (isatty(STDIN_FILENO))
			printf("\n%s:\n==========\n%s\n==========\n", argv[0], prog[0].total);
		int ret = compile(prog[1].total, cc_argv, argv);
		/* print output and exit code if non-zero */
		if (ret || isatty(STDIN_FILENO))
			printf("[exit status: %d]\n", ret);
	}

	return 0;
}
