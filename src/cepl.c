/*
 * cepl.c - REPL translation unit
 *
 * AUTHOR: Joey Pabalinas <joeypabalinas@gmail.com>
 * See LICENSE file for copyright and license details.
 */

/* silence linter */
#undef _GNU_SOURCE
#define _GNU_SOURCE

#include "compile.h"
#include "errs.h"
#include "hist.h"
#include "parseopts.h"
#include "readline.h"
#include <setjmp.h>
#include <sys/stat.h>
#include <sys/types.h>

/* SIGINT buffer for non-local goto */
static sigjmp_buf jmp_env;
/* TODO: change history filename to a non-hardcoded string */
static char hist_name[] = "./.cepl_history";
/* global pointer for signal handler */
static struct program *prog_ptr;

/* string to compile */
extern char const *prologue, *prog_start, *prog_start_user, *prog_end;

static inline char *read_line(struct program *prog)
{
	/* false while waiting for input */
	prog->state_flags &= ~EXEC_FLAG;
	/* return early if executed with `-e` argument */
	if (prog->state_flags & EVAL_FLAG)
		return prog->cur_line = prog->eval_arg;
	/* use an empty prompt if stdin is a pipe */
	if (isatty(STDIN_FILENO))
		return prog->cur_line = readline(">>> ");
	/* redirect stdout to /dev/null */
	FILE *bitbucket;
	xfopen(&bitbucket, "/dev/null", "r+b");
	rl_outstream = bitbucket;
	prog->cur_line = readline(NULL);
	rl_outstream = NULL;
	fclose(bitbucket);
	return prog->cur_line;
}

static inline void undo_last_line(struct program *prog)
{
	/* break early if no history to pop */
	if (prog->src[0].flags.cnt < 1 || prog->src[1].flags.cnt < 1)
		return;
	pop_history(prog);
}

/* exit handler registration */
static inline void free_bufs(void)
{
	free_buffers(prog_ptr);
	cleanup(prog_ptr);
}

/* set io streams to non-buffering */
static inline void tty_break(struct program *prog)
{
	if (prog->tty_state.modes_changed)
		return;
	struct termios *cur_mode = prog->tty_state.save_modes;
	FILE *streams[] = {stdin, stdout, stderr, NULL};
	for (FILE **cur = streams; *cur; cur_mode++, cur++) {
		struct termios mod_modes = {0};
		int cur_fd = fileno(*cur);
		if (tcgetattr(cur_fd, cur_mode) < 0) {
#ifdef _DEBUG
			if (isatty(cur_fd))
				printe("%s\n", "tty_break()");
#endif
			continue;
		}
		mod_modes = *cur_mode;
		mod_modes.c_lflag &= ~ICANON;
		mod_modes.c_cc[VMIN] = 1;
		mod_modes.c_cc[VTIME] = 0;
		tcsetattr(cur_fd, TCSANOW, &mod_modes);
	}
	prog->tty_state.modes_changed = true;
}

/* reset attributes of standard io streams */
static inline void tty_fix(struct program *prog)
{
	if (prog->tty_state.modes_changed)
		return;
	struct termios *cur_mode = prog->tty_state.save_modes;
	FILE *streams[] = {stdin, stdout, stderr, NULL};
	for (FILE **cur = streams; *cur; cur_mode++, cur++)
		tcsetattr(fileno(*cur), TCSANOW, cur_mode);
	prog->tty_state.modes_changed = false;
}

