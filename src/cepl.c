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
char *lptr;

/* `-o` flag output file */
extern FILE *ofile;
/* output file buffer and cc args */
extern char *hist_file, **cc_argv;
/* program source strucs (prg[0] is truncated for interactive printing) */
extern struct prog_src prg[2];
/* completion list of generated symbols */
extern struct str_list comp_list;
/* toggle flags */
extern bool asm_flag, out_flag, parse_flag, track_flag, warn_flag;
/* history file flag */
extern bool has_hist;
/* type, identifier, and var lists */
extern struct type_list tl;
extern struct str_list il;
extern struct var_list vl;
/* output filenames */
extern char *out_filename, *asm_filename;
extern enum asm_type volatile asm_dialect;

static inline char *read_line(char **restrict ln)
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
		FILE *bitbucket;
		if (!(bitbucket = fopen("/dev/null", "r+b")))
			ERR("read_line() fopen()");
		rl_outstream = bitbucket;
		*ln = readline(NULL);
		rl_outstream = NULL;
		fclose(bitbucket);
	}
	return *ln;
}

/* free_buffers() wrapper for at_quick_exit() registration */
static inline void free_bufs(void)
{
	free_buffers(&vl, &tl, &il, &prg, &lptr);
}

/* general signal handling function */
static inline void sig_handler(int sig)
{
	free_buffers(&vl, &tl, &il, &prg, &lptr);
	cleanup();
	raise(sig);
}

/* register signal handlers to make sure that history is written out */
static inline void reg_handlers(void)
{
	/* signals to trap */
	struct {
		int sig;
		char *sig_name;
	} sigs[] = {
		{SIGHUP, "SIGHUP"}, {SIGINT, "SIGINT"},
		{SIGQUIT, "SIGQUIT"}, {SIGILL, "SIGILL"},
		{SIGABRT, "SIGABRT"}, {SIGFPE, "SIGFPE"},
		{SIGSEGV, "SIGSEGV"}, {SIGPIPE, "SIGPIPE"},
		{SIGALRM, "SIGALRM"}, {SIGTERM, "SIGTERM"},
		{SIGBUS, "SIGBUS"}, {SIGSYS, "SIGSYS"},
		{SIGVTALRM, "SIGVTALRM"}, {SIGXCPU, "SIGXCPU"},
		{SIGXFSZ, "SIGXFSZ"},
	};
	struct sigaction sa[sizeof sigs / sizeof sigs[0]];
	for (size_t i = 0; i < sizeof sigs / sizeof sigs[0]; i++) {
		sa[i].sa_handler = &sig_handler;
		sigemptyset(&sa[i].sa_mask);
		/* Restart functions if interrupted by handler/reset signal disposition */
		sa[i].sa_flags = SA_RESETHAND|SA_RESTART|SA_NODEFER;
		if (sigaction(sigs[i].sig, &sa[i], NULL) == -1)
			ERRMSG(sigs[i].sig_name, "sigaction()");
	}
	if (at_quick_exit(&cleanup))
		WARN("at_quick_exit(&cleanup)");
	if (at_quick_exit(&free_bufs))
		WARN("at_quick_exit(&free_bufs)");
}

