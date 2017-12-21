/*
 * t/testvars.c - unit-test for vars.c
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#include "tap.h"
#include "../src/vars.h"

/* global linker arguments struct */
STR_LIST ld_list;

int main(void)
{
	STR_LIST ids = {0};
	TYPE_LIST types = {0};
	char const *src[] = {
		"int a = 0, b = 0,*c = &a, *d = &b;",
		"char wark[] = \"wark\", *ptr = wark;",
		"long foo = 1, bar = 456;",
		"short baz = 50; int *quix = &baz;",
		"double res = foo + (double)bar / 1000;",
		"ssize_t boop = -5; wchar_t florp = L'x';",
		"int plonk[5] = {1,2,3,4,5}, vroom[5] = {0};",
		"struct foo { int boop; } kabonk = {0}, *klakow = &kabonk;",
	};

	plan(27);

	/* initialize lists */
	init_list(&ids, NULL);
	init_tlist(&types);

	ok(extract_type("unsigned long long foo = 5", "foo") == T_UINT, "succeed extracting unsigned type from `foo`.");
	ok(extract_type("int baz[] = {5,4,3,2,1,0}", "baz") == T_PTR, "succeed extracting pointer type from `baz`.");
	ok(find_vars(src[0], &ids, &types) == 4, "succeed finding four objects.");
	ok(extract_type(src[0], "a") == T_INT, "succeed extracting signed type from `a`.");
	ok(extract_type(src[0], "b") == T_INT, "succeed extracting signed type from `b`.");
	ok(extract_type(src[0], "c") == T_PTR, "succeed extracting pointer type from `c`.");
	ok(extract_type(src[0], "d") == T_PTR, "succeed extracting pointer type from `d`.");
	ok(find_vars(src[1], &ids, &types) == 2, "succeed finding two objects.");
	ok(extract_type(src[1], "wark") == T_STR, "succeed extracting string type from `wark`.");
	ok(extract_type(src[1], "ptr") == T_STR, "succeed extracting string type from `ptr`.");
	ok(find_vars(src[2], &ids, &types) == 2, "succeed finding two objects.");
	ok(extract_type(src[2], "foo") == T_INT, "succeed extracting signed type from `foo`.");
	ok(extract_type(src[2], "bar") == T_INT, "succeed extracting signed type from `bar`.");
	ok(find_vars(src[3], &ids, &types) == 2, "succeed finding two objects.");
	ok(extract_type(src[3], "baz") == T_INT, "succeed extracting signed type from `baz`.");
	ok(extract_type(src[3], "quix") == T_PTR, "succeed extracting pointer type from `quix`.");
	ok(find_vars(src[4], &ids, &types) == 1, "succeed finding one object.");
	ok(extract_type(src[4], "res") == T_DBL, "succeed extracting floating type from `res`.");
	ok(find_vars(src[5], &ids, &types) == 2, "succeed finding two objects.");
	ok(extract_type(src[5], "boop") == T_INT, "succeed extracting signed type from `boop`.");
	ok(extract_type(src[5], "florp") == T_UINT, "succeed extracting unsigned type from `florp`.");
	ok(find_vars(src[6], &ids, &types) == 2, "succeed finding two objects.");
	ok(extract_type(src[6], "plonk") == T_PTR, "succeed extracting pointer type from `plonk`.");
	ok(extract_type(src[6], "vroom") == T_PTR, "succeed extracting pointer type from `vroom`.");
	ok(find_vars(src[7], &ids, &types) == 2, "succeed finding two objects.");
	ok(extract_type(src[7], "kabonk") == T_OTHER, "succeed extracting other type from `kabonk`.");
	ok(extract_type(src[7], "klakow") == T_OTHER, "succeed extracting other type from `klakow`.");

	/* cleanup */
	free_str_list(&ids);
	free(types.list);

	done_testing();
}
