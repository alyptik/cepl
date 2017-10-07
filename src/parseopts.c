/*
 * parseopts.c - option parsing
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#include "parseopts.h"
#include "readline.h"
#include <getopt.h>

/* silence linter */
int getopt_long(int ___argc, char *const ___argv[], char const *__shortopts, struct option const *__longopts, int *__longind);
FILE *fdopen(int __fd, char const *__modes);
ssize_t getline(char **__lineptr, size_t *__n, FILE *__stream);

/* global toggle flags */
bool warn_flag = false, parse_flag = true, track_flag = true, out_flag = false;
/* global compiler arg array */
char **cc_argv;

static struct option long_opts[] = {
	{"help", no_argument, 0, 'h'},
	{"parse", no_argument, 0, 'p'},
	{"tracking", no_argument, 0, 't'},
	{"version", no_argument, 0, 'v'},
	{"warnings", no_argument, 0, 'w'},
	{"compiler", required_argument, 0, 'c'},
	{0, 0, 0, 0}
};
static char *const cc_arg_list[] = {
	"-O0", "-pipe",
	"-fPIC", "-std=c11",
	"-Wno-unused-parameter",
	"-S", "-xc", "/dev/stdin",
	"-o", "/dev/stdout", NULL
};
static char *const ld_arg_list[] = {
	"-O0", "-pipe",
	"-fPIC", "-std=c11",
	"-Wno-unused-parameter",
	"-xassembler", "/dev/stdin",
	"-o", "/dev/stdout", NULL
};
static char *const warn_list[] = {
	"-pedantic-errors", "-Wall", "-Wextra", NULL
};
static int option_index;
static char *tmp_arg;
/* compiler arguments and library list structs */
static struct str_list cc_list, lib_list, sym_list;

/* getopts variables */
extern char *optarg;
extern int optind, opterr, optopt;
/* global completion list */
extern char *comp_arg_list[];
/* global linker flags and completions structs */
extern struct str_list ld_list, comp_list;

char **parse_opts(int argc, char *argv[], char const optstring[], FILE *volatile *restrict ofile, char **restrict out_filename)
{
	int opt;
	char *out_file = NULL;
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
	if (!out_filename)
		ERRX("out_filename NULL");

	/* initilize argument lists */
	init_list(&cc_list, "FOOBARTHISVALUEDOESNTMATTERTROLLOLOLOL");
	/* TODO: allow use of other linkers besides gcc without breaking due to seek errors */
	init_list(&ld_list, "gcc");
	init_list(&lib_list, NULL);

	/* re-zero cc_list.list[0] so -c argument can be added */
	memset(cc_list.list[0], 0, strlen(cc_list.list[0]) + 1);

	while ((opt = getopt_long(argc, argv, optstring, long_opts, &option_index)) != -1) {
		switch (opt) {
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

	/* output file flag */
	if (out_flag) {
		if (out_file && !*out_filename) {
			if (!(*out_filename = calloc(1, strlen(out_file) + 1)))
				ERR("error during out_filename calloc()");
			strmv(0, *out_filename, out_file);
		}
		if (*ofile)
			fclose(*ofile);
		if (!(*ofile = fopen(*out_filename, "wb")))
			ERR("failed to create output file");
		out_flag ^= true;
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

void parse_libs(struct str_list *restrict symbols, char *restrict libs[])
{
	for (size_t i = 0; libs[i]; i++) {
		struct str_list cur_syms = {.cnt = 0, .max = 1, .list = NULL};
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
