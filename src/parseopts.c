/*
 * parseopts.c - option parsing
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#include <getopt.h>
#include "parseopts.h"
#include "readline.h"

/* silence linter */
int getopt_long(int argc, char *const argv[], char const *optstring, struct option const *longopts, int *longindex);
FILE *fdopen(int fd, char const *mode);
ssize_t getline(char **lineptr, size_t *n, FILE *stream);

/* global toggle flag for warnings and completions */
bool warn_flag = false, parse_flag = false, out_flag = false;

static struct option long_opts[] = {
	{"help", no_argument, 0, 'h'},
	{"parse", no_argument, 0, 'p'},
	{"version", no_argument, 0, 'v'},
	{"warnings", no_argument, 0, 'w'},
	{"compiler", required_argument, 0, 'c'},
	{0, 0, 0, 0}
};
static char *const cc_arg_list[] = {
	"-O0", "-pipe", "-fPIC", "-std=c11",
	"-S", "-xc", "/proc/self/fd/0",
	"-o", "/proc/self/fd/1", NULL
};
static char *const ld_arg_list[] = {
	"-O0", "-pipe", "-fPIC", "-std=c11",
	"-xassembler", "/proc/self/fd/0",
	"-o", "/proc/self/fd/1", NULL
};
static char *const warn_list[] = {
	"-pedantic-errors", "-Wall", "-Wextra", NULL
};
static int option_index = 0;
static char *tmp_arg;
static char **tmp_list = NULL;
/* compiler arguments and library list structs */
static struct str_list cc_list = {.cnt = 0, .list = NULL};
static struct str_list lib_list = {.cnt = 0, .list = NULL};
static struct str_list sym_list = {.cnt = 0, .list = NULL};

/* getopts variables */
extern char *optarg;
extern int optind, opterr, optopt;
/* global completion list */
extern char *comp_arg_list[];
/* global linker flags and completions structs */
extern struct str_list ld_list, comp_list;

