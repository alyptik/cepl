/*
 * parseopts.c - option parsing
 *
 * AUTHOR: Joey Pabalinas <joeypabalinas@gmail.com>
 * See LICENSE file for copyright and license details.
 */

/* silence linter */
#undef _GNU_SOURCE
#define _GNU_SOURCE

#include "hist.h"
#include "parseopts.h"
#include "readline.h"
#include <getopt.h>
#include <limits.h>
#include <regex.h>

/* globals */
static struct option long_opts[] = {
	{"compiler", required_argument, 0, 'c'},
	{"eval", required_argument, 0, 'e'},
	{"file", required_argument, 0, 'f'},
	{"help", no_argument, 0, 'h'},
	{"output", required_argument, 0, 'o'},
	{"parse", no_argument, 0, 'p'},
	{"tracking", no_argument, 0, 't'},
	{"version", no_argument, 0, 'v'},
	{"warnings", no_argument, 0, 'w'},
	{0}
};
static char *const cc_arg_list[] = {
	"-g3", "-O0", "-pipe",
	"-fPIC", "-std=gnu2x",
	"-xc", "-",
	"-o/tmp/cepl_program",
	NULL
};
static char *const ccxx_arg_list[] = {
	"-g3", "-O0", "-pipe",
	"-fPIC", "-std=gnu++2a",
	"-xc++", "-",
	"-o/tmp/cepl_program",
	NULL
};
static char *const warn_list[] = {
	"-Wall", "-Wextra",
	"-Wno-unused",
	"-pedantic",
	NULL
};
static int option_index;
static char *tmp_arg;

extern struct str_list comp_list;
extern char *comp_arg_list[];
extern char const *prologue, *prog_start, *prog_start_user, *prog_end;
/* getopts variables */
extern char *optarg;
extern int optind, opterr, optopt;

static inline void parse_input_file(struct program *restrict prog, char **restrict in_file)
{
	if (*in_file)
		ERRX("%s", "too many input files specified");
	*in_file = optarg;
	regex_t reg[2];
	bool after_main_signature = false;
	int scan_state = IN_PROLOGUE;
	size_t sz[3] = {PAGE_SIZE, PAGE_SIZE, PAGE_SIZE};
	char tmp_buf[PAGE_SIZE];
	for (size_t i = 0; i < arr_len(prog->input_src); i++) {
		xmalloc(&prog->input_src[i], PAGE_SIZE, "malloc() prog->input_src");
		prog->input_src[i][0] = 0;
	}

	/* regexes for pre-main, main, and return */
	char const main_regex[] = "^[[:blank:]]*int[[:blank:]]+main[^\\(]*\\(";
	char const end_regex[] = "^[[:blank:]]*return[[:blank:]]+[^;]+;";
	if (regcomp(&reg[0], main_regex, REG_EXTENDED|REG_NEWLINE|REG_NOSUB))
		ERR("%s", "failed to compile main_regex");
	if (regcomp(&reg[1], end_regex, REG_EXTENDED|REG_NEWLINE|REG_NOSUB))
		ERR("%s", "failed to compile end_regex");
	FILE *tmp_file;
	xfopen(&tmp_file, *in_file, "r");
	char *buf_ptr, *ret = fgets(tmp_buf, PAGE_SIZE, tmp_file);

	/* loop over file lines */
	for (; ret; ret = fgets(tmp_buf, PAGE_SIZE, tmp_file)) {
		switch (scan_state) {
		/* pre-main */
		case IN_PROLOGUE:
			/* no match */
			if (regexec(&reg[0], tmp_buf, 1, 0, 0)) {
				buf_ptr = tmp_buf;
				sz[0] += strlen(tmp_buf);
				xrealloc(&prog->input_src[0], sz[0], "xrealloc(char)");
				strmv(CONCAT, prog->input_src[0], tmp_buf);
				break;
			}
			regfree(&reg[0]);
			scan_state = IN_MIDDLE;

		/* main */
		case IN_MIDDLE:
			/* no match */
			if (regexec(&reg[1], tmp_buf, 1, 0, 0)) {
				buf_ptr = tmp_buf;
				sz[1] += strlen(tmp_buf);
				xrealloc(&prog->input_src[1], sz[1], "xrealloc(char)");
				strmv(CONCAT, prog->input_src[1], tmp_buf);
				if (!after_main_signature) {
					after_main_signature = true;
					break;
				}
				/* strip leading whitespace */
				buf_ptr += strspn(buf_ptr, " \t");
				strchrnul(buf_ptr, '\n')[0] = 0;
				/* skip single chars and argc/argv void statements */
				if (strlen(buf_ptr) > 1 && strcmp(buf_ptr, "(void)argc, (void)argv;"))
					dedup_history_add(&buf_ptr);
				break;
			}
			regfree(&reg[1]);
			scan_state = IN_EPILOGUE;

		/* return */
		case IN_EPILOGUE:
			buf_ptr = tmp_buf;
			sz[2] += strlen(tmp_buf);
			xrealloc(&prog->input_src[2], sz[2], "xrealloc(char)");
			strmv(CONCAT, prog->input_src[2], tmp_buf);
			break;
		}
	}
	prologue = prog->input_src[0];
	prog_start = prog_start_user = prog->input_src[1];
	prog_end = prog->input_src[2];
	prog->sflags.in_flag ^= true;
	xfclose(&tmp_file);
}

