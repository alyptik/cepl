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

/* globals */
static struct option long_opts[] = {
	{"asm", required_argument, 0, 'a'},
	{"compiler", required_argument, 0, 'c'},
	{"eval", required_argument, 0, 'e'},
	{"help", no_argument, 0, 'h'},
	{"output", required_argument, 0, 'o'},
	{"parse", no_argument, 0, 'p'},
	{"std", required_argument , 0, 's'},
	{"version", no_argument, 0, 'v'},
	{"warnings", no_argument, 0, 'w'},
	{0}
};
static char *const cc_arg_list[] = {
	"-g3", "-O0", "-pipe",
	"-xc", "-",
	"-o/tmp/cepl_program",
	NULL
};
static char *const ccxx_arg_list[] = {
	"-g3", "-O0", "-pipe",
	"-xc++", "-",
	"-o/tmp/cepl_program",
	NULL
};
static char *const warn_list[] = {
	"-Wall", "-Wextra",
	"-pedantic",
	"-Wno-unused",
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

static inline void copy_compiler(struct program *prog)
{
	if (!prog->cc_list.list[0][0]) {
		size_t cc_len = strlen(optarg) + 1;
		size_t pval_len = strlen("initial") + 1;
		/* set cxx_flag if c++ compiler */
		if (optarg[strlen(optarg) - 1] == '+')
			prog->state_flags |= CXX_FLAG;
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

static inline void copy_libs(struct program *prog)
{
	char buf[strlen(optarg) + 12];
	strmv(0, buf, "/lib64/lib");
	strmv(CONCAT, buf, optarg);
	strmv(CONCAT, buf, ".so");
	append_str(&prog->lib_list, buf, 0);
	append_str(&prog->cc_list, optarg, 2);
	if (!prog->cc_list.list[prog->cc_list.cnt - 1])
		ERRX("null cc_list member passed to memcpy()");
	memcpy(prog->cc_list.list[prog->cc_list.cnt - 1], "-l", 2);
}

static inline void copy_eval_code(struct program *prog)
{
	if (strlen(optarg) > sizeof prog->eval_arg)
		ERRX("eval string too long");
	strmv(0, prog->eval_arg, optarg);
	prog->state_flags |= EVAL_FLAG;
}

static inline void copy_header_dirs(struct program *prog)
{
	append_str(&prog->cc_list, optarg, 2);
	memcpy(prog->cc_list.list[prog->cc_list.cnt - 1], "-I", 2);
}

static inline void copy_lib_dirs(struct program *prog)
{
	append_str(&prog->cc_list, optarg, 2);
	memcpy(prog->cc_list.list[prog->cc_list.cnt - 1], "-L", 2);
}

static inline void copy_std(struct program *prog)
{
	prog->state_flags |= STD_FLAG;
	append_str(&prog->cc_list, optarg, 5);
	memcpy(prog->cc_list.list[prog->cc_list.cnt - 1], "-std=", 5);
}

static inline void copy_out_file(struct program *prog, char **out_name)
{
	if (*out_name)
		ERRX("too many output files specified");
	*out_name = optarg;
	prog->state_flags |= OUT_FLAG;
}

static inline void set_out_file(struct program *prog, char *out_name)
{
	/* output file flag */
	if (prog->state_flags & OUT_FLAG) {
		if (out_name && !prog->out_filename) {
			xcalloc(&prog->out_filename, 1, strlen(out_name) + 1, "prog->out_filename calloc()");
			strmv(0, prog->out_filename, out_name);
		}
		if (prog->ofile)
			xfclose(&prog->ofile);
		xfopen(&prog->ofile, prog->out_filename, "wb");
	}
}

static inline void copy_asm_file(struct program *prog, char **asm_name)
{
	if (*asm_name)
		ERRX("too many assembly files specified");
	*asm_name = optarg;
	prog->state_flags |= ASM_FLAG;
}

static inline void set_asm_file(struct program *prog, char *asm_name)
{
	/* assembly file flag */
	if (prog->state_flags & ASM_FLAG) {
		if (asm_name && !prog->asm_filename) {
			xcalloc(&prog->asm_filename, 1, strlen(asm_name) + 1, "prog->asm_filename calloc()");
			strmv(0, prog->asm_filename, asm_name);
		}
	}
}

static inline void enable_warnings(struct program *prog)
{
	/* append warning flags */
	if (prog->state_flags & WARN_FLAG) {
		for (size_t i = 0; warn_list[i]; i++)
			append_str(&prog->cc_list, warn_list[i], 0);
	}
}

static inline void append_arg_list(struct program *prog, char *const *cc_list, char *const *lib_list)
{
	if (cc_list)
		for (size_t i = 0; cc_list[i]; i++)
			append_str(&prog->cc_list, cc_list[i], 0);
	if (lib_list)
		for (size_t i = 0; lib_list[i]; i++)
			append_str(&prog->lib_list, lib_list[i], 0);
	if (prog->state_flags & WARN_FLAG)
		enable_warnings(prog);
}

static inline void build_arg_list(struct program *prog, char *const *cc_list)
{
	char *cflags_orig = getenv("CFLAGS");
	char *ldflags_orig = getenv("LDFLAGS");
	char *ldlibs_orig = getenv("LDLIBS");
	char *libs_orig = getenv("LIBS");
	char *cflags = NULL, *ldflags = NULL, *ldlibs = NULL, *libs = NULL;

	/* don't modify environment strings */
	if (cflags_orig) {
		xmalloc(&cflags, strlen(cflags_orig) + 1, "cflags");
		strmv(0, cflags, cflags_orig);
	}
	if (ldflags_orig) {
		xmalloc(&ldflags, strlen(ldflags_orig) + 1, "ldflags");
		strmv(0, ldflags, ldflags_orig);
	}
	if (ldlibs_orig) {
		xmalloc(&ldlibs, strlen(ldlibs_orig) + 1, "ldlibs");
		strmv(0, ldlibs, ldlibs_orig);
	}
	if (libs_orig) {
		xmalloc(&libs, strlen(libs_orig) + 1, "libs");
		strmv(0, libs, libs_orig);
	}

	/* default to gcc as a compiler */
	if (!prog->cc_list.list[0][0])
		strmv(0, prog->cc_list.list[0], "gcc");
	append_arg_list(prog, cc_list, NULL);

	/* parse CFLAGS, LDFLAGS, LDLIBS, and LIBS from the environment */
	if (cflags)
		for (char *arg = strtok(cflags, " \t"); arg; arg = strtok(NULL, " \t"))
			append_str(&prog->cc_list, arg, 0);
	if (ldflags)
		for (char *arg = strtok(ldflags, " \t"); arg; arg = strtok(NULL, " \t"))
			append_str(&prog->cc_list, arg, 0);
	if (ldlibs)
		for (char *arg = strtok(ldlibs, " \t"); arg; arg = strtok(NULL, " \t"))
			append_str(&prog->lib_list, arg, 0);
	if (libs)
		for (char *arg = strtok(libs, " \t"); arg; arg = strtok(NULL, " \t"))
			append_str(&prog->lib_list, arg, 0);

	/* free temporary environment strings */
	free(cflags);
	free(ldflags);
	free(ldlibs);
	free(libs);

	/* NULL-terminate lists */
	append_str(&prog->cc_list, NULL, 0);
	append_str(&prog->lib_list, NULL, 0);
}

static inline void build_sym_list(struct program *prog)
{
	/* parse ELF shared libraries for completions */
	if (prog->state_flags & PARSE_FLAG) {
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

void read_syms(struct str_list *tokens, char const *elf_file)
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
		ERR("libelf out of date");
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

void parse_libs(struct str_list *symbols, char **libs)
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

char **parse_opts(struct program *prog, int argc, char **argv, char const *optstring)
{
	int opt;
	char *out_name = NULL, *asm_name = NULL;
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

	/* initilize argument lists */
	init_str_list(&prog->cc_list, "initial");
	init_str_list(&prog->lib_list, NULL);
	/* re-zero prog->cc_list.list[0] so -c argument can be added */
	memset(prog->cc_list.list[0], 0, strlen(prog->cc_list.list[0]) + 1);

	while ((opt = getopt_long(argc, argv, optstring, long_opts, &option_index)) != -1) {
		switch (opt) {
		/* output assembly */
		case 'a':
			copy_asm_file(prog, &asm_name);
			break;
		/* specify compiler */
		case 'c':
			copy_compiler(prog);
			break;

		/* eval string */
		case 'e':
			copy_eval_code(prog);
			break;

		/* header directory flag */
		case 'I':
			copy_header_dirs(prog);
			break;

		/* library directory flag */
		case 'L':
			copy_lib_dirs(prog);
			break;

		/* dynamic library flag */
		case 'l':
			copy_libs(prog);
			break;

		/* standard flag */
		case 's':
			copy_std(prog);
			break;

		/* output file flag */
		case 'o':
			copy_out_file(prog, &out_name);
			break;

		/* parse flag */
		case 'p':
			prog->state_flags &= ~PARSE_FLAG;
			break;

		/* warning flag */
		case 'w':
			prog->state_flags |= WARN_FLAG;
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
	set_asm_file(prog, asm_name);
	/* c++ compiler */
	if (prog->state_flags & CXX_FLAG) {
		if (!(prog->state_flags & STD_FLAG)) {
			append_str(&prog->cc_list, "gnu++26", 5);
			memcpy(prog->cc_list.list[prog->cc_list.cnt - 1], "-std=", 5);
		}
		build_arg_list(prog, ccxx_arg_list);
	/* c compiler */
	} else {
		if (!(prog->state_flags & STD_FLAG)) {
			append_str(&prog->cc_list, "gnu23", 5);
			memcpy(prog->cc_list.list[prog->cc_list.cnt - 1], "-std=", 5);
		}
		build_arg_list(prog, cc_arg_list);
	}
	build_sym_list(prog);

#ifdef _DEBUG
	printe("%s", "compiler command line: \"");
	for (size_t i = 0; prog->cc_list.list[i]; i++)
		printe("%s ", prog->cc_list.list[i]);
	printe("\b%s\n", "\"");
#endif

	return prog->cc_list.list;
}
