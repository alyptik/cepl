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
#include <setjmp.h>
#include <sys/stat.h>
#include <sys/types.h>

/* line buffer */
char *lptr;

/* "currently executing" flag */
static bool exec_flag = true;
/* SIGINT buffer for non-local goto */
static sigjmp_buf jmp_env;
/* TODO: change history filename to a non-hardcoded string */
static char hist_name[] = "./.cepl_history";

/* `-o` flag output file */
extern FILE *ofile;
/* output file buffer and cc args */
extern char *hist_file, **cc_argv;
/* string to compile */
extern char eval_arg[];
/* program source strucs (prg[0] is truncated for interactive printing) */
extern PROG_SRC prg[2];
/* completion list of generated symbols */
extern STR_LIST comp_list;
/* toggle flags */
extern bool asm_flag, eval_flag, out_flag, parse_flag, track_flag, warn_flag;
/* history file flag */
extern bool has_hist;
/* type, identifier, and var lists */
extern TYPE_LIST tl;
extern STR_LIST il;
extern VAR_LIST vl;
/* output filenames */
extern char *out_filename, *asm_filename;
extern enum asm_type asm_dialect;

static inline char *read_line(char **restrict ln)
{
	/* false while waiting for input */
	exec_flag = false;
	/* return early if executed with `-e` argument */
	if (eval_flag) {
		*ln = eval_arg;
		return *ln;
	}
	/* use an empty prompt if stdin is a pipe */
	if (isatty(STDIN_FILENO)) {
		*ln = readline(">>> ");
		return *ln;
	}

	/* redirect stdout to /dev/null */
	FILE *bitbucket;
	if (!(bitbucket = fopen("/dev/null", "r+b")))
		ERR("read_line() fopen()");
	rl_outstream = bitbucket;
	*ln = readline(NULL);
	rl_outstream = NULL;
	fclose(bitbucket);
	return *ln;
}

static inline void undo_last_line(void)
{
	/* break early if no history to pop */
	if (prg[0].flags.cnt < 1 || prg[1].flags.cnt < 1)
		return;
	for (size_t i = 0; i < 2; i++)
		pop_history(&prg[i]);
	/* break early if tracking disabled */
	if (!track_flag)
		return;
	/* re-init vars */
	if (tl.list) {
		free(tl.list);
		tl.list = NULL;
	}
	if (vl.list) {
		for (size_t i = 0; i < vl.cnt; i++)
			free(vl.list[i].id);
		free(vl.list);
	}
	init_vlist(&vl);
	/* add vars from previous lines */
	for (size_t i = 1; i < prg[0].lines.cnt; i++) {
		if (prg[0].lines.list[i] && prg[0].flags.list[i] == IN_MAIN) {
			if (find_vars(prg[0].lines.list[i], &il, &tl))
				gen_vlist(&vl, &il, &tl);
		}
	}
}

/* free_buffers() wrapper for at_quick_exit() registration */
static inline void free_bufs(void)
{
	free_buffers(&vl, &tl, &il, &prg, &lptr);
}

/* general signal handling function */
static void sig_handler(int sig)
{
	/* abort current input line */
	if (sig == SIGINT) {
		rl_clear_visible_line();
		rl_reset_line_state();
		rl_free_line_state();
		fputc('\n', stderr);
		if (exec_flag) {
			undo_last_line();
			exec_flag = false;
		}
		siglongjmp(jmp_env, 1);
	}
	free_buffers(&vl, &tl, &il, &prg, &lptr);
	cleanup();
	raise(sig);
}

/* register signal handlers to make sure that history is written out */
static void reg_handlers(void)
{
	/* signals to trap */
	struct { int sig; char *sig_name; } sigs[] = {
		{SIGHUP, "SIGHUP"}, {SIGINT, "SIGINT"},
		{SIGQUIT, "SIGQUIT"}, {SIGILL, "SIGILL"},
		{SIGABRT, "SIGABRT"}, {SIGFPE, "SIGFPE"},
		{SIGSEGV, "SIGSEGV"}, {SIGPIPE, "SIGPIPE"},
		{SIGALRM, "SIGALRM"}, {SIGTERM, "SIGTERM"},
		{SIGBUS, "SIGBUS"}, {SIGSYS, "SIGSYS"},
		{SIGVTALRM, "SIGVTALRM"}, {SIGXCPU, "SIGXCPU"},
		{SIGXFSZ, "SIGXFSZ"},
	};
	struct sigaction sa[ARRLEN(sigs)];
	for (size_t i = 0; i < ARRLEN(sigs); i++) {
		sa[i].sa_handler = &sig_handler;
		sigemptyset(&sa[i].sa_mask);
		/* Restart functions if interrupted by handler/reset signal disposition */
		sa[i].sa_flags = SA_RESETHAND|SA_RESTART;
		/* don't reset `SIGINT` handler */
		if (sigs[i].sig == SIGINT)
			sa[i].sa_flags &= ~SA_RESETHAND;
		if (sigaction(sigs[i].sig, &sa[i], NULL) == -1)
			ERRMSG(sigs[i].sig_name, "sigaction()");
	}
	if (at_quick_exit(&cleanup))
		WARN("at_quick_exit(&cleanup)");
	if (at_quick_exit(&free_bufs))
		WARN("at_quick_exit(&free_bufs)");
}