static inline void copy_compiler(struct program *restrict prog)
{
	if (!prog->cc_list.list[0][0]) {
		size_t cc_len = strlen(optarg) + 1;
		size_t pval_len = strlen("FOOBARTHISVALUEDOESNTMATTERTROLLOLOLOL") + 1;
		/* set cxx_flag if c++ compiler */
		if (!strcmp(optarg, "g++") || !strcmp(optarg, "clang++"))
			prog->sflags.cxx_flag = true;
		/* realloc if needed */
		if (cc_len > pval_len) {
			if (!(tmp_arg = realloc(prog->cc_list.list[0], cc_len)))
				ERR("%s[%zu]", "prog->cc_list.list", (size_t)0);
			/* copy argument to prog->cc_list.list[0] */
			prog->cc_list.list[0] = tmp_arg;
			memset(prog->cc_list.list[0], 0, strlen(optarg) + 1);
		}
		strmv(0, prog->cc_list.list[0], optarg);
	}
}

static inline void copy_libs(struct program *restrict prog)
{
	char buf[strlen(optarg) + 12];
	strmv(0, buf, "/lib64/lib");
	strmv(CONCAT, buf, optarg);
	strmv(CONCAT, buf, ".so");
	append_str(&prog->lib_list, buf, 0);
	append_str(&prog->cc_list, optarg, 2);
	if (!prog->cc_list.list[prog->cc_list.cnt - 1])
		ERRX("%s", "null ld_list member passed to memcpy()");
	memcpy(prog->cc_list.list[prog->cc_list.cnt - 1], "-l", 2);
}

static inline void copy_eval_code(struct program *restrict prog)
{
	if (strlen(optarg) > sizeof prog->eval_arg)
		ERRX("%s", "eval string too long");
	strmv(0, prog->eval_arg, optarg);
	prog->sflags.eval_flag ^= true;
}

static inline void copy_header_dirs(struct program *restrict prog)
{
	append_str(&prog->cc_list, optarg, 2);
	strmv(0, prog->cc_list.list[prog->cc_list.cnt - 1], "-I");
}

static inline void copy_out_file(struct program *restrict prog, char **restrict out_name)
{
	if (*out_name)
		ERRX("%s", "too many output files specified");
	*out_name = optarg;
	prog->sflags.out_flag ^= true;
}

static inline void set_out_file(struct program *restrict prog, char *restrict out_name)
{
	/* output file flag */
	if (prog->sflags.out_flag) {
		if (out_name && !prog->out_filename) {
			xcalloc(&prog->out_filename, 1, strlen(out_name) + 1, "prog->out_filename calloc()");
			strmv(0, prog->out_filename, out_name);
		}
		if (prog->ofile)
			fclose(prog->ofile);
		if (!(prog->ofile = fopen(prog->out_filename, "wb")))
			ERR("%s", "failed to create output file");
	}
}

