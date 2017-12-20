/*
 * t/testvars.c - unit-test for vars.c
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE.md file for copyright and license details.
 */

#include "tap.h"
#include "../src/vars.h"
#include <sys/resource.h>
#include <sys/time.h>

/* global linker arguments struct */
STR_LIST ld_list;

int main(void)
{
	STR_LIST ids = {0};
	TYPE_LIST types = {0};
	char const src[] =
		"int a = 0, b = 0,*c = &a, *d = &b;"
		"struct { int *memb; } e = {0}, f = {0}, *g = &e, *h = &f;"
		"char wark[] = \"wark\", *ptr = wark;"
		"long foo = 1, bar = 456;"
		"short baz = 50; int *quix = &baz;"
		"double res = foo + (double)bar / 1000;"
		"ssize_t boop = -5; wchar_t florp = L'x';"
		"int plonk[5] = {1,2,3,4,5}, vroom[5] = {0};"
		"struct foo kabonk = {0};";

	struct rlimit old, new;
	getrlimit(RLIMIT_STACK, &old);
	printf("%zu - %zu\n", old.rlim_cur, old.rlim_max);
	new = old;
	new.rlim_cur = new.rlim_max;
	setrlimit(RLIMIT_STACK, &new);

	plan(23);

	/* initialize lists */
	init_list(&ids, NULL);
	init_tlist(&types);

	ok(extract_type("unsigned long long foo = 5", "foo") == T_UINT, "succeed extracting unsigned type from `foo`.");
	ok(extract_type("int baz[] = {5,4,3,2,1,0}", "baz") == T_PTR, "succeed extracting pointer type from `baz`.");
	ok(find_vars(src, &ids, &types) == 20, "succeed finding twelve objects.");
	ok(extract_type(src, "a") == T_INT, "succeed extracting signed type from `a`.");
	ok(extract_type(src, "b") == T_INT, "succeed extracting signed type from `b`.");
	ok(extract_type(src, "c") == T_PTR, "succeed extracting pointer type from `c`.");
	ok(extract_type(src, "d") == T_PTR, "succeed extracting pointer type from `d`.");
	ok(extract_type(src, "e") == T_OTHER, "succeed extracting struct type from `e`.");
	ok(extract_type(src, "f") == T_OTHER, "succeed extracting struct type from `f`.");
	ok(extract_type(src, "g") == T_OTHER, "succeed extracting struct type from `g`.");
	ok(extract_type(src, "h") == T_OTHER, "succeed extracting struct type from `h`.");
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

	/* cleanup */
	free_str_list(&ids);
	free(types.list);

	done_testing();
}
