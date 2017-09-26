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

#include "compile.h"
#include "errs.h"
#include "hist.h"
#include "parseopts.h"
#include "readline.h"
#include "vars.h"
#include <sys/stat.h>
#include <sys/types.h>

/* TODO: change history filename to a non-hardcoded string */
static char hist_name[] = "./.cepl_history";
/* static line buffer */
static char *gline;

/* `-o` flag output file */
extern FILE volatile *ofile;
/* output file buffer */
extern char *hist_file;
/* program source strucs (prog[0] is truncated for interactive printing) */
extern struct prog_src prog[2];
/* compiler arg array */
extern char **cc_argv;
/* completion list of generated symbols */
extern struct str_list comp_list;
/* toggle flag for warnings and completions */
extern bool warn_flag, parse_flag, track_flag, out_flag;
/* history file flag */
extern bool has_hist;
/* type, identifier, and var lists */
extern enum var_type *types;
extern struct str_list ids;
extern struct var_list vars;

static inline char *read_line(char **ln)
{
	/* cleanup old buffer */
	if (ln && *ln) {
		free(*ln);
		*ln = NULL;
	}
	/* use an empty prompt if stdin is a pipe */
	if (isatty(STDIN_FILENO)) {
		*ln = readline("\n>>> ");
	} else {
		/* redirect stdout to /dev/null */
		FILE *null;
		if (!(null = fopen("/dev/null", "r+b")))
			ERR("read_line() fopen()");
		rl_outstream = null;
		*ln = readline(NULL);
		rl_outstream = NULL;
		fclose(null);
	}
	return *ln;
}

/* free_buffers() wrapper for at_quick_exit() registration */
static inline void free_bufs(void)
{
	free_buffers(&vars, &types, &ids, &prog, &gline);
}

/* general signal handling function */
static inline void sig_handler(int sig)
{
	free_buffers(&vars, &types, &ids, &prog, &gline);
	cleanup();
	raise(sig);
}

/* register signal handlers to make sure that history is written out */
static inline void reg_handlers(void)
{
	/* signal array */
	int sigs[] = {
		SIGHUP, SIGINT, SIGQUIT,
		SIGABRT, SIGFPE, SIGSEGV,
		SIGPIPE, SIGALRM, SIGTERM,
		SIGUSR1, SIGUSR2, SIGBUS,
		SIGPOLL, SIGPROF, SIGSYS,
		SIGVTALRM, SIGXCPU, SIGXFSZ,
	};
	struct sigaction sa[sizeof sigs / sizeof sigs[0]];
	for (size_t i = 0; i < sizeof sigs / sizeof sigs[0]; i++) {
		sa[i].sa_handler = &sig_handler;
		sigemptyset(&sa[i].sa_mask);
		/* Restart functions if interrupted by handler and reset disposition */
		sa[i].sa_flags = SA_RESETHAND|SA_RESTART|SA_NODEFER;
		if (sigaction(sigs[i], &sa[i], NULL) == -1)
			ERRARR("sigaction() sig", i);
	}
	if (at_quick_exit(&cleanup))
		WARN("at_quick_exit(&cleanup)");
	if (at_quick_exit(&free_bufs))
		WARN("at_quick_exit(&free_bufs(prog, line)");
}