static inline void enable_warnings(struct program *restrict prog)
{
	/* append warning flags */
	if (prog->sflags.warn_flag) {
		for (size_t i = 0; warn_list[i]; i++)
			append_str(&prog->cc_list, warn_list[i], 0);
	}
}

static inline void append_arg_list(struct program *restrict prog, char *const *cc_list, char *const *lib_list)
{
	if (cc_list)
		for (size_t i = 0; cc_list[i]; i++)
			append_str(&prog->cc_list, cc_list[i], 0);
	if (lib_list)
		for (size_t i = 0; lib_list[i]; i++)
			append_str(&prog->lib_list, lib_list[i], 0);
	if (prog->sflags.warn_flag)
		enable_warnings(prog);
}

static inline void build_arg_list(struct program *restrict prog, char *const *cc_list)
{
	char *cflags = getenv("CFLAGS");
	char *ldlibs = getenv("LDLIBS");
	char *libs = getenv("LIBS");

	/* default to gcc as a compiler */
	if (!prog->cc_list.list[0][0])
		strmv(0, prog->cc_list.list[0], "gcc");
	append_arg_list(prog, cc_list, NULL);
	/* parse CFLAGS, LDFLAGS, LDLIBS, and LIBS from the environment (-g flags will hang) */
	if (cflags)
		for (char *arg = strtok(cflags, " \t"); arg; arg = strtok(NULL, " \t"))
			if (arg[0] != '-' && arg[1] != 'g')
				append_str(&prog->cc_list, arg, 0);
	if (ldlibs)
		for (char *arg = strtok(ldlibs, " \t"); arg; arg = strtok(NULL, " \t"))
			append_str(&prog->lib_list, arg, 0);
	if (libs)
		for (char *arg = strtok(libs, " \t"); arg; arg = strtok(NULL, " \t"))
			append_str(&prog->lib_list, arg, 0);
	/* NULL-terminate lists */
	append_str(&prog->cc_list, NULL, 0);
	append_str(&prog->lib_list, NULL, 0);
}

static inline void build_sym_list(struct program *restrict prog)
{
	/* parse ELF shared libraries for completions */
	if (prog->sflags.parse_flag) {
		init_str_list(&comp_list, NULL);
		init_str_list(&prog->sym_list, NULL);
		parse_libs(&prog->sym_list, prog->lib_list.list);
		for (size_t i = 0; comp_arg_list[i]; i++)
			append_str(&comp_list, comp_arg_list[i], 0);
		for (size_t i = 0; i < prog->sym_list.cnt; i++)
			append_str(&comp_list, prog->sym_list.list[i], 0);
		append_str(&comp_list, NULL, 0);
		append_str(&prog->sym_list, NULL, 0);
		free_str_list(&prog->sym_list);
	}
}

void read_syms(struct str_list *restrict tokens, char const *restrict elf_file)
{
	int elf_fd;
	GElf_Shdr shdr;
	Elf *elf;
	Elf_Scn *scn = NULL;

	/* sanity check filename */
	if (!elf_file)
		return;

	/* coordinate API and lib versions */
	if (elf_version(EV_CURRENT) == EV_NONE)
		ERR("%s", "libelf out of date");
	elf_fd = open(elf_file, O_RDONLY);
	elf = elf_begin(elf_fd, ELF_C_READ, NULL);

	while ((scn = elf_nextscn(elf, scn))) {
		gelf_getshdr(scn, &shdr);
		/* found a symbol table, go print it. */
		if (shdr.sh_type == SHT_DYNSYM)
			break;
	}

	/* don't try to parse if NULL section descriptor */
	if (scn) {
		Elf_Data *data = elf_getdata(scn, NULL);
		size_t count = shdr.sh_size / shdr.sh_entsize;
		/* read the symbol names */
		for (size_t i = 0; i < count; i++) {
			GElf_Sym sym;
			char *sym_str;
			gelf_getsym(data, i, &sym);
			sym_str = elf_strptr(elf, shdr.sh_link, sym.st_name);
			append_str(tokens, sym_str, 0);
		}
		append_str(tokens, NULL, 0);
	}

	elf_end(elf);
	close(elf_fd);
}

