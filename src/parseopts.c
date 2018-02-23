/*
 * parseopts.c - option parsing
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#include "parseopts.h"
#include "readline.h"
#include <getopt.h>
#include <limits.h>
#include <regex.h>

/* globals */
enum asm_type asm_dialect = NONE;

static struct option long_opts[] = {
	{"att", required_argument, 0, 'a'},
	{"cc", required_argument, 0, 'c'},
	{"eval", required_argument, 0, 'e'},
	{"file", required_argument, 0, 'f'},
	{"help", no_argument, 0, 'h'},
	{"intel", required_argument, 0, 'i'},
	{"output", required_argument, 0, 'o'},
	{"parse", no_argument, 0, 'p'},
	{"tracking", no_argument, 0, 't'},
	{"version", no_argument, 0, 'v'},
	{"warnings", no_argument, 0, 'w'},
	{0}
};
static char *const cc_arg_list[] = {
	"-O0", "-pipe", "-fPIC",
	"-fverbose-asm", "-std=c11",
	"-S", "-xc", "/dev/stdin",
	"-o", "/dev/stdout",
	NULL
};
static char *const ld_arg_list[] = {
	"-O0", "-pipe",
	"-fPIC", "-no-pie",
	"-xassembler", "/dev/stdin",
	"-lm", "-o", "/dev/stdout",
	NULL
};
static char *const warn_list[] = {
	"-Wall", "-Wextra",
	"-Wno-unused",
	"-pedantic",
	NULL
};
static char *const asm_list[] = {
	"ERROR", "-masm=att", "-masm=intel",
	NULL
};
static int option_index;
static char *tmp_arg;

extern struct str_list comp_list;
extern char *comp_arg_list[];
extern char const *prelude, *prog_start, *prog_start_user, *prog_end;
/* getopts variables */
extern char *optarg;
extern int optind, opterr, optopt;

static inline void parse_input_file(struct program *restrict prog, char **restrict in_file)
{
	if (*in_file)
		ERRX("%s", "too many input files specified");
	*in_file = optarg;
	int scan_state = IN_PRELUDE;
	size_t sz[3] = {PAGE_SIZE, PAGE_SIZE, PAGE_SIZE};
	char tmp_buf[PAGE_SIZE];
	for (size_t i = 0; i < ARR_LEN(prog->input_src); i++) {
		xmalloc(char, &prog->input_src[i], PAGE_SIZE, "malloc() prog->input_src");
		prog->input_src[i][0] = 0;
	}

	regex_t reg[2];
	char const main_regex[] = "^[[:blank:]]*int[[:blank:]]+main[^\\(]*\\(";
	char const end_regex[] = "^[[:blank:]]*return[[:blank:]]+[^;]+;";
	if (regcomp(&reg[0], main_regex, REG_EXTENDED|REG_NEWLINE|REG_NOSUB))
		ERR("%s", "failed to compile main_regex");
	if (regcomp(&reg[1], end_regex, REG_EXTENDED|REG_NEWLINE|REG_NOSUB))
		ERR("%s", "failed to compile end_regex");

	FILE *tmp_file;
	xfopen(&tmp_file, *in_file, "r");
	char *ret = fgets(tmp_buf, PAGE_SIZE, tmp_file);
	for (; ret; ret = fgets(tmp_buf, PAGE_SIZE, tmp_file)) {
		switch (scan_state) {
		case IN_PRELUDE:
			/* no match */
			if (regexec(&reg[0], tmp_buf, 1, 0, 0)) {
				strmv(CONCAT, prog->input_src[0], tmp_buf);
				sz[0] += strlen(tmp_buf);
				xrealloc(char, &prog->input_src[0], sz[0], "xrealloc(char)");
				break;
			}
			regfree(&reg[0]);
			scan_state = IN_MIDDLE;

		case IN_MIDDLE:
			/* no match */
			if (regexec(&reg[1], tmp_buf, 1, 0, 0)) {
				strmv(CONCAT, prog->input_src[1], tmp_buf);
				sz[1] += strlen(tmp_buf);
				xrealloc(char, &prog->input_src[1], sz[1], "xrealloc(char)");
				break;
			}
			regfree(&reg[1]);
			scan_state = IN_EPILOGUE;

		case IN_EPILOGUE:
			strmv(CONCAT, prog->input_src[2], tmp_buf);
			sz[2] += strlen(tmp_buf);
			xrealloc(char, &prog->input_src[2], sz[2], "xrealloc(char)");
			break;
		}
	}
	prelude = prog->input_src[0];
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
	strmv(0, buf, "/lib/lib");
	strmv(CONCAT, buf, optarg);
	strmv(CONCAT, buf, ".so");
	append_str(&prog->lib_list, buf, 0);
	append_str(&prog->ld_list, optarg, 2);
	memcpy(prog->ld_list.list[prog->ld_list.cnt - 1], "-l", 2);
}

