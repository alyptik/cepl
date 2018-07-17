/*
 * cepl.c - REPL translation unit
 *
 * AUTHOR: Joey Pabalinas <joeypabalinas@gmail.com>
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

/* SIGINT buffer for non-local goto */
static sigjmp_buf jmp_env;
/* TODO: change history filename to a non-hardcoded string */
static char hist_name[] = "./.cepl_history";
/*
 * program source strucs (program_state.src[0] is
 * truncated for interactive printing)
 */
static struct program program_state;

/* string to compile */
extern char const *prelude, *prog_start, *prog_start_user, *prog_end;
extern enum asm_type asm_dialect;

static inline char *read_line(struct program *restrict prog)
{
	/* false while waiting for input */
	prog->sflags.exec_flag = false;
	/* return early if executed with `-e` argument */
	if (prog->sflags.eval_flag)
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

static inline void init_vars(void)
{
	if (program_state.type_list.list) {
		free(program_state.type_list.list);
		program_state.type_list.list = NULL;
	}
	if (program_state.var_list.list) {
		for (size_t i = 0; i < program_state.var_list.cnt; i++)
			free(program_state.var_list.list[i].id);
		free(program_state.var_list.list);
	}
	init_var_list(&program_state.var_list);
}

static inline void undo_last_line(void)
{
	/* break early if no history to pop */
	if (program_state.src[0].flags.cnt < 1 || program_state.src[1].flags.cnt < 1)
		return;
	pop_history(&program_state);
	/* break early if tracking disabled */
	if (!program_state.sflags.track_flag)
		return;
	init_vars();
	/* add vars from previous lines */
	for (size_t i = 1; i < program_state.src[0].lines.cnt; i++) {
		if (program_state.src[0].lines.list[i] && program_state.src[0].flags.list[i] == IN_MAIN) {
			if (find_vars(&program_state, program_state.src[0].lines.list[i]))
				gen_var_list(&program_state);
		}
	}
}

/* exit handler registration */
static inline void free_bufs(void)
{
	free_buffers(&program_state);
	cleanup(&program_state);
}

/* set io streams to non-buffering */
static inline void tty_break(struct program *restrict prg)
{
	if (prg->tty_state.modes_changed)
		return;
	struct termio *cur_mode = prg->tty_state.save_modes;
	FILE *streams[] = {stdin, stdout, stderr, NULL};
	for (FILE **cur = streams; *cur; cur_mode++, cur++) {
		struct termio mod_modes = {0};
		int cur_fd = fileno(*cur);
		if (ioctl(cur_fd, TCGETA, cur_mode) < 0) {
#ifdef _DEBUG
			if (isatty(cur_fd))
				DPRINTF("%s\n", "tty_break()");
#endif
			continue;
		}
		mod_modes = *cur_mode;
		mod_modes.c_lflag &= ~ICANON;
		mod_modes.c_cc[VMIN] = 1;
		mod_modes.c_cc[VTIME] = 0;
		ioctl(cur_fd, TCSETAW, &mod_modes);
	}
	prg->tty_state.modes_changed = true;
}

/* reset attributes of standard io streams */
static inline void tty_fix(struct program *restrict prg)
{
	if (prg->tty_state.modes_changed)
		return;
	struct termio *cur_mode = prg->tty_state.save_modes;
	FILE *streams[] = {stdin, stdout, stderr, NULL};
	for (FILE **cur = streams; *cur; cur_mode++, cur++)
		ioctl(fileno(*cur), TCSETAW, cur_mode);
	prg->tty_state.modes_changed = false;
}

/* general signal handling function */
static void sig_handler(int sig)
{
	static char const wtf[] = "wtf did you do to the signal mask to hit this return???\n";
	int ret;
	/*
	 * TODO (?):
	 *
	 * this relies on stderr never being fd 0, unsure
	 * if worth caring about the corner case where stderr
	 * is 0 (possible but *very* unlikely).
	 */
	if (program_state.saved_fd)
		dup2(program_state.saved_fd, STDOUT_FILENO);
	/* reset io stream buffering modes */
	tty_break(&program_state);
	tty_fix(&program_state);
	/* cleanup input line */
	free(program_state.cur_line);
	program_state.cur_line = NULL;
	/* fallback to kill children who have masked SIGINT */
	kill(0, SIGKILL);
	/* reap any leftover children */
	while (wait(&ret) >= 0 && errno != ECHILD);
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
		if (program_state.sflags.exec_flag) {
			undo_last_line();
			program_state.sflags.exec_flag = false;
		}
		siglongjmp(jmp_env, 1);
	}
	/* cleanup and die if not SIGINT */
	free_buffers(&program_state);
	cleanup(&program_state);
	raise(sig);
	/* wat */
	if (write(STDERR_FILENO, wtf, sizeof wtf) < 0)
	    ERR("%s\n", wtf);
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
	struct sigaction sa[ARR_LEN(sigs)];
	for (size_t i = 0; i < ARR_LEN(sigs); i++) {
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
		WARN("%s", "at_quick_exit(&free_bufs)");
}

static inline char *gen_bin_str(char const *restrict in_str)
{
	size_t in_len = 0, cnt = 0, num_octets = 0;
	char base_arr[65] = {0}, *base_ptr = base_arr, *end_ptr;
	char rev_arr[sizeof base_arr + sizeof base_arr / 8] = {0}, *rev_ptr = rev_arr;
	static char final_array[sizeof rev_arr], *final_ptr = NULL;

	/* return early if NULL or empty string */
	if (!in_str || !(in_len = strlen(in_str))) {
#ifdef _DEBUG
		DPRINTF("%s", "NULL or empty string passed to gen_bin_str()");
#endif
		return "";
	}
	/* reset for ERANGE / EINVAL check */
	errno = 0;
	unsigned long long num = strtoll(in_str, &end_ptr, 0);
#ifdef _DEBUG
	DPRINTF("endptr: \"%s\"\n*endptr = '%c'\n", end_ptr, *end_ptr);
#endif
	/* return empty string on parse error */
	if (errno || !in_len || (*end_ptr && (strspn(end_ptr, " \t;") != strlen(end_ptr))))
		return "";

	/* build base binary string */
	while (num) {
		for (long long i = 0; i < ffsll(num) - 1; i++) {
			sprintf(base_ptr++, "%c", '0');
			num >>= 1;
			cnt++;
		}
		sprintf(base_ptr++, "%c", '1');
		num >>= 1;
		cnt++;
	}

	/* zero fill and reverse the digits */
	for (; cnt % 8; cnt++)
		sprintf(base_ptr++, "%c", '0');
	for (size_t i = 0, j = cnt - 1; i < cnt; i++, j--)
		rev_arr[j] = base_arr[i];

	/* convert it to "0b_0000000_000..." format */
	memset(final_array, 0, sizeof final_array);
	final_ptr = final_array + 2;
	sprintf(final_array, "%s", "0b");
	num_octets = strlen(rev_arr) / 8;
	for (size_t i = 0; i < num_octets; i++) {
		sprintf(final_ptr, "%c%.8s", '_', rev_ptr);
		rev_ptr += 8;
		final_ptr += 9;
	}

#ifdef _DEBUG
	DPRINTF("[%zu] %s - %s - %s\n", 8 - (cnt % 8), base_arr, rev_arr, final_array);
#endif
	/* return empty string on error */
	if (strlen(final_array) < 3)
		return "";

	return final_array;
}

static void eval_line(int argc, char **restrict argv, char const *restrict optstring)
{
	/* return early if line is a cepl command */
	if (program_state.cur_line && *program_state.cur_line == ';')
		return;

	char const *const term = getenv("TERM");
	struct program prg = {0};
	struct str_list temp = strsplit(program_state.cur_line);
	bool has_color = term
		&& isatty(STDOUT_FILENO)
		&& isatty(STDERR_FILENO)
		&& strcmp(term, "")
		&& strcmp(term, "dumb");
	char const *const ln_beg = has_color
		? "fprintf(stderr, \"" YELLOW "%s[%lld, %#llx%s\\n" RST "\", \"result = \", "
		: "fprintf(stderr, \"%s[%lld, %#llx%s\\n\", \"result = \", ";
	char const *const ln_long[] = {"(long long)(", "), "};
	char const *const ln_hex[] = {"(unsigned long long)(", "), \""};
	char const *const ln_end = "\");";

	/* bit bucket */
	parse_opts(&prg, argc, argv, optstring);
	init_buffers(&prg);
	build_final(&prg, argv);
	/* don't write out files for line evaluation */
	prg.sflags = (struct state_flags){0};

	for (size_t i = 0; i < temp.cnt; i++) {
		char const *const ln_bin = gen_bin_str(temp.list[i]);
		char const *const ln_bin_pre = strlen(ln_bin) ? ", " : "";
		char const *const ln_bin_end = "]";
		size_t sz = 1 + strlen(ln_beg) + strlen(ln_end)
			+ strlen(ln_long[0]) + strlen(ln_long[1])
			+ strlen(ln_hex[0]) + strlen(ln_hex[1])
			+ strlen(ln_bin_pre) + strlen(ln_bin) + strlen(ln_bin_end)
			+ strlen(temp.list[i]) * 2;
		/* initialize source buffers */
		xcalloc(char, &prg.cur_line, 1, sz, "eval_line() calloc");
		sprintf(prg.cur_line, "%s%s%s%s%s%s%s%s%s%s%s", ln_beg,
				ln_long[0], temp.list[i], ln_long[1],
				ln_hex[0], temp.list[i], ln_hex[1],
				ln_bin_pre, ln_bin, ln_bin_end,
				ln_end);
#ifdef _DEBUG
		DPRINTF("eval_line(): \"%s\"\n", prg.cur_line);
#endif
		for (size_t j = 0; j < 2; j++) {
			resize_sect(&prg, &prg.src[j].body, sz);
			resize_sect(&prg, &prg.src[j].total, sz);
		}
		/* extract identifiers and types */
		if (temp.list[i][0] != ';' && !find_vars(&prg, temp.list[i])) {
			build_body(&prg);
			build_final(&prg, argv);
		}
	}

	/* print line evaluation */
	int null_fd;
	if ((null_fd = open("/dev/null", O_WRONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) == -1)
		ERR("%s", "open()");
	dup2(null_fd, STDOUT_FILENO);
	compile(prg.src[1].total.buf, program_state.cc_list.list, argv, false);
	free_buffers(&prg);
	free_str_list(&temp);
	dup2(program_state.saved_fd, STDOUT_FILENO);
	close(null_fd);
}

static inline void toggle_att(char *tbuf)
{
	/* if file was open, flip it and break early */
	if (program_state.sflags.asm_flag) {
		program_state.sflags.asm_flag ^= true;
		return;
	}
	program_state.sflags.asm_flag ^= true;
	tbuf = strpbrk(program_state.cur_line, " \t");
	/* return if file name empty */
	if (!tbuf || strspn(tbuf, " \t") == strlen(tbuf)) {
		/* reset flag */
		program_state.sflags.asm_flag ^= true;
		return;
	}
	/* increment pointer to start of definition */
	tbuf += strspn(tbuf, " \t");
	if (program_state.asm_filename) {
		free(program_state.asm_filename);
		program_state.asm_filename = NULL;
	}
	if (!(program_state.asm_filename = calloc(1, strlen(tbuf) + 1)))
		ERR("%s", "error during program_state.asm_filename calloc()");
	strmv(0, program_state.asm_filename, tbuf);
	free_buffers(&program_state);
	init_buffers(&program_state);
	asm_dialect = ATT;
}

static inline void toggle_intel(char *tbuf)
{
	/* if file was open, flip it and break early */
	if (program_state.sflags.asm_flag) {
		program_state.sflags.asm_flag ^= true;
		return;
	}
	program_state.sflags.asm_flag ^= true;
	tbuf = strpbrk(program_state.cur_line, " \t");
	/* return if file name empty */
	if (!tbuf || strspn(tbuf, " \t") == strlen(tbuf)) {
		/* reset flag */
		program_state.sflags.asm_flag ^= true;
		return;
	}
	/* increment pointer to start of definition */
	tbuf += strspn(tbuf, " \t");
	if (program_state.asm_filename) {
		free(program_state.asm_filename);
		program_state.asm_filename = NULL;
	}
	if (!(program_state.asm_filename = calloc(1, strlen(tbuf) + 1)))
		ERR("%s", "error during program_state.asm_filename calloc()");
	strmv(0, program_state.asm_filename, tbuf);
	free_buffers(&program_state);
	init_buffers(&program_state);
	asm_dialect = INTEL;
}

static inline void toggle_output_file(char *tbuf)
{
	/* if file was open, flip it and break early */
	if (program_state.sflags.out_flag) {
		program_state.sflags.out_flag ^= true;
		return;
	}
	program_state.sflags.out_flag ^= true;
	tbuf = strpbrk(program_state.cur_line, " \t");
	/* return if file name empty */
	if (!tbuf || strspn(tbuf, " \t") == strlen(tbuf)) {
		/* reset flag */
		program_state.sflags.out_flag ^= true;
		return;
	}
	/* increment pointer to start of definition */
	tbuf += strspn(tbuf, " \t");
	if (program_state.out_filename) {
		free(program_state.out_filename);
		program_state.out_filename = NULL;
	}
	xcalloc(char, &program_state.out_filename, 1, strlen(tbuf) + 1, "program_state.out_filename calloc()");
	strmv(0, program_state.out_filename, tbuf);
	free_buffers(&program_state);
	init_buffers(&program_state);
}

static inline void parse_macro(void)
{
	struct str_list tmp_list;
	char *saved, *tmp_buf;
	/* remove trailing ' ' and '\t' */
	for (size_t i = strlen(program_state.cur_line) - 1; i > 0; i--) {
		if (program_state.cur_line[i] != ' ' && program_state.cur_line[i] != '\t')
			break;
		program_state.cur_line[i] = '\0';
	}
	tmp_buf = strpbrk(program_state.cur_line, " \t");
	/* return if function definition empty */
	if (!tmp_buf || strspn(tmp_buf, " \t") == strlen(tmp_buf))
		return;
	saved = program_state.cur_line;
	/* increment pointer to start of definition */
	tmp_buf += strspn(tmp_buf, " \t");
	/* re-allocate enough memory for program_state.cur_line + '\n' + '\n' + '\0' */
	size_t sz = strlen(tmp_buf) + 3;
	for (size_t i = 0; i < 2; i++) {
		/* keep line length to a minimum */
		struct program *prg = &program_state;
		resize_sect(prg, &prg->src[i].funcs, sz);
	}
	program_state.cur_line = tmp_buf;

	switch (program_state.cur_line[0]) {
	/* dont append ';' for preprocessor directives */
	case '#':
		/* remove trailing ' ' and '\t' */
		for (size_t i = strlen(program_state.cur_line) - 1; i > 0; i--) {
			if (program_state.cur_line[i] != ' ' && program_state.cur_line[i] != '\t')
				break;
			program_state.cur_line[i] = '\0';
		}
		build_funcs(&program_state);
		for (size_t i = 0; i < 2; i++)
			strmv(CONCAT, program_state.src[i].funcs.buf, "\n");
		break;

	default:
		/* remove trailing ' ' and '\t' */
		for (size_t i = strlen(program_state.cur_line) - 1; i > 0; i--) {
			if (program_state.cur_line[i] != ' ' && program_state.cur_line[i] != '\t')
				break;
			program_state.cur_line[i] = '\0';
		}

		switch(program_state.cur_line[strlen(program_state.cur_line) - 1]) {
		case '{': /* fallthough */
		case '}': /* fallthough */
		case ';': /* fallthough */
		case '\\':
			/* remove extra trailing ';' */
			for (size_t j = strlen(program_state.cur_line) - 1; j > 0; j--) {
				if (program_state.cur_line[j] != ';' || program_state.cur_line[j - 1] != ';')
					break;
				program_state.cur_line[j] = '\0';
			}
			build_funcs(&program_state);
			for (size_t i = 0; i < 2; i++)
				strmv(CONCAT, program_state.src[i].funcs.buf, "\n");

			tmp_list = strsplit(program_state.cur_line);
			for (size_t i = 0; i < tmp_list.cnt; i++) {
				/* extract identifiers and types */
				if (program_state.sflags.track_flag && find_vars(&program_state, tmp_list.list[i]))
					gen_var_list(&program_state);
			}
			free_str_list(&tmp_list);
			break;

		default:
			build_funcs(&program_state);
			/* append ';' if no trailing '}', ';', or '\' */
			for (size_t i = 0; i < 2; i++)
				strmv(CONCAT, program_state.src[i].funcs.buf, ";\n");
			tmp_list = strsplit(program_state.cur_line);
			for (size_t i = 0; i < tmp_list.cnt; i++) {
				/* extract identifiers and types */
				if (program_state.sflags.track_flag && find_vars(&program_state, tmp_list.list[i]))
					gen_var_list(&program_state);
			}
			free_str_list(&tmp_list);
		}
	}
	program_state.cur_line = saved;
}

static inline void parse_normal(void)
{
	/* remove trailing ' ' and '\t' */
	for (size_t i = strlen(program_state.cur_line) - 1; i > 0; i--) {
		if (program_state.cur_line[i] != ' ' && program_state.cur_line[i] != '\t')
			break;
		program_state.cur_line[i] = '\0';
	}
	switch(program_state.cur_line[strlen(program_state.cur_line) - 1]) {
	case '{': /* fallthough */
	case '}': /* fallthough */
	case ';': /* fallthough */
	case '\\': {
			build_body(&program_state);
			for (size_t i = 0; i < 2; i++) {
				/* keep line length to a minimum */
				struct program *prg = &program_state;
				/* remove extra trailing ';' */
				size_t b_len = strlen(prg->src[i].body.buf) - 1;
				for (size_t j = b_len; j > 0; j--) {
					if (prg->src[i].body.buf[j] != ';' || prg->src[i].body.buf[j - 1] != ';')
						break;
					prg->src[i].body.buf[j] = '\0';
				}
				strmv(CONCAT, prg->src[i].body.buf, "\n");
			}
			struct str_list tmp = strsplit(program_state.cur_line);
			for (size_t i = 0; i < tmp.cnt; i++) {
				/* extract identifiers and types */
				if (program_state.sflags.track_flag && find_vars(&program_state, tmp.list[i]))
					gen_var_list(&program_state);
			}
			free_str_list(&tmp);
			break;
		   }

	default: {
			build_body(&program_state);
			/* append ';' if no trailing '}', ';', or '\' */
			for (size_t i = 0; i < 2; i++)
				strmv(CONCAT, program_state.src[i].body.buf, ";\n");
			struct str_list tmp = strsplit(program_state.cur_line);
			for (size_t i = 0; i < tmp.cnt; i++) {
				/* extract identifiers and types */
				if (program_state.sflags.track_flag && find_vars(&program_state, tmp.list[i]))
					gen_var_list(&program_state);
			}
			free_str_list(&tmp);
		}
	}
}

/* parse input file if one is specified */
static inline void scan_input_file(void)
{
	if (!program_state.sflags.in_flag)
		return;
	init_vars();
	char *prog_buf = strchr(prog_start_user, '{');
	if (!prog_buf)
		return;
	if (program_state.sflags.track_flag && find_vars(&program_state, ++prog_buf))
		gen_var_list(&program_state);
}

static inline void build_hist_name(void)
{
	struct stat hist_stat;
	size_t buf_sz = sizeof hist_name, hist_len = 0;
	char const *const home_env = getenv("HOME");
	FILE *make_hist = NULL;

	/* add hist_length of “$HOME/” if home_env is non-NULL */
	if (home_env && strcmp(home_env, ""))
		buf_sz += strlen(home_env) + 1;
	/* prepend "~/" to history fihist_lename ("~/.cepl_history" by default) */
	if (!(program_state.hist_file = calloc(1, buf_sz)))
		ERR("%s", "program_state.hist_file malloc()");
	/* check if home_env is non-NULL */
	if (home_env && strcmp(home_env, "")) {
		hist_len = strlen(home_env);
		strmv(0, program_state.hist_file, home_env);
		program_state.hist_file[hist_len++] = '/';
	}
	strmv(hist_len, program_state.hist_file, hist_name);

	/* enable completion */
	rl_completion_entry_function = &generator;
	rl_attempted_completion_function = &completer;
	rl_basic_word_break_characters = " \t\n\"\\'`@$><=|&{}()[]";
	rl_completion_suppress_append = 1;
	rl_bind_key('\t', &rl_complete);

	/* initialize history sesssion */
	using_history();
	/* create history file if it doesn't exsit */
	if (!(make_hist = fopen(program_state.hist_file, "ab"))) {
		WARN("%s", "error creating history file with fopen()");
	} else {
		fclose(make_hist);
		program_state.sflags.hist_flag = true;
	}
	/* read program_state.hist_file if size is non-zero */
	stat(program_state.hist_file, &hist_stat);
	if (hist_stat.st_size > 0) {
		if (read_history(program_state.hist_file)) {
			char hist_pre[] = "error reading history from ";
			char hist_full[sizeof hist_pre + strlen(program_state.hist_file)];
			char *hist_ptr = hist_full;
			strmv(0, hist_ptr, hist_pre);
			strmv(sizeof hist_pre - 1, hist_ptr, program_state.hist_file);
			WARN("%s", hist_ptr);
		}
	}
}

static inline void save_flag_state(struct state_flags *restrict sflags)
{
	sflags->asm_flag = program_state.sflags.asm_flag;
	sflags->eval_flag = program_state.sflags.eval_flag;
	sflags->exec_flag = program_state.sflags.exec_flag;
	sflags->in_flag = program_state.sflags.in_flag;
	sflags->out_flag = program_state.sflags.out_flag;
	sflags->parse_flag = program_state.sflags.parse_flag;
	sflags->track_flag = program_state.sflags.track_flag;
	sflags->warn_flag = program_state.sflags.warn_flag;
}

static inline void restore_flag_state(struct state_flags *restrict sflags)
{
	program_state.sflags.asm_flag = sflags->asm_flag;
	program_state.sflags.eval_flag = sflags->eval_flag;
	program_state.sflags.exec_flag = sflags->exec_flag;
	program_state.sflags.in_flag = sflags->in_flag;
	program_state.sflags.out_flag = sflags->out_flag;
	program_state.sflags.parse_flag = sflags->parse_flag;
	program_state.sflags.track_flag = sflags->track_flag;
	program_state.sflags.warn_flag = sflags->warn_flag;
}

int main(int argc, char **argv)
{
	struct state_flags saved_flags = STATE_FLAG_DEF_INIT;
	char const *const optstring = "hptvwc:a:f:e:i:l:I:o:";

	/* initialize compiler arg array */
	build_hist_name();
	save_flag_state(&saved_flags);
	parse_opts(&program_state, argc, argv, optstring);
	init_buffers(&program_state);
	scan_input_file();
	/* save stderr for signal handler */
	program_state.saved_fd = dup(STDERR_FILENO);
	/*
	 * initialize program_state.src[0].total
	 * and program_state.src[1].total then
	 * print version
	 */
	build_final(&program_state, argv);
	if (isatty(STDIN_FILENO) && !program_state.sflags.eval_flag)
		fprintf(stderr, "%s\n", VERSION_STRING);
	reg_handlers();
	rl_set_signals();

	/*
	 * the siglongjmp() here is needed in order to handle using
	 * ^C to both both clear the current command-line and also
	 * to abort running code early
	 */
	if (sigsetjmp(jmp_env, 1)) {
		int rl_flags = 0;
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
		fputc('\n', stderr);
	}

	/* loop readline() until EOF is read */
	while (read_line(&program_state)) {
		/* if all whitespace (non-state commands) or empty read a new line */
		char *stripped = program_state.cur_line;
		if (!*program_state.cur_line)
			continue;
		stripped += strspn(stripped, " \t;");
		if (!*stripped && *program_state.cur_line != ';')
			continue;
		/* set io streams to non-buffering */
		tty_break(&program_state);
		/* re-enable completion if disabled */
		rl_bind_key('\t', &rl_complete);
		dedup_history_add(&program_state.cur_line);
		/* re-allocate enough memory for line + '\t' + ';' + '\n' + '\0' */
		for (size_t i = 0; i < 2; i++) {
			/* keep line length to a minimum */
			struct program *prg = &program_state;
			resize_sect(prg, &prg->src[i].body, 3);
			resize_sect(prg, &prg->src[i].total, 3);
		}
		stripped = program_state.cur_line;
		stripped += strspn(stripped, " \t");
		eval_line(argc, argv, optstring);

		/* control sequence and preprocessor directive parsing */
		switch (stripped[0]) {
		case ';':
			switch(stripped[1]) {
			/* pop last history statement */
			case 'u':
				undo_last_line();
				break;

			/* toggle writing at&t-dialect asm output */
			case 'a':
				restore_flag_state(&saved_flags);
				toggle_att(stripped);
				save_flag_state(&saved_flags);
				parse_opts(&program_state, argc, argv, optstring);
				break;

			/* toggle writing intel-dialect asm output */
			case 'i':
				restore_flag_state(&saved_flags);
				toggle_intel(stripped);
				save_flag_state(&saved_flags);
				parse_opts(&program_state, argc, argv, optstring);
				break;

			/* toggle output file writing */
			case 'o':
				restore_flag_state(&saved_flags);
				toggle_output_file(stripped);
				save_flag_state(&saved_flags);
				parse_opts(&program_state, argc, argv, optstring);
				break;

			/* toggle library parsing */
			case 'p':
				free_buffers(&program_state);
				init_buffers(&program_state);
				restore_flag_state(&saved_flags);
				program_state.sflags.parse_flag ^= true;
				save_flag_state(&saved_flags);
				parse_opts(&program_state, argc, argv, optstring);
				break;

			/* toggle variable tracking */
			case 't':
				free_buffers(&program_state);
				init_buffers(&program_state);
				restore_flag_state(&saved_flags);
				program_state.sflags.track_flag ^= true;
				save_flag_state(&saved_flags);
				parse_opts(&program_state, argc, argv, optstring);
				break;

			/* toggle warnings */
			case 'w':
				free_buffers(&program_state);
				init_buffers(&program_state);
				restore_flag_state(&saved_flags);
				program_state.sflags.warn_flag ^= true;
				save_flag_state(&saved_flags);
				parse_opts(&program_state, argc, argv, optstring);
				break;

			/* reset state */
			case 'r':
				free_buffers(&program_state);
				init_buffers(&program_state);
				restore_flag_state(&saved_flags);
				save_flag_state(&saved_flags);
				parse_opts(&program_state, argc, argv, optstring);
				scan_input_file();
				break;

			/* define an include/macro/function */
			case 'm': /* fallthrough */
			case 'f':
				parse_macro();
				break;

			/* show usage information */
			case 'h':
				fprintf(stderr, "%s %s %s\n", "Usage:", argv[0], USAGE_STRING);
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
			parse_normal();
		}

		/* set to true before compiling */
		program_state.sflags.exec_flag = true;
		/* finalize source */
		build_final(&program_state, argv);
		/* print generated source code unless stdin is a pipe */
		if (isatty(STDIN_FILENO) && !program_state.sflags.eval_flag) {
			fprintf(stderr, "%s:\n", argv[0]);
			fprintf(stderr, "==========\n");
			fprintf(stderr, "%s\n", program_state.src[0].total.buf);
			fprintf(stderr, "==========\n");
		}
		int ret = compile(program_state.src[1].total.buf, program_state.cc_list.list, argv, true);
		/* print output and exit code if non-zero */
		if (ret || (isatty(STDIN_FILENO) && !program_state.sflags.eval_flag))
			fprintf(stderr, "[exit status: %d]\n", ret);

		/* reset io stream buffering modes */
		tty_fix(&program_state);

		/* exit if executed with `-e` argument */
		if (program_state.sflags.eval_flag) {
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