int main(int argc, char *argv[])
{
	struct stat hist_stat;
	size_t hist_len = 0;
	size_t buf_sz = sizeof hist_name;
	FILE *make_hist = NULL;
	char const optstring[] = "hptvwc:a:i:l:I:o:";
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
	while (read_line(&lbuf) && *lbuf) {
		/* point global at line */
		lptr = lbuf;
		/* strip newlines */
		if ((tbuf = strpbrk(lptr, "\f\r\n")))
			tbuf[0] = '\0';
		/* add and dedup history */
		dedup_history(&lbuf);
		/* re-enable completion if disabled */
		rl_bind_key('\t', &rl_complete);
		/* re-allocate enough memory for line + '\t' + ';' + '\n' + '\0' */
		for (size_t i = 0; i < 2; i++) {
			rsz_buf(&prg[i].b, &prg[i].b_sz, &prg[i].b_max, 3, &vl, &tl, &il, &prg, &lptr);
			rsz_buf(&prg[i].total, &prg[i].t_sz, &prg[i].t_max, 3, &vl, &tl, &il, &prg, &lptr);
		}

		/* strip leading whitespace */
		char *strip = lptr;
		strip += strspn(lptr, " \t");
		/* control sequence and preprocessor directive parsing */
		switch (strip[0]) {
		case ';':
			switch(strip[1]) {

			/* pop last history statement */
			case 'u':
				/* break early if no history to pop */
				if (prg[0].flags.cnt < 1 || prg[1].flags.cnt < 1)
					break;
				for (size_t i = 0; i < 2; i++)
					pop_history(&prg[i]);
				/* break early if tracking disabled */
				if (!track_flag)
					break;
				/* re-init vars */
				if (tl.list) {
					free(tl.list);
					tl.list = NULL;
				}
				if (vl.list) {
					for (size_t i = 0; i < vl.cnt; i++) {
						if (vl.list[i].key)
							free(vl.list[i].key);
					}
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
				break;

			/* toggle writing at&t-dialect asm output */
			case 'a':
				asm_flag ^= true;
				/* if file was open, close it and break early */
				if (asm_flag)
					break;
				tbuf = strpbrk(strip, " \t");
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
				/* re-initiatalize compiler arg array */
				cc_argv = parse_opts(argc, argv, optstring, &ofile, &out_filename, &asm_filename);
				break;

			/* toggle writing intel-dialect asm output */
			case 'i':
				asm_flag ^= true;
				/* if file was open, close it and break early */
				if (asm_flag)
					break;
				tbuf = strpbrk(strip, " \t");
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
				/* re-initiatalize compiler arg array */
				cc_argv = parse_opts(argc, argv, optstring, &ofile, &out_filename, &asm_filename);
				break;

			/* toggle output file writing */
			case 'o':
				/* toggle global warning flag */
				out_flag ^= true;
				/* if file was open, close it and break early */
				if (out_flag)
					break;
				tbuf = strpbrk(strip, " \t");
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
				/* re-initiatalize compiler arg array */
				cc_argv = parse_opts(argc, argv, optstring, &ofile, &out_filename, &asm_filename);
				break;

			/* toggle library parsing */
			case 'p':
				free_buffers(&vl, &tl, &il, &prg, &lbuf);
				init_buffers(&vl, &tl, &il, &prg, &lbuf);
				/* toggle global parse flag */
				parse_flag ^= true;
				/* re-initiatalize compiler arg array */
				cc_argv = parse_opts(argc, argv, optstring, &ofile, &out_filename, &asm_filename);
				break;

			/* toggle variable tracking */
			case 't':
				free_buffers(&vl, &tl, &il, &prg, &lbuf);
				init_buffers(&vl, &tl, &il, &prg, &lbuf);
				/* toggle global parse flag */
				track_flag ^= true;
				/* re-initiatalize compiler arg array */
				cc_argv = parse_opts(argc, argv, optstring, &ofile, &out_filename, &asm_filename);
				break;

			/* toggle warnings */
			case 'w':
				free_buffers(&vl, &tl, &il, &prg, &lbuf);
				init_buffers(&vl, &tl, &il, &prg, &lbuf);
				/* toggle global warning flag */
				warn_flag ^= true;
				/* re-initiatalize compiler arg array */
				cc_argv = parse_opts(argc, argv, optstring, &ofile, &out_filename, &asm_filename);
				break;

			/* reset state */
			case 'r':
				free_buffers(&vl, &tl, &il, &prg, &lbuf);
				init_buffers(&vl, &tl, &il, &prg, &lbuf);
				/* re-initiatalize compiler arg array */
				cc_argv = parse_opts(argc, argv, optstring, &ofile, &out_filename, &asm_filename);
				break;

			/* define an include/macro/function */
			case 'm': /* fallthrough */
			case 'f':
				/* remove trailing ' ' and '\t' */
				for (size_t i = strlen(strip) - 1; i > 0; i--) {
					if (strip[i] != ' ' && strip[i] != '\t')
						break;
					strip[i] = '\0';
				}
				tbuf = strpbrk(strip, " \t");
				/* break if function definition empty */
				if (!tbuf || strspn(tbuf, " \t") == strlen(tbuf))
					break;
				/* increment pointer to start of definition */
				tbuf += strspn(tbuf, " \t");
				/* re-allocate enough memory for strip + '\n' + '\n' + '\0' */
				size_t s = strlen(tbuf) + 3;
				for (size_t i = 0; i < 2; i++) {
					rsz_buf(&prg[i].f, &prg[i].f_sz, &prg[i].f_max, s, &vl, &tl, &il, &prg, &tbuf);
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
					case '\\':
						/* remove extra trailing ';' */
						for (size_t j = strlen(tbuf) - 1; j > 0; j--) {
							if (tbuf[j] != ';' || tbuf[j - 1] != ';')
								break;
							tbuf[j] = '\0';
						}
						build_funcs(&prg, tbuf);
						for (size_t i = 0; i < 2; i++)
							strmv(CONCAT, prg[i].f, "\n");
						/* extract identifiers and types */
						if (track_flag && !strpbrk(tbuf, "()")) {
							/* remove final `;` */
							char *tmp_buf = strchr(tbuf, ';');
							if (tmp_buf)
								tmp_buf[0] = '\0';
							if (find_vars(tbuf, &il, &tl))
								gen_vlist(&vl, &il, &tl);
						}
						break;

					default:
						build_funcs(&prg, tbuf);
						/* append ';' if no trailing '}', ';', or '\' */
						for (size_t i = 0; i < 2; i++)
							strmv(CONCAT, prg[i].f, ";\n");
						/* extract identifiers and types */
						if (track_flag && !strpbrk(tbuf, "()")) {
							/* remove final `;` */
							char *tmp_buf = strchr(tbuf, ';');
							if (tmp_buf)
								tmp_buf[0] = '\0';
							if (find_vars(tbuf, &il, &tl))
								gen_vlist(&vl, &il, &tl);
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
			for (size_t i = strlen(strip) - 1; i > 0; i--) {
				if (strip[i] != ' ' && strip[i] != '\t')
					break;
				strip[i] = '\0';
			}
			/* start building program source */
			build_body(&prg, lptr);
			for (size_t i = 0; i < 2; i++)
				strmv(CONCAT, prg[i].b, "\n");
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
				/* extract identifiers and types */
				if (track_flag && find_vars(strip, &il, &tl))
					gen_vlist(&vl, &il, &tl);
				break;

			default:
				build_body(&prg, lptr);
				/* append ';' if no trailing '}', ';', or '\' */
				for (size_t i = 0; i < 2; i++)
					strmv(CONCAT, prg[i].b, ";\n");
				/* extract identifiers and types */
				if (track_flag && find_vars(strip, &il, &tl))
					gen_vlist(&vl, &il, &tl);
			}
		}

		/* finalize source */
		build_final(&prg, &vl, argv);
		/* print generated source code unless stdin is a pipe */
		if (isatty(STDIN_FILENO))
			printf("\n%s:\n==========\n%s\n==========\n", argv[0], prg[0].total);
		int ret = compile(prg[1].total, cc_argv, argv);
		/* print output and exit code if non-zero */
		if (ret || isatty(STDIN_FILENO))
			printf("[exit status: %d]\n", ret);
	}

	free_buffers(&vl, &tl, &il, &prg, &lbuf);
	cleanup();
	return 0;
}