static void eval_line(char **restrict argv)
{
	char *ln = NULL, *ln_save = lptr, *term = getenv("TERM");
	VAR_LIST ln_vars;
	TYPE_LIST ln_types;
	PROG_SRC ln_prg[2];
	STR_LIST ln_ids;
	bool has_color = true;

	/* toggle flag to `false` if `TERM` is NULL, empty, or matches `dumb` */
	if (!isatty(STDOUT_FILENO) || !isatty(STDERR_FILENO) || !term || !strcmp(term, "") || !strcmp(term, "dumb"))
		has_color = false;
	char *ln_beg = has_color
		? "printf(\"" GREEN "%s%lld\\n" RST "\", \"result = \", (long long)"
		: "printf(\"%s%lld\\n\", \"result = \", (long long)";
	char *ln_end = ");";
	size_t sz = strlen(ln_beg) + strlen(ln_end) + strlen(lptr) + 1;
	/* save and close stderr */
	int saved_fd = dup(STDERR_FILENO);
	close(STDERR_FILENO);

	/* initialize source buffers */
	init_buffers(&ln_vars, &ln_types, &ln_ids, &ln_prg, &ln);
	xmalloc(&ln, strlen(lptr) + sz, "eval_line() malloc");
	strmv(0, ln, ln_beg);
	strmv(CONCAT, ln, lptr);
	strmv(CONCAT, ln, ln_end);
	build_final(&ln_prg, &ln_vars, argv);
#ifdef _DEBUG
	puts(ln);
#endif

	for (size_t i = 0; i < 2; i++) {
		rsz_buf(&ln_prg[i].b, &ln_prg[i].b_sz, &ln_prg[i].b_max, sz, &lptr);
		rsz_buf(&ln_prg[i].total, &ln_prg[i].t_sz, &ln_prg[i].t_max, sz, &lptr);
	}
	if (lptr[0] == ';' || find_vars(lptr, &ln_ids, &ln_types)) {
		free_buffers(&ln_vars, &ln_types, &ln_ids, &ln_prg, &ln);
		dup2(saved_fd, STDERR_FILENO);
		lptr = ln_save;
		return;
	}
	build_body(&ln_prg, ln);
	build_final(&ln_prg, &ln_vars, argv);
	/* print generated source code unless stdin is a pipe */
	if (isatty(STDIN_FILENO) && !eval_flag)
		compile(ln_prg[1].total, cc_argv, argv);
	free_buffers(&ln_vars, &ln_types, &ln_ids, &ln_prg, &ln);
	dup2(saved_fd, STDERR_FILENO);
	lptr = ln_save;
}