/* general signal handling function */
static void sig_handler(int sig)
{
	static char const wtf[] = "wtf did you do to the signal mask to hit this return??\n";
	int ret;
	/* reset io stream buffering modes */
	tty_break(prog_ptr);
	tty_fix(prog_ptr);
	/* cleanup input line */
	free(prog_ptr->cur_line);
	prog_ptr->cur_line = NULL;
	/*
	 * the siglongjmp() here is needed in order to handle using
	 * ^C to both both clear the current command-line and also
	 * to abort running code early
	 */
	if (sig == SIGINT) {
		/*
		 * else abort current input line and
		 * siglongjmp() back to loop beginning
		 */
		if (prog_ptr->state_flags & EXEC_FLAG) {
			undo_last_line(prog_ptr);
			prog_ptr->state_flags &= ~EXEC_FLAG;
		}
		/* reap any leftover children */
		while (wait(&ret) >= 0 && errno != ECHILD);
		/* remove /tmp/cepl_program if it exists */
		unlink("/tmp/cepl_program");
		siglongjmp(jmp_env, 1);
	}
	/* cleanup and die if not SIGINT */
	free_buffers(prog_ptr);
	cleanup(prog_ptr);
	/* fallback to kill children who have masked SIGINT */
	kill(0, SIGKILL);
	/* reap any leftover children */
	while (wait(&ret) >= 0 && errno != ECHILD);
	raise(sig);
	/* wat */
	if (write(STDERR_FILENO, wtf, sizeof wtf) < 0)
	    ERR("%s", wtf);
	abort();
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
	struct sigaction sa[arr_len(sigs)];
	for (size_t i = 0; i < arr_len(sigs); i++) {
		sa[i].sa_handler = &sig_handler;
		sigemptyset(&sa[i].sa_mask);
		sa[i].sa_flags = SA_RESETHAND|SA_RESTART;
		/* don't reset SIGINT handler */
		if (sigs[i].sig == SIGINT)
			sa[i].sa_flags &= ~SA_RESETHAND;
		if (sigaction(sigs[i].sig, &sa[i], NULL) == -1)
			ERR("%s %s", sigs[i].sig_name, "sigaction()");
	}
	if (at_quick_exit(&free_bufs))
		WARN("at_quick_exit(&free_bufs)");
}

/* reset readline after signal */
static inline void reset_readline(void)
{
	int rl_flags = 0;

	/* setup readline */
	rl_flags |= RL_STATE_ISEARCH;
	rl_flags |= RL_STATE_NSEARCH;
	rl_flags |= RL_STATE_VIMOTION;
	rl_flags |= RL_STATE_NUMERICARG;
	rl_flags |= RL_STATE_MULTIKEY;
	rl_clear_visible_line();
	rl_reset_line_state();
	rl_free_line_state();
	rl_cleanup_after_signal();
	RL_UNSETSTATE(rl_flags);
	rl_line_buffer[rl_point = rl_end = rl_mark = 0] = 0;
	rl_initialize();
}

static inline void parse_function(struct program *prog)
{
	char *saved, *tmp_buf;
	/* remove trailing ' ' and '\t' */
	for (size_t i = strlen(prog->cur_line) - 1; i > 0; i--) {
		if (prog->cur_line[i] != ' ' && prog->cur_line[i] != '\t')
			break;
		prog->cur_line[i] = '\0';
	}
	tmp_buf = strpbrk(prog->cur_line, " \t");
	/* return if function definition empty */
	if (!tmp_buf || strspn(tmp_buf, " \t") == strlen(tmp_buf))
		return;
	saved = prog->cur_line;
	/* increment pointer to start of definition */
	tmp_buf += strspn(tmp_buf, " \t");
	/* re-allocate enough memory for program_state.cur_line + '\n' + '\n' + '\0' */
	size_t sz = strlen(tmp_buf) + 3;
	for (size_t i = 0; i < 2; i++) {
		/* keep line length to a minimum */
		resize_sect(prog, &prog->src[i].funcs, sz);
	}
	prog->cur_line = tmp_buf;

	switch (prog->cur_line[0]) {
	/* dont append ';' for preprocessor directives */
	case '#':
		/* remove trailing ' ' and '\t' */
		for (size_t i = strlen(prog->cur_line) - 1; i > 0; i--) {
			if (prog->cur_line[i] != ' ' && prog->cur_line[i] != '\t')
				break;
			prog->cur_line[i] = '\0';
		}
		build_funcs(prog);
		for (size_t i = 0; i < 2; i++)
			strmv(CONCAT, prog->src[i].funcs.buf, "\n");
		break;

	default:
		/* remove trailing ' ' and '\t' */
		for (size_t i = strlen(prog->cur_line) - 1; i > 0; i--) {
			if (prog->cur_line[i] != ' ' && prog->cur_line[i] != '\t')
				break;
			prog->cur_line[i] = '\0';
		}

		switch(prog->cur_line[strlen(prog->cur_line) - 1]) {
		case '{': /* fallthough */
		case '}': /* fallthough */
		case ';': /* fallthough */
		case '\\':
			/* remove extra trailing ';' */
			for (size_t j = strlen(prog->cur_line) - 1; j > 0; j--) {
				if (prog->cur_line[j] != ';' || prog->cur_line[j - 1] != ';')
					break;
				prog->cur_line[j] = '\0';
			}
			build_funcs(prog);
			for (size_t i = 0; i < 2; i++)
				strmv(CONCAT, prog->src[i].funcs.buf, "\n");
			break;

		default:
			build_funcs(prog);
			/* append ';' if no trailing '}', ';', or '\' */
			for (size_t i = 0; i < 2; i++)
				strmv(CONCAT, prog->src[i].funcs.buf, ";\n");
		}
	}
	prog->cur_line = saved;
}