char **parse_opts(int argc, char *argv[], char const optstring[], FILE **ofile)
{
	int opt;
	char *out_file = NULL;

	if (lib_list.list)
		free_argv(lib_list.list);
	if (ld_list.list)
		free_argv(ld_list.list);
	*ofile = NULL;
	tmp_list = NULL;
	lib_list.cnt = 0, cc_list.cnt = 0, comp_list.cnt = 0, ld_list.cnt = 0;
	cc_list.list = NULL, lib_list.list = NULL, sym_list.list = NULL;
	/* don't print an error if option not found */
	opterr = 0;

	/* initilize argument lists */
	init_list(&cc_list, "gcc");
	init_list(&ld_list, "gcc");
	if ((lib_list.list = malloc(sizeof *lib_list.list)) == NULL)
		err(EXIT_FAILURE, "%s", "error during initial lib_list.list malloc()");

	/* re-zero cc_list.list[0] so -c argument can be added */
	memset(cc_list.list[0], 0, strlen(cc_list.list[0]) + 1);

	while ((opt = getopt_long(argc, argv, optstring, long_opts, &option_index)) != -1) {
		switch (opt) {
		/* switch compiler */
		case 'c':
			if (!cc_list.list[0][0]) {
				/* copy argument to cc_list.list[0] */
				if ((tmp_arg = realloc(cc_list.list[0], strlen(optarg) + 1)) == NULL)
					err(EXIT_FAILURE, "%s[%d] %s", "error during initial cc_list.list", 0, "malloc()");
				cc_list.list[0] = tmp_arg;
				memset(cc_list.list[0], 0, strlen(optarg) + 1);
				memcpy(cc_list.list[0], optarg, strlen(optarg) + 1);
			}
			break;

		/* header directory flag */
		case 'I':
			append_str(&cc_list, optarg, 2);
			memcpy(cc_list.list[cc_list.cnt - 1], "-I", 2);
			break;

		/* dynamic library flag */
		case 'l':
			{
				char buf[strlen(optarg) + 12];
				memcpy(buf, "/lib/lib", 8);
				memcpy(buf + 8, optarg, strlen(optarg));
				memcpy(buf + 8 + strlen(optarg), ".so", 4);
				append_str(&lib_list, buf, 0);
				append_str(&ld_list, optarg, 2);
				memcpy(ld_list.list[ld_list.cnt - 1], "-l", 2);
			}
			break;

		/* output file flag */
		case 'o':
			if (out_file != NULL)
				errx(EXIT_FAILURE, "%s", "too many output files specified");
			out_file = optarg;
			out_flag ^= true;
			break;

		/* parse flag */
		case 'p':
			parse_flag ^= true;
			break;

		/* warning flag */
		case 'w':
			warn_flag ^= true;
			break;

		/* version flag */
		case 'v':
			fprintf(stderr, "%s\n", CEPL_VERSION);
			exit(EXIT_FAILURE);
			/* unused break */
			break;

		/* usage and unrecognized flags */
		case 'h':
		case '?':
		default:
			fprintf(stderr, "%s %s %s\n", "Usage:", argv[0], USAGE);
			exit(EXIT_FAILURE);
		}
	}

	/* output file flag */
	if (out_flag) {
		if ((*ofile = fopen(out_file, "w")) == NULL)
			err(EXIT_FAILURE, "%s", "failed to create output file");
	}

	/* append warning flags */
	if (warn_flag) {
		for (register size_t i = 0; warn_list[i]; i++)
			append_str(&cc_list, warn_list[i], 0);
	}

	/* default to gcc as a compiler */
	if (!cc_list.list[0][0])
		memcpy(cc_list.list[0], "gcc", strlen("gcc") + 1);

	/* finalize argument lists */
	for (register size_t i = 0; cc_arg_list[i]; i++)
		append_str(&cc_list, cc_arg_list[i], 0);
	for (register size_t i = 0; ld_arg_list[i]; i++)
		append_str(&ld_list, ld_arg_list[i], 0);

	/* append NULL to generated lists */
	append_str(&cc_list, NULL, 0);
	append_str(&ld_list, NULL, 0);
	append_str(&lib_list, NULL, 0);

	/* parse ELF shared libraries for completions */
	if (parse_flag) {
		if (comp_list.list)
			free_argv(comp_list.list);
		comp_list.list = malloc(sizeof *comp_list.list);
		parse_libs(&sym_list, lib_list.list);
		for (register size_t i = 0; comp_arg_list[i]; i++)
			append_str(&comp_list, comp_arg_list[i], 0);
		for (register size_t i = 0; sym_list.list[i]; i++)
			append_str(&comp_list, sym_list.list[i], 0);
		append_str(&comp_list, NULL, 0);
		free_argv(sym_list.list);
	}

	return cc_list.list;
}

void read_syms(struct str_list *tokens, char const *elf_file)
{
	int elf_fd;
	Elf *elf;
	GElf_Shdr shdr;
	Elf_Scn *scn = NULL;

	/* sanity check filename */
	if (!elf_file)
		return;
	/* coordinate API and lib versions */
	elf_version(EV_CURRENT);
	elf_fd = open(elf_file, O_RDONLY);
	elf = elf_begin(elf_fd, ELF_C_READ, NULL);

	while ((scn = elf_nextscn(elf, scn))) {
		gelf_getshdr(scn, &shdr);
		if (shdr.sh_type == SHT_DYNSYM) {
			/* found a symbol table, go print it. */
			break;
		}
	}

	/* don't try to parse if NULL section descriptor */
	if (scn) {
		Elf_Data *data = elf_getdata(scn, NULL);
		size_t count = shdr.sh_size / shdr.sh_entsize;
		/* read the symbol names */
		for (register size_t i = 0; i < count; i++) {
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

void parse_libs(struct str_list *symbols, char *libs[])
{
	for (register size_t i = 0; libs[i]; i++) {
		struct str_list cur_syms = {.cnt = 0, .list = NULL};
		read_syms(&cur_syms, libs[i]);
		for (register ssize_t j = 0; j < cur_syms.cnt; j++) {
			append_str(symbols, cur_syms.list[j], 0);
			free(cur_syms.list[j]);
		}
		free(cur_syms.list);
	}
	append_str(symbols, NULL, 0);
}
