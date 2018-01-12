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

/* silence linter */
int getopt_long(int ___argc, char *const ___argv[], char const *__shortopts, struct option const *__longopts, int *__longind);

/* global toggle flags */
bool asm_flag = false, eval_flag = false, in_flag = false, out_flag = false;
bool parse_flag = true, track_flag = true, warn_flag = false;
/* global compiler arg array */
char **cc_argv;
/* string to compile */
char eval_arg[EVAL_LIMIT];
/* input file source */
char *input_src[3];
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
	"-pedantic-errors",
	NULL
};
static char *const asm_list[] = {
	"ERROR", "-masm=att", "-masm=intel",
	NULL
};
static int option_index;
static char *tmp_arg;
/* compiler arguments and library list structs */
static STR_LIST cc_list, lib_list, sym_list;

/* getopts variables */
extern char *optarg;
extern int optind, opterr, optopt;
/* global completion list */
extern char *comp_arg_list[];
/* global linker flags and completions structs */
extern STR_LIST ld_list, comp_list;
extern char const *prelude, *prog_start, *prog_start_user, *prog_end;

char **parse_opts(int argc, char *argv[], char const optstring[], FILE **restrict ofile, char **restrict out_filename, char **restrict asm_filename)
{
	int opt;
	enum asm_type asm_choice = NONE;
	char *out_file = NULL, *in_file = NULL, *asm_file = NULL;
	/* cleanup previous allocations */
	free_str_list(&ld_list);
	free_str_list(&lib_list);
	free_str_list(&comp_list);
	cc_list.cnt = 0;
	cc_list.max = 1;
	/* don't print an error if option not found */
	opterr = 0;
	/* reset option indices to reuse argv */
	option_index = 0;
	optind = 1;

	/* sanity check */
	if (!out_filename || !asm_filename)
		ERRX("output filename NULL");

	/* initilize argument lists */
	init_list(&cc_list, "FOOBARTHISVALUEDOESNTMATTERTROLLOLOLOL");
	/* TODO: allow use of other linkers besides gcc without breaking due to seek errors */
	init_list(&ld_list, "gcc");
	init_list(&lib_list, NULL);

	/* re-zero cc_list.list[0] so -c argument can be added */
	memset(cc_list.list[0], 0, strlen(cc_list.list[0]) + 1);

	while ((opt = getopt_long(argc, argv, optstring, long_opts, &option_index)) != -1) {
		switch (opt) {

		/* use input file */
		case 'f': {
			if (in_file)
				ERRX("too many input files specified");
			in_file = optarg;
			int scan_state = IN_PRELUDE;
			size_t sz[3] = {PAGE_SIZE, PAGE_SIZE, PAGE_SIZE};
			char tmp_buf[PAGE_SIZE];
			for (size_t i = 0; i < ARRLEN(input_src); i++) {
				xmalloc(&input_src[i], PAGE_SIZE, "malloc() input_src");
				input_src[i][0] = 0;
			}
			regex_t reg[2];
			char const main_regex[] = "^[[:blank:]]*int[[:blank:]]+main[^\\(]*\\(";
			char const end_regex[] = "^[[:blank:]]*return[[:blank:]]+[^;]+;";
			if (regcomp(&reg[0], main_regex, REG_EXTENDED|REG_NEWLINE|REG_NOSUB))
				ERR("failed to compile main_regex");
			if (regcomp(&reg[1], end_regex, REG_EXTENDED|REG_NEWLINE|REG_NOSUB))
				ERR("failed to compile end_regex");

			FILE *tmp_file = xfopen(in_file, "r");
			char *ret = fgets(tmp_buf, PAGE_SIZE, tmp_file);
			for (; ret; ret = fgets(tmp_buf, PAGE_SIZE, tmp_file)) {
				switch (scan_state) {
				case IN_PRELUDE:
					/* no match */
					if (regexec(&reg[0], tmp_buf, 1, 0, 0)) {
						strmv(CONCAT, input_src[0], tmp_buf);
						sz[0] += strlen(tmp_buf) + 1;
						xrealloc(&input_src[0], sz[0], "parse_opts() xrealloc()");
						break;
					}
					regfree(&reg[0]);
					scan_state = IN_MIDDLE;

				case IN_MIDDLE:
					/* no match */
					if (regexec(&reg[1], tmp_buf, 1, 0, 0)) {
						strmv(CONCAT, input_src[1], tmp_buf);
						sz[1] += strlen(tmp_buf) + 1;
						xrealloc(&input_src[1], sz[1], "parse_opts() xrealloc()");
						break;
					}
					strmv(CONCAT, input_src[1], "\n");
					sz[1] += 2;
					xrealloc(&input_src[1], sz[1], "parse_opts() xrealloc()");
					regfree(&reg[1]);
					scan_state = IN_EPILOGUE;

				case IN_EPILOGUE:
					strmv(CONCAT, input_src[2], tmp_buf);
					sz[2] += strlen(tmp_buf) + 1;
					xrealloc(&input_src[2], sz[2], "parse_opts() xrealloc()");
					break;
				}
			}
			prelude = input_src[0];
			prog_start = prog_start_user = input_src[1];
			prog_end = input_src[2];
			in_flag ^= true;
			break;
		}

		/* at&t asm style */
		case 'a':
			if (asm_file)
				ERRX("too many output files specified");
			if (asm_choice)
				ERRX("more than one assembler dialect specified");
			asm_choice = ATT;
			asm_file = optarg;
			asm_flag ^= true;
			break;

		/* switch compiler */
		case 'c':
			if (!cc_list.list[0][0]) {
				size_t cc_len = strlen(optarg) + 1;
				size_t pval_len = strlen("FOOBARTHISVALUEDOESNTMATTERTROLLOLOLOL") + 1;
				/* realloc if needed */
				if (cc_len > pval_len) {
					if (!(tmp_arg = realloc(cc_list.list[0], cc_len)))
						ERRARR("cc_list.list", (size_t)0);
					/* copy argument to cc_list.list[0] */
					cc_list.list[0] = tmp_arg;
					memset(cc_list.list[0], 0, strlen(optarg) + 1);
				}
				strmv(0, cc_list.list[0], optarg);
			}
			break;

		/* eval string */
		case 'e':
			if (strlen(optarg) > sizeof eval_arg)
				ERRX("eval string too long");
			strmv(0, eval_arg, optarg);
			eval_flag ^= true;
			break;

		/* intel asm style */
		case 'i':
			if (asm_file)
				ERRX("too many output files specified");
			if (asm_choice)
				ERRX("more than one assembler dialect specified");
			asm_choice = INTEL;
			asm_file = optarg;
			asm_flag ^= true;
			break;

		/* header directory flag */
		case 'I':
			append_str(&cc_list, optarg, 2);
			strmv(0, cc_list.list[cc_list.cnt - 1], "-I");
			break;

		/* dynamic library flag */
		case 'l':
			do {
				char buf[strlen(optarg) + 12];
				strmv(0, buf, "/lib/lib");
				strmv(CONCAT, buf, optarg);
				strmv(CONCAT, buf, ".so");
				append_str(&lib_list, buf, 0);
				append_str(&ld_list, optarg, 2);
				memcpy(ld_list.list[ld_list.cnt - 1], "-l", 2);
			} while (0);
			break;

		/* output file flag */
		case 'o':
			if (out_file)
				ERRX("too many output files specified");
			out_file = optarg;
			out_flag ^= true;
			break;

		/* parse flag */
		case 'p':
			parse_flag ^= true;
			break;

		/* track flag */
		case 't':
			track_flag ^= true;
			break;

		/* warning flag */
		case 'w':
			warn_flag ^= true;
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

	/* asm output flag */
	if (asm_flag) {
		if (asm_file && !*asm_filename) {
			xcalloc(asm_filename, 1, strlen(asm_file) + 1, "error during asm_filename calloc()");
			strmv(0, *asm_filename, asm_file);
			if (!strcmp(cc_list.list[0], "icc"))
				asm_choice = ATT;
			if (!asm_dialect)
				asm_dialect = asm_choice;
			append_str(&cc_list, asm_list[asm_choice], 0);
		}
	}

	/* output file flag */
	if (out_flag) {
		if (out_file && !*out_filename) {
			xcalloc(out_filename, 1, strlen(out_file) + 1, "error during out_filename calloc()");
			strmv(0, *out_filename, out_file);
		}
		if (*ofile)
			fclose(*ofile);
		if (!(*ofile = fopen(*out_filename, "wb")))
			ERR("failed to create output file");
	}

	/* append warning flags */
	if (warn_flag) {
		for (size_t i = 0; warn_list[i]; i++)
			append_str(&cc_list, warn_list[i], 0);
	}

	/* default to gcc as a compiler */
	if (!cc_list.list[0][0])
		strmv(0, cc_list.list[0], "gcc");
	/* finalize argument lists */
	for (size_t i = 0; cc_arg_list[i]; i++)
		append_str(&cc_list, cc_arg_list[i], 0);
	for (size_t i = 0; ld_arg_list[i]; i++)
		append_str(&ld_list, ld_arg_list[i], 0);
	/* append NULL to generated lists */
	append_str(&cc_list, NULL, 0);
	append_str(&ld_list, NULL, 0);
	append_str(&lib_list, NULL, 0);

	/* parse ELF shared libraries for completions */
	if (parse_flag) {
		init_list(&comp_list, NULL);
		init_list(&sym_list, NULL);
		parse_libs(&sym_list, lib_list.list);
		for (size_t i = 0; comp_arg_list[i]; i++)
			append_str(&comp_list, comp_arg_list[i], 0);
		for (size_t i = 0; i < sym_list.cnt; i++)
			append_str(&comp_list, sym_list.list[i], 0);
		append_str(&comp_list, NULL, 0);
		append_str(&sym_list, NULL, 0);
		free_str_list(&sym_list);
	}

	return cc_list.list;
}

void read_syms(STR_LIST *restrict tokens, char const *restrict elf_file)
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

void parse_libs(STR_LIST *restrict symbols, char *restrict libs[])
{
	for (size_t i = 0; libs[i]; i++) {
		STR_LIST cur_syms = {.cnt = 0, .max = 1, .list = NULL};
		init_list(&cur_syms, NULL);
		read_syms(&cur_syms, libs[i]);
		for (size_t j = 0; j < cur_syms.cnt; j++) {
			append_str(symbols, cur_syms.list[j], 0);
		}
		append_str(&cur_syms, NULL, 0);
		free_str_list(&cur_syms);
	}
	append_str(symbols, NULL, 0);
}