static inline void parse_normal(struct program *prog)
{
	/* remove trailing ' ' and '\t' */
	for (size_t i = strlen(prog->cur_line) - 1; i > 0; i--) {
		if (prog->cur_line[i] != ' ' && prog->cur_line[i] != '\t')
			break;
		prog->cur_line[i] = '\0';
	}
	switch(prog->cur_line[strlen(prog->cur_line) - 1]) {
	case '{': /* fallthough */
	case '}': /* fallthough */
	case ';': /* fallthough */
	case '\\': {
			build_body(prog);
			for (size_t i = 0; i < 2; i++) {
				/* remove extra trailing ';' */
				size_t b_len = strlen(prog->src[i].body.buf) - 1;
				for (size_t j = b_len; j > 0; j--) {
					if (prog->src[i].body.buf[j] != ';' || prog->src[i].body.buf[j - 1] != ';')
						break;
					prog->src[i].body.buf[j] = '\0';
				}
				strmv(CONCAT, prog->src[i].body.buf, "\n");
			}
			break;
		   }

	default: {
			build_body(prog);
			/* append ';' if no trailing '}', ';', or '\' */
			for (size_t i = 0; i < 2; i++)
				strmv(CONCAT, prog->src[i].body.buf, ";\n");
		}
	}
}

static inline void build_hist_name(struct program *prog)
{
	struct stat hist_stat;
	size_t buf_sz = sizeof hist_name, hist_len = 0;
	char const *const home_env = getenv("HOME");
	FILE *make_hist = NULL;

	/* return early if $HOME is unset or zero-length */
	if (!home_env || !strcmp(home_env, ""))
		return;

	/* add hist_length of “$HOME/” */
	buf_sz += strlen(home_env) + 1;

	/* prepend "~/" to history fihist_lename ("~/.cepl_history" by default) */
	if (!(prog->hist_file = calloc(1, buf_sz)))
		ERR("program_state.hist_file malloc()");
	/* check if home_env is non-NULL */
	if (home_env && strcmp(home_env, "")) {
		hist_len = strlen(home_env);
		strmv(0, prog->hist_file, home_env);
		prog->hist_file[hist_len++] = '/';
	}
	strmv(hist_len, prog->hist_file, hist_name);

	/* initialize history sesssion */
	using_history();
	prog->state_flags |= HIST_FLAG;
	/* read program_state.hist_file if size is non-zero */
	stat(prog->hist_file, &hist_stat);
	if (hist_stat.st_size > 0) {
		if (read_history(prog->hist_file)) {
			char hist_pre[] = "error reading history from ";
			char hist_full[sizeof hist_pre + strlen(prog->hist_file)];
			char *hist_ptr = hist_full;
			strmv(0, hist_ptr, hist_pre);
			strmv(sizeof hist_pre - 1, hist_ptr, prog->hist_file);
			WARN("%s", hist_ptr);
		}
	}
}

static inline void show_man(const char *query)
{
	int ret;
	char *split;
	struct str_list man_args;
	init_str_list(&man_args, "man");
	/* skip ;m[an] */
	query = strpbrk(query, " \t");
	xmalloc(&split, strlen(query) +1, "split");
	strmv(0, split, query);
	/* split query on whitespace */
	for (char *arg = strtok(split, " \t"); arg; arg = strtok(NULL, " \t"))
		append_str(&man_args, arg, 0);
	free(split);
	/* show man <query> */
	switch (fork()) {
	case -1:
		ERR("show_man() fork()");
	case 0:
		execvp("man", man_args.list);
		ERR("show_man() execvp()");
	default:
		wait(&ret);
	}
}