void parse_libs(struct str_list *restrict symbols, char **restrict libs)
{
	for (size_t i = 0; libs[i]; i++) {
		struct str_list cur_syms = {.cnt = 0, .max = 1, .list = NULL};
		init_str_list(&cur_syms, NULL);
		read_syms(&cur_syms, libs[i]);
		for (size_t j = 0; j < cur_syms.cnt; j++) {
			append_str(symbols, cur_syms.list[j], 0);
		}
		append_str(&cur_syms, NULL, 0);
		free_str_list(&cur_syms);
	}
	append_str(symbols, NULL, 0);
}

char **parse_opts(struct program *restrict prog, int argc, char **argv, char const *optstring)
{
	int opt;
	char *out_name = NULL, *in_file = NULL, *asm_file = NULL;
	/* cleanup previous allocations */
	free_str_list(&prog->cc_list);
	free_str_list(&prog->lib_list);
	free_str_list(&comp_list);
	prog->cc_list.cnt = 0;
	prog->cc_list.max = 1;
	/* don't print an error if option not found */
	opterr = 0;
	/* reset option indices to reuse argv */
	option_index = 0;
	optind = 1;

	/*
	 * TODO XXX:
	 *
	 * you currently need to need to invert the parse and track flags
	 * because they start out true; need to figure out why and fix it.
	 */
	prog->sflags.parse_flag ^= true;
	prog->sflags.track_flag ^= true;
	/* initilize argument lists */
	init_str_list(&prog->cc_list, "FOOBARTHISVALUEDOESNTMATTERTROLLOLOLOL");
	init_str_list(&prog->lib_list, NULL);
	/* re-zero prog->cc_list.list[0] so -c argument can be added */
	memset(prog->cc_list.list[0], 0, strlen(prog->cc_list.list[0]) + 1);

	while ((opt = getopt_long(argc, argv, optstring, long_opts, &option_index)) != -1) {
		switch (opt) {
		/* use input file */
		case 'f':
			parse_input_file(prog, &in_file);
			break;

		/* specify compiler */
		case 'c':
			copy_compiler(prog);
			break;

		/* eval string */
		case 'e':
			copy_eval_code(prog);
			break;

		case 'I':
			/* header directory flag */
			copy_header_dirs(prog);
			break;

		/* dynamic library flag */
		case 'l':
			copy_libs(prog);
			break;

		/* output file flag */
		case 'o':
			copy_out_file(prog, &out_name);
			break;

		/* parse flag */
		case 'p':
			prog->sflags.parse_flag ^= true;
			break;

		/* track flag */
		case 't':
			prog->sflags.track_flag ^= true;
			break;

		/* warning flag */
		case 'w':
			prog->sflags.warn_flag ^= true;
			break;

		/* version flag */
		case 'v':
			fprintf(stderr, "%s\n", VERSION_STRING);
			exit(EXIT_SUCCESS);
			/* unused break */
			break;

		/* usage and unrecognized flags */
		case 'h':
		case '?':
		default:
			fprintf(stderr, "%s %s %s\n", "Usage:", argv[0], USAGE_STRING);
			exit(EXIT_SUCCESS);
			/* unused break */
			break;
		}
	}

	set_out_file(prog, out_name);
	/* c compiler */
	if (!prog->sflags.cxx_flag)
		build_arg_list(prog, cc_arg_list);
	/* c++ compiler */
	else
		build_arg_list(prog, ccxx_arg_list);
	build_sym_list(prog);

#ifdef _DEBUG
	printe("%s", "compiler command line: \"");
	for (size_t i = 0; prog->cc_list.list[i]; i++)
		printe("%s ", prog->cc_list.list[i]);
	printe("\b%s\n", "\"");
#endif

	return prog->cc_list.list;
}