static inline void set_att_flag(struct program *restrict prog, char **asm_file, enum asm_type *asm_choice)
{
	if (*asm_file)
		ERRX("%s", "too many output files specified");
	if (*asm_choice)
		ERRX("%s", "more than one assembler dialect specified");
	*asm_choice = ATT;
	*asm_file = optarg;
	prog->sflags.asm_flag ^= true;
}

static inline void set_intel_flag(struct program *restrict prog, char **asm_file, enum asm_type *asm_choice)
{
	if (*asm_file)
		ERRX("%s", "too many output files specified");
	if (*asm_choice)
		ERRX("%s", "more than one assembler dialect specified");
	*asm_choice = INTEL;
	*asm_file = optarg;
	prog->sflags.asm_flag ^= true;
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

static inline void copy_asm_filename(struct program *restrict prog, char **asm_file, enum asm_type *asm_choice)
{
	/* asm output flag */
	if (prog->sflags.asm_flag) {
		if (*asm_file && !prog->asm_filename) {
			xcalloc(char, &prog->asm_filename, 1, strlen(*asm_file) + 1, "prog->asm_filename calloc()");
			strmv(0, prog->asm_filename, *asm_file);
			if (!strcmp(prog->cc_list.list[0], "icc"))
				*asm_choice = ATT;
			if (!asm_dialect)
				asm_dialect = *asm_choice;
			append_str(&prog->cc_list, asm_list[*asm_choice], 0);
		}
	}
}

static inline void set_out_file(struct program *restrict prog, char *restrict out_name)
{
	/* output file flag */
	if (prog->sflags.out_flag) {
		if (out_name && !prog->out_filename) {
			xcalloc(char, &prog->out_filename, 1, strlen(out_name) + 1, "prog->out_filename calloc()");
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

static inline void build_arg_list(struct program *restrict prog, char *const *cc_list, char *const *ld_list)
{
	/* default to gcc as a compiler */
	if (!prog->cc_list.list[0][0])
		strmv(0, prog->cc_list.list[0], "gcc");
	/* finalize argument lists */
	for (size_t i = 0; cc_arg_list[i]; i++)
		append_str(&prog->cc_list, cc_list[i], 0);
	for (size_t i = 0; ld_arg_list[i]; i++)
		append_str(&prog->ld_list, ld_list[i], 0);
	if (prog->sflags.warn_flag)
		enable_warnings(prog);
	/* append NULL to generated lists */
	append_str(&prog->cc_list, NULL, 0);
	append_str(&prog->ld_list, NULL, 0);
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
	enum asm_type asm_choice = NONE;
	char *out_name = NULL, *in_file = NULL, *asm_file = NULL;
	/* cleanup previous allocations */
	free_str_list(&prog->ld_list);
	free_str_list(&prog->lib_list);
	free_str_list(&comp_list);
	prog->cc_list.cnt = 0;
	prog->cc_list.max = 1;
	/* don't print an error if option not found */
	opterr = 0;
	/* reset option indices to reuse argv */
	option_index = 0;
	optind = 1;
	/* need to invert tracking flag because of control flow oddities */
	prog->sflags.track_flag ^= true;

	/* initilize argument lists */
	init_str_list(&prog->cc_list, "FOOBARTHISVALUEDOESNTMATTERTROLLOLOLOL");
	/* TODO: allow use of other linkers besides gcc without breaking due to seek errors */
	init_str_list(&prog->ld_list, "gcc");
	init_str_list(&prog->lib_list, NULL);

	/* re-zero prog->cc_list.list[0] so -c argument can be added */
	memset(prog->cc_list.list[0], 0, strlen(prog->cc_list.list[0]) + 1);

	while ((opt = getopt_long(argc, argv, optstring, long_opts, &option_index)) != -1) {
		switch (opt) {

		/* use input file */
		case 'f':
			parse_input_file(prog, &in_file);
			break;

		/* at&t asm style */
		case 'a':
			set_att_flag(prog, &asm_file, &asm_choice);
			break;

		/* specify compiler */
		case 'c':
			copy_compiler(prog);
			break;

		/* eval string */
		case 'e':
			copy_eval_code(prog);
			break;

		/* intel asm style */
		case 'i':
			set_intel_flag(prog, &asm_file, &asm_choice);
			break;

		/* header directory flag */
		case 'I':
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
			exit(0);
			/* unused break */
			break;

		/* usage and unrecognized flags */
		case 'h':
		case '?':
		default:
			fprintf(stderr, "%s %s %s\n", "Usage:", argv[0], USAGE_STRING);
			exit(0);
		}
	}

	copy_asm_filename(prog, &asm_file, &asm_choice);
	set_out_file(prog, out_name);
	build_arg_list(prog, cc_arg_list, ld_arg_list);
	build_sym_list(prog);

#ifdef _DEBUG
	DPRINTF("%s", "cc command line: \"");
	for (size_t i = 0; prog->cc_list.list[i]; i++)
		DPRINTF("%s ", prog->cc_list.list[i]);
	DPRINTF("\b%s\n", "\"");
#endif

	return prog->cc_list.list;
}