int main(int argc, char **argv)
{
	/*
	 * program source struct (program_state.src[0]
	 * is truncated for interactive printing)
	 */
	static struct program program_state;
	char const *const optstring = "hpvwa:c:e:o:l:s:I:L:";

	/* set global pointer for signal handler */
	prog_ptr = &program_state;

	/* set default state flags */
	program_state.state_flags = PARSE_FLAG;
	build_hist_name(&program_state);

	/* enable readline completion */
	rl_completion_entry_function = &generator;
	rl_attempted_completion_function = &completer;
	rl_basic_word_break_characters = " \t\n\"\\'`@$><=|&{}()[]";
	rl_completion_suppress_append = 1;
	rl_bind_key('\t', &rl_complete);

	/* parse commandline options */
	parse_opts(&program_state, argc, argv, optstring);
	init_buffers(&program_state);
	/*
	 * initialize program_state.src[0].total
	 * and program_state.src[1].total then
	 * print version
	 */
	build_final(&program_state, argv);
	if (isatty(STDIN_FILENO) && !(program_state.state_flags & EVAL_FLAG))
		fprintf(stdout, "%s\n", VERSION_STRING);
	reg_handlers();
	rl_set_signals();

	/*
	 * the siglongjmp() here is needed in order to handle using
	 * ^C to both both clear the current command-line and also
	 * to abort running code early
	 */
	if (sigsetjmp(jmp_env, 1)) {
		reset_readline();
		fputc('\n', stdout);
	}

	/* loop readline() until EOF is read */
	while (read_line(&program_state)) {
		/* if all whitespace (non-state commands) or empty read a new line */
		char *stripped = program_state.cur_line;
		if (!*program_state.cur_line)
			continue;
		/* set io streams to non-buffering */
		tty_break(&program_state);
		/* re-enable completion if disabled */
		rl_bind_key('\t', &rl_complete);
		dedup_history_add(&program_state.cur_line);
		/* re-allocate enough memory for line + '\t' + ';' + '\n' + '\0' */
		for (size_t i = 0; i < 2; i++) {
			/* keep line length to a minimum */
			resize_sect(&program_state, &program_state.src[i].body, 3);
			resize_sect(&program_state, &program_state.src[i].total, 3);
		}
		stripped = program_state.cur_line;
		stripped += strspn(stripped, " \t");

		/* control sequence and preprocessor directive parsing */
		switch (stripped[0]) {
		case ';':
			switch(stripped[1]) {
			/* show documentation about argument */
			case 'm':
				show_man(stripped);
				break;

			/* pop last history statement */
			case 'u':
				undo_last_line(&program_state);
				break;

			/* reset state */
			case 'r':
				free_buffers(&program_state);
				parse_opts(&program_state, argc, argv, optstring);
				init_buffers(&program_state);
				break;

			/* define an include/macro/function */
			case 'f':
				parse_function(&program_state);
				break;

			/* show usage information */
			case 'h':
				fprintf(stdout, "%s %s %s\n", "Usage:", argv[0], USAGE_STRING);
				break;

			/* clean up and exit program */
			case 'q':
				free_buffers(&program_state);
				cleanup(&program_state);
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
			for (size_t i = strlen(stripped) - 1; i > 0; i--) {
				if (stripped[i] != ' ' && stripped[i] != '\t')
					break;
				stripped[i] = '\0';
			}
			/* start building program source */
			build_body(&program_state);
			for (size_t i = 0; i < 2; i++)
				strmv(CONCAT, program_state.src[i].body.buf, "\n");
			break;

		default:
			parse_normal(&program_state);
		}

		/* set to true before compiling */
		program_state.state_flags |= EXEC_FLAG;
		/* finalize source */
		build_final(&program_state, argv);
		/* print generated source code unless stdin is a pipe */
		if (isatty(STDIN_FILENO) && !(program_state.state_flags & EVAL_FLAG)) {
			fprintf(stdout, "%s:\n", argv[0]);
			fprintf(stdout, "==========\n");
			fprintf(stdout, "%s\n", program_state.src[0].total.buf);
			fprintf(stdout, "==========\n");
		}
		int ret = compile(program_state.src[1].total.buf, program_state.cc_list.list, true);
		/* print output and exit code if non-zero */
		if (ret || (isatty(STDIN_FILENO) && !(program_state.state_flags & EVAL_FLAG)))
			fprintf(stdout, "[exit status: %d]\n", ret);

		/* reset io stream buffering modes */
		tty_fix(&program_state);

		/* exit if executed with `-e` argument */
		if (program_state.state_flags & EVAL_FLAG) {
			/* don't call free() since this points to eval_arg[0] */
			program_state.cur_line = NULL;
			break;
		}
		/* cleanup old buffer */
		free(program_state.cur_line);
		program_state.cur_line = NULL;
	}

	free_buffers(&program_state);
	cleanup(&program_state);
	return 0;
}