int main(int argc, char *argv[])
{
	struct stat hist_stat;
	size_t hist_len = 0;
	size_t buf_sz = sizeof hist_name;
	FILE *make_hist = NULL;
	char const optstring[] = "hptvwc:l:I:o:";
	char const *const home_env = getenv("HOME");
	/* line and token buffers */
	char *line = NULL, *tok_buf =  NULL;

	/* add hist_length of “$HOME/” if home_env is non-NULL */
	if (home_env && strcmp(home_env, ""))
		buf_sz += strlen(home_env) + 1;
	/* prepend "~/" to history fihist_lename ("~/.cepl_history" by default) */
	if (!(hist_file = calloc(1, buf_sz)))
		ERR("hist_file malloc()");
	/* check if home_env is non-NULL */
	if (home_env && strcmp(home_env, "")) {
		hist_len = strlen(home_env);
		strmv(0, hist_file, home_env);
		hist_file[hist_len++] = '/';
	}
	/* build history filename */
	strmv(hist_len, hist_file, hist_name);

	/* initialize source buffers */
	init_buffers(&vars, &types, &ids, &prog, &line);
	/* initiatalize compiler arg array */
	cc_argv = parse_opts(argc, argv, optstring, &ofile);
	/* initialize prog[0].total and prog[1].total then print version */
	build_final(&prog, &vars, argv);
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
			strmv(0, hist_ptr, hist_pre);
			strmv(sizeof hist_pre - 1, hist_ptr, hist_file);
			WARN(hist_ptr);
		}
	}
	reg_handlers();

	/* loop readline() until EOF is read */
	while (read_line(&line) && *line) {
		/* point global at line */
		gline = line;
		/* strip newlines */
		if ((tok_buf = strpbrk(line, "\f\r\n")))
			tok_buf[0] = '\0';
		/* add and dedup history */
		dedup_history(&line);
		/* re-enable completion if disabled */
		rl_bind_key('\t', &rl_complete);
		/* re-allocate enough memory for line + '\t' + ';' + '\n' + '\0' */
		for (size_t i = 0; i < 2; i++) {
			resize_buffer(&prog[i].body, &prog[i].b_sz, &prog[i].b_max, 3, &vars, &types, &ids, &prog, &line);
			resize_buffer(&prog[i].total, &prog[i].t_sz, &prog[i].t_max, 3, &vars, &types, &ids, &prog, &line);
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
				free_buffers(&vars, &types, &ids, &prog, &line);
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
					write_file(&ofile, &prog);
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
					free_buffers(&vars, &types, &ids, &prog, &line);
					cleanup();
					ERR("failed to create output file");
				}
				break;

			/* toggle library parsing */
			case 'p':
				free_buffers(&vars, &types, &ids, &prog, &line);
				init_buffers(&vars, &types, &ids, &prog, &line);
				/* toggle global parse flag */
				parse_flag ^= true;
				/* re-initiatalize compiler arg array */
				cc_argv = parse_opts(argc, argv, optstring, &ofile);
				break;

			/* toggle variable tracking */
			case 't':
				free_buffers(&vars, &types, &ids, &prog, &line);
				init_buffers(&vars, &types, &ids, &prog, &line);
				/* toggle global parse flag */
				track_flag ^= true;
				/* re-initiatalize compiler arg array */
				cc_argv = parse_opts(argc, argv, optstring, &ofile);
				break;

			/* toggle warnings */
			case 'w':
				free_buffers(&vars, &types, &ids, &prog, &line);
				init_buffers(&vars, &types, &ids, &prog, &line);
				/* toggle global warning flag */
				warn_flag ^= true;
				/* re-initiatalize compiler arg array */
				cc_argv = parse_opts(argc, argv, optstring, &ofile);
				break;

			/* reset state */
			case 'r':
				free_buffers(&vars, &types, &ids, &prog, &line);
				init_buffers(&vars, &types, &ids, &prog, &line);
				/* re-initiatalize compiler arg array */
				cc_argv = parse_opts(argc, argv, optstring, &ofile);
				break;

			/* define an include/macro/function */
			case 'i': /* fallthrough */
			case 'm': /* fallthrough */
			case 'f':
				/* remove trailing ' ' and '\t' */
				for (size_t i = strlen(strip) - 1; i > 0; i--) {
					if (strip[i] != ' ' && strip[i] != '\t')
						break;
					strip[i] = '\0';
				}
				tok_buf = strpbrk(strip, " \t");
				/* break if function definition empty */
				if (!tok_buf || strspn(tok_buf, " \t") == strlen(tok_buf))
					break;
				/* increment pointer to start of definition */
				tok_buf += strspn(tok_buf, " \t");
				/* re-allocate enough memory for strip + '\n' + '\n' + '\0' */
				size_t tok_sz = strlen(tok_buf) + 3;
				for (size_t i = 0; i < 2; i++)
					resize_buffer(&prog[i].funcs, &prog[i].f_sz, &prog[i].f_max, tok_sz, &vars, &types, &ids, &prog, &tok_buf);
				switch (tok_buf[0]) {
				/* dont append ';' for preprocessor directives */
				case '#':
					/* remove trailing ' ' and '\t' */
					for (size_t i = strlen(tok_buf) - 1; i > 0; i--) {
						if (tok_buf[i] != ' ' && tok_buf[i] != '\t')
							break;
						tok_buf[i] = '\0';
					}
					build_funcs(&prog, tok_buf);
					for (size_t i = 0; i < 2; i++)
						strmv(CONCAT, prog[i].funcs, "\n");
					break;

				default:
					/* remove trailing ' ' and '\t' */
					for (size_t i = strlen(tok_buf) - 1; i > 0; i--) {
						if (tok_buf[i] != ' ' && tok_buf[i] != '\t')
							break;
						tok_buf[i] = '\0';
					}
					switch(tok_buf[strlen(tok_buf) - 1]) {
					case '{': /* fallthough */
					case '}': /* fallthough */
					case ';': /* fallthough */
					case '\\':
						build_funcs(&prog, tok_buf);
						for (size_t i = 0; i < 2; i++) {
							/* remove extra trailing ';' */
							size_t len = strlen(prog[i].funcs) - 1;
							for (size_t j = len; i > 0 && prog[i].funcs[j - 1] == ';'; j--) {
								if (prog[i].funcs[j] == ';')
									prog[i].funcs[j] = '\0';
							}
							strmv(CONCAT, prog[i].funcs, "\n");
						}
						/* extract identifiers and types */
						if (track_flag && find_vars(strip, &ids, &types))
							gen_vlist(&vars, &ids, &types);
						break;

					default:
						build_funcs(&prog, tok_buf);
						/* append ';' if no trailing '}', ';', or '\' */
						for (size_t i = 0; i < 2; i++)
							strmv(CONCAT, prog[i].funcs, ";\n");
						/* extract identifiers and types */
						if (track_flag && find_vars(strip, &ids, &types))
							gen_vlist(&vars, &ids, &types);
					}
					/* track variables */
					if (track_flag && !strpbrk(tok_buf, "()")) {
						if (find_vars(tok_buf, &ids, &types))
							gen_vlist(&vars, &ids, &types);
					}
				}
				break;

			/* show usage information */
			case 'h':
				fprintf(stderr, "%s %s %s\n", "Usage:", argv[0], USAGE_STRING);
				break;

			/* pop last history statement */
			case 'u':
				/* break early if no history to pop */
				if (prog[0].flags.cnt < 1 || prog[1].flags.cnt < 1)
					break;
				for (size_t i = 0; i < 2; i++)
					pop_history(&prog[i]);
				/* break early if tracking disabled */
				if (!track_flag)
					break;

				/* re-init vars */
				if (types) {
					free(types);
					types = NULL;
				}
				if (vars.list) {
					for (size_t i = 0; i < vars.cnt; i++) {
						if (vars.list[i].key) {
							free(vars.list[i].key);
							vars.list[i].key = NULL;
						}

					}
					free(vars.list);
					vars.list = NULL;
				}
				init_vlist(&vars);
				/* add vars from previous lines */
				for (size_t i = 1; i < prog[0].lines.cnt; i++) {
					if (prog[0].lines.list[i] && prog[0].flags.list[i] == IN_MAIN) {
						if (find_vars(prog[0].lines.list[i], &ids, &types))
							gen_vlist(&vars, &ids, &types);
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
			for (size_t i = strlen(strip) - 1; i > 0; i--) {
				if (strip[i] != ' ' && strip[i] != '\t')
					break;
				strip[i] = '\0';
			}
			/* start building program source */
			build_body(&prog, line);
			for (size_t i = 0; i < 2; i++)
				strmv(CONCAT, prog[i].body, "\n");
			break;

		default:
			/* remove trailing ' ' and '\t' */
			for (size_t i = strlen(strip) - 1; i > 0; i--) {
				if (strip[i] != ' ' && strip[i] != '\t')
					break;
				strip[i] = '\0';
			}
			switch(strip[strlen(strip) - 1]) {
			case '{': /* fallthough */
			case '}': /* fallthough */
			case ';': /* fallthough */
			case '\\':
				build_body(&prog, line);
				for (size_t i = 0; i < 2; i++) {
					/* remove extra trailing ';' */
					size_t len = strlen(prog[i].body) - 1;
					for (size_t j = len; i > 0 && prog[i].body[j - 1] == ';'; j--) {
						if (prog[i].body[j] == ';')
							prog[i].body[j] = '\0';
					}
					strmv(CONCAT, prog[i].body, "\n");
				}
				/* extract identifiers and types */
				if (track_flag && find_vars(strip, &ids, &types))
					gen_vlist(&vars, &ids, &types);
				break;

			default:
				build_body(&prog, line);
				/* append ';' if no trailing '}', ';', or '\' */
				for (size_t i = 0; i < 2; i++)
					strmv(CONCAT, prog[i].body, ";\n");
				/* extract identifiers and types */
				if (track_flag && find_vars(strip, &ids, &types))
					gen_vlist(&vars, &ids, &types);
			}
		}

		/* finalize source */
		build_final(&prog, &vars, argv);
		/* print generated source code unless stdin is a pipe */
		if (isatty(STDIN_FILENO))
			printf("\n%s:\n==========\n%s\n==========\n", argv[0], prog[0].total);
		int ret = compile(prog[1].total, cc_argv, argv);
		/* print output and exit code if non-zero */
		if (ret || isatty(STDIN_FILENO))
			printf("[exit status: %d]\n", ret);
	}

	free_buffers(&vars, &types, &ids, &prog, &line);
	cleanup();
	return 0;
}