int main(int argc, char *argv[])
{
	struct stat hist_stat;
	size_t hist_len = 0;
	size_t buf_sz = sizeof hist_name;
	FILE *make_hist = NULL;
	char const optstring[] = "hptvwc:a:e:i:l:I:o:";
	char const *const home_env = getenv("HOME");
	/* token buffers */
	char *lbuf = NULL, *tbuf = NULL;

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
	init_buffers(&vl, &tl, &il, &prg, &lbuf);
	/* initiatalize compiler arg array */
	cc_argv = parse_opts(argc, argv, optstring, &ofile, &out_filename, &asm_filename);
	/* initialize prg[0].total and prg[1].total then print version */
	build_final(&prg, &vl, argv);
	if (isatty(STDIN_FILENO) && !eval_flag)
		printf("%s\n", VERSION_STRING);
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
	rl_set_signals();
	sigsetjmp(jmp_env, 1);

	/* loop readline() until EOF is read */
	while (read_line(&lbuf)) {
		/* if no input then do nothing */
		if (!*lbuf)
			continue;

		/* point global at line */
		lptr = lbuf;
		/* lptr newlines */
		if ((tbuf = strpbrk(lptr, "\f\r\n")))
			tbuf[0] = '\0';
		/* add and dedup history */
		dedup_history(&lbuf);
		/* re-enable completion if disabled */
		rl_bind_key('\t', &rl_complete);

		/* re-allocate enough memory for line + '\t' + ';' + '\n' + '\0' */
		for (size_t i = 0; i < 2; i++) {
			rsz_buf(&prg[i].b, &prg[i].b_sz, &prg[i].b_max, 3, &lptr);
			rsz_buf(&prg[i].total, &prg[i].t_sz, &prg[i].t_max, 3, &lptr);
		}

		/* strip leading whitespace */
		lptr += strspn(lptr, " \t");

		/* print line expression result */
		eval_line(argv);
		/* reset to defaults */
		warn_flag = false;
		track_flag = true;
		parse_flag = true;
		out_flag = false;
		eval_flag = false;
		asm_flag = false;
		/* re-initiatalize compiler arg array */
		cc_argv = parse_opts(argc, argv, optstring, &ofile, &out_filename, &asm_filename);

		/* control sequence and preprocessor directive parsing */
		switch (lptr[0]) {
		case ';':
			switch(lptr[1]) {
			/* pop last history statement */
			case 'u':
				undo_last_line();
				break;

			/* toggle writing at&t-dialect asm output */
			case 'a':
				/* if file was open, flip it and break early */
				if (asm_flag) {
					asm_flag ^= true;
					break;
				}
				asm_flag ^= true;
				tbuf = strpbrk(lptr, " \t");
				/* break if file name empty */
				if (!tbuf || strspn(tbuf, " \t") == strlen(tbuf)) {
					/* reset flag */
					asm_flag ^= true;
					break;
				}
				/* increment pointer to start of definition */
				tbuf += strspn(tbuf, " \t");
				if (asm_filename) {
					free(asm_filename);
					asm_filename = NULL;
				}
				if (!(asm_filename = calloc(1, strlen(tbuf) + 1)))
					ERR("error during asm_filename calloc()");
				strmv(0, asm_filename, tbuf);
				free_buffers(&vl, &tl, &il, &prg, &lbuf);
				init_buffers(&vl, &tl, &il, &prg, &lbuf);
				asm_dialect = ATT;
				/* reset to defaults */
				warn_flag = false;
				track_flag = true;
				parse_flag = true;
				out_flag = false;
				eval_flag = false;
				/* re-initiatalize compiler arg array */
				cc_argv = parse_opts(argc, argv, optstring, &ofile, &out_filename, &asm_filename);
				break;

			/* toggle writing intel-dialect asm output */
			case 'i':
				/* if file was open, flip it and break early */
				if (asm_flag) {
					asm_flag ^= true;
					break;
				}
				asm_flag ^= true;
				tbuf = strpbrk(lptr, " \t");
				/* break if file name empty */
				if (!tbuf || strspn(tbuf, " \t") == strlen(tbuf)) {
					/* reset flag */
					asm_flag ^= true;
					break;
				}
				/* increment pointer to start of definition */
				tbuf += strspn(tbuf, " \t");
				if (asm_filename) {
					free(asm_filename);
					asm_filename = NULL;
				}
				if (!(asm_filename = calloc(1, strlen(tbuf) + 1)))
					ERR("error during asm_filename calloc()");
				strmv(0, asm_filename, tbuf);
				free_buffers(&vl, &tl, &il, &prg, &lbuf);
				init_buffers(&vl, &tl, &il, &prg, &lbuf);
				asm_dialect = INTEL;
				/* reset to defaults */
				warn_flag = false;
				track_flag = true;
				parse_flag = true;
				out_flag = false;
				eval_flag = false;
				/* re-initiatalize compiler arg array */
				cc_argv = parse_opts(argc, argv, optstring, &ofile, &out_filename, &asm_filename);
				break;

			/* toggle output file writing */
			case 'o':
				/* if file was open, flip it and break early */
				if (out_flag) {
					out_flag ^= true;
					break;
				}
				out_flag ^= true;
				tbuf = strpbrk(lptr, " \t");
				/* break if file name empty */
				if (!tbuf || strspn(tbuf, " \t") == strlen(tbuf)) {
					/* reset flag */
					out_flag ^= true;
					break;
				}
				/* increment pointer to start of definition */
				tbuf += strspn(tbuf, " \t");
				if (out_filename) {
					free(out_filename);
					out_filename = NULL;
				}
				if (!(out_filename = calloc(1, strlen(tbuf) + 1)))
					ERR("error during out_filename calloc()");
				strmv(0, out_filename, tbuf);
				free_buffers(&vl, &tl, &il, &prg, &lbuf);
				init_buffers(&vl, &tl, &il, &prg, &lbuf);
				printf("%d", out_flag);
				/* reset to defaults */
				warn_flag = false;
				track_flag = true;
				parse_flag = true;
				eval_flag = false;
				asm_flag = false;
				/* re-initiatalize compiler arg array */
				cc_argv = parse_opts(argc, argv, optstring, &ofile, &out_filename, &asm_filename);
				break;

			/* toggle library parsing */
			case 'p':
				parse_flag ^= true;
				free_buffers(&vl, &tl, &il, &prg, &lbuf);
				init_buffers(&vl, &tl, &il, &prg, &lbuf);
				/* reset to defaults */
				warn_flag = false;
				track_flag = true;
				out_flag = false;
				eval_flag = false;
				asm_flag = false;
				/* re-initiatalize compiler arg array */
				cc_argv = parse_opts(argc, argv, optstring, &ofile, &out_filename, &asm_filename);
				break;

			/* toggle variable tracking */
			case 't':
				track_flag ^= true;
				free_buffers(&vl, &tl, &il, &prg, &lbuf);
				init_buffers(&vl, &tl, &il, &prg, &lbuf);
				/* reset to defaults */
				warn_flag = false;
				parse_flag = true;
				out_flag = false;
				eval_flag = false;
				asm_flag = false;
				/* re-initiatalize compiler arg array */
				cc_argv = parse_opts(argc, argv, optstring, &ofile, &out_filename, &asm_filename);
				break;

			/* toggle warnings */
			case 'w':
				warn_flag ^= true;
				free_buffers(&vl, &tl, &il, &prg, &lbuf);
				init_buffers(&vl, &tl, &il, &prg, &lbuf);
				/* reset to defaults */
				track_flag = true;
				parse_flag = true;
				out_flag = false;
				eval_flag = false;
				asm_flag = false;
				/* re-initiatalize compiler arg array */
				cc_argv = parse_opts(argc, argv, optstring, &ofile, &out_filename, &asm_filename);
				break;

			/* reset state */
			case 'r':
				free_buffers(&vl, &tl, &il, &prg, &lbuf);
				init_buffers(&vl, &tl, &il, &prg, &lbuf);
				/* reset to defaults */
				warn_flag = false;
				track_flag = true;
				parse_flag = true;
				out_flag = false;
				eval_flag = false;
				asm_flag = false;
				/* re-initiatalize compiler arg array */
				cc_argv = parse_opts(argc, argv, optstring, &ofile, &out_filename, &asm_filename);
				break;

			/* define an include/macro/function */
			case 'm': /* fallthrough */
			case 'f':
				/* remove trailing ' ' and '\t' */
				for (size_t i = strlen(lptr) - 1; i > 0; i--) {
					if (lptr[i] != ' ' && lptr[i] != '\t')
						break;
					lptr[i] = '\0';
				}
				tbuf = strpbrk(lptr, " \t");
				/* break if function definition empty */
				if (!tbuf || strspn(tbuf, " \t") == strlen(tbuf))
					break;
				/* increment pointer to start of definition */
				tbuf += strspn(tbuf, " \t");
				/* re-allocate enough memory for lptr + '\n' + '\n' + '\0' */
				size_t s = strlen(tbuf) + 3;
				for (size_t i = 0; i < 2; i++) {
					rsz_buf(&prg[i].f, &prg[i].f_sz, &prg[i].f_max, s, &tbuf);
				}

				switch (tbuf[0]) {
				/* dont append ';' for preprocessor directives */
				case '#':
					/* remove trailing ' ' and '\t' */
					for (size_t i = strlen(tbuf) - 1; i > 0; i--) {
						if (tbuf[i] != ' ' && tbuf[i] != '\t')
							break;
						tbuf[i] = '\0';
					}
					build_funcs(&prg, tbuf);
					for (size_t i = 0; i < 2; i++)
						strmv(CONCAT, prg[i].f, "\n");
					break;

				default:
					/* remove trailing ' ' and '\t' */
					for (size_t i = strlen(tbuf) - 1; i > 0; i--) {
						if (tbuf[i] != ' ' && tbuf[i] != '\t')
							break;
						tbuf[i] = '\0';
					}

					switch(tbuf[strlen(tbuf) - 1]) {
					case '{': /* fallthough */
					case '}': /* fallthough */
					case ';': /* fallthough */
					case '\\': {
							/* remove extra trailing ';' */
							for (size_t j = strlen(tbuf) - 1; j > 0; j--) {
								if (tbuf[j] != ';' || tbuf[j - 1] != ';')
									break;
								tbuf[j] = '\0';
							}
							build_funcs(&prg, tbuf);
							for (size_t i = 0; i < 2; i++)
								strmv(CONCAT, prg[i].f, "\n");

							STR_LIST tmp = strsplit(lptr);
							for (size_t i = 0; i < tmp.cnt; i++) {
								/* extract identifiers and types */
								if (track_flag && find_vars(tmp.list[i], &il, &tl))
									gen_vlist(&vl, &il, &tl);
							}
							free_str_list(&tmp);
							break;
						}

					default: {
							build_funcs(&prg, tbuf);
							/* append ';' if no trailing '}', ';', or '\' */
							for (size_t i = 0; i < 2; i++)
								strmv(CONCAT, prg[i].f, ";\n");
							STR_LIST tmp = strsplit(lptr);
							for (size_t i = 0; i < tmp.cnt; i++) {
								/* extract identifiers and types */
								if (track_flag && find_vars(tmp.list[i], &il, &tl))
									gen_vlist(&vl, &il, &tl);
							}
							free_str_list(&tmp);
						 }
					}
				}
				break;

			/* show usage information */
			case 'h':
				fprintf(stderr, "%s %s %s\n", "Usage:", argv[0], USAGE_STRING);
				break;

			/* clean up and exit program */
			case 'q':
				free_buffers(&vl, &tl, &il, &prg, &lbuf);
				cleanup();
				exit(EXIT_SUCCESS);
				/* unused break */
				break;

			/* unknown command becomes a noop */
			default:;
			}
			break;

		/* dont append ';' for preprocessor directives */
		case '#':
			/* remove trailing ' ' and '\t' */
			for (size_t i = strlen(lptr) - 1; i > 0; i--) {
				if (lptr[i] != ' ' && lptr[i] != '\t')
					break;
				lptr[i] = '\0';
			}
			/* start building program source */
			build_body(&prg, lptr);
			for (size_t i = 0; i < 2; i++)
				strmv(CONCAT, prg[i].b, "\n");
			break;

		default:
			/* remove trailing ' ' and '\t' */
			for (size_t i = strlen(lptr) - 1; i > 0; i--) {
				if (lptr[i] != ' ' && lptr[i] != '\t')
					break;
				lptr[i] = '\0';
			}
			switch(lptr[strlen(lptr) - 1]) {
			case '{': /* fallthough */
			case '}': /* fallthough */
			case ';': /* fallthough */
			case '\\': {
					build_body(&prg, lptr);
					for (size_t i = 0; i < 2; i++) {
						/* remove extra trailing ';' */
						size_t b_len = strlen(prg[i].b) - 1;
						for (size_t j = b_len; j > 0; j--) {
							if (prg[i].b[j] != ';' || prg[i].b[j - 1] != ';')
								break;
							prg[i].b[j] = '\0';
						}
						strmv(CONCAT, prg[i].b, "\n");
					}
					STR_LIST tmp = strsplit(lptr);
					for (size_t i = 0; i < tmp.cnt; i++) {
						/* extract identifiers and types */
						if (track_flag && find_vars(tmp.list[i], &il, &tl))
							gen_vlist(&vl, &il, &tl);
					}
					free_str_list(&tmp);
					break;
				   }

			default: {
					build_body(&prg, lptr);
					/* append ';' if no trailing '}', ';', or '\' */
					for (size_t i = 0; i < 2; i++)
						strmv(CONCAT, prg[i].b, ";\n");
					STR_LIST tmp = strsplit(lptr);
					for (size_t i = 0; i < tmp.cnt; i++) {
						/* extract identifiers and types */
						if (track_flag && find_vars(tmp.list[i], &il, &tl))
							gen_vlist(&vl, &il, &tl);
					}
					free_str_list(&tmp);
				}
			}
		}

		/* set to true before compiling */
		exec_flag = true;
		/* finalize source */
		build_final(&prg, &vl, argv);
		/* fix buffering issues */
		sync();
		usleep(5000);
		/* print generated source code unless stdin is a pipe */
		if (isatty(STDIN_FILENO) && !eval_flag)
			printf("%s:\n==========\n%s\n==========\n", argv[0], prg[0].total);
		int ret = compile(prg[1].total, cc_argv, argv);
		/* fix buffering issues */
		sync();
		usleep(5000);
		/* print output and exit code if non-zero */
		if (ret || (isatty(STDIN_FILENO) && !eval_flag))
			printf("[exit status: %d]\n", ret);

		/* exit if executed with `-e` argument */
		if (eval_flag) {
			lbuf = NULL;
			break;
		}
		/* cleanup old buffer */
		free(lptr);
		lptr = NULL;
	}

	free_buffers(&vl, &tl, &il, &prg, &lbuf);
	cleanup();
	return 0;
}
