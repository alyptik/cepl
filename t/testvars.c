/*
 * t/testvars.c - unit-test for vars.c
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#include "tap.h"
#include "../src/vars.h"

/* global linker arguments struct */
struct str_list ld_list = {0};

/* compiler pre-program */
char const *prog_start =
	"\nint main(int argc, char **argv)\n"
	"{\n"
		"\t(void)argc, (void)argv;\n";
/* pre-program shown to user */
char const *prog_start_user =
	"\nint main(int argc, char **argv)\n"
	"{\n";
/* final section */
char const *prog_end =
		"\n\treturn 0;\n"
	"}\n";

int main(void)
{
	struct program prg = {0};
	char const src[] =
		"unsigned long long a = 5;"
		"int b[];"
		"int c = 0, d = 0, *e = &c, *f = &d;"
		"char wark[] = \"wark\", *ptr = wark;"
		"long foo = 1, bar = 456;"
		"short baz = 50; int *quix = &baz;"
		"double res = foo + (double)bar / 1000;"
		"ssize_t boop = -5; wchar_t florp = L'x';"
		"int plonk[5] = {1,2,3,4,5}, vroom[5] = {0};"
		"struct foo { int boop; } kabonk = {0}, *klakow = &kabonk;";

	plan(20);

	/* initialize lists */
	init_str_list(&prg.id_list, NULL);
	init_type_list(&prg.type_list);

	printf("%d", find_vars(&prg, src));
	ok(find_vars(&prg, src) == 19, "succeed finding nineteen objects.");
	ok(extract_type(src, "a") == T_UINT, "succeed extracting unsigned type from `a`.");
	ok(extract_type(src, "b") == T_PTR, "succeed extracting pointer type from `b`.");
	ok(extract_type(src, "c") == T_INT, "succeed extracting signed type from `c`.");
	ok(extract_type(src, "d") == T_INT, "succeed extracting signed type from `d`.");
	ok(extract_type(src, "e") == T_PTR, "succeed extracting pointer type from `e`.");
	ok(extract_type(src, "f") == T_PTR, "succeed extracting pointer type from `f`.");
	ok(extract_type(src, "wark") == T_STR, "succeed extracting string type from `wark`.");
	ok(extract_type(src, "ptr") == T_STR, "succeed extracting string type from `ptr`.");
	ok(extract_type(src, "foo") == T_INT, "succeed extracting signed type from `foo`.");
	ok(extract_type(src, "bar") == T_INT, "succeed extracting signed type from `bar`.");
	ok(extract_type(src, "baz") == T_INT, "succeed extracting signed type from `baz`.");
	ok(extract_type(src, "quix") == T_PTR, "succeed extracting pointer type from `quix`.");
	ok(extract_type(src, "res") == T_DBL, "succeed extracting floating type from `res`.");
	ok(extract_type(src, "boop") == T_INT, "succeed extracting signed type from `boop`.");
	ok(extract_type(src, "florp") == T_UINT, "succeed extracting unsigned type from `florp`.");
	ok(extract_type(src, "plonk") == T_PTR, "succeed extracting pointer type from `plonk`.");
	ok(extract_type(src, "vroom") == T_PTR, "succeed extracting pointer type from `vroom`.");
	ok(extract_type(src, "kabonk") == T_OTHER, "succeed extracting other type from `kabonk`.");
	ok(extract_type(src, "klakow") == T_OTHER, "succeed extracting other type from `klakow`.");

	/* cleanup */
	free_str_list(&prg.id_list);
	free(prg.type_list.list);

	done_testing();
}
