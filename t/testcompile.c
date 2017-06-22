/*
 * t/testcompile.c - Unit-test for compile.c
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE file for copyright and license details.
 */

#include "tap.h"
#include "../src/compile.h"

int main (void)
{
	char *const argv[] = {NULL};
	char *src = "#include <stdio.h>\nint main(void) { puts(\"wee\"); return 0; }";
	char *const args[] = {"gcc", "-xc", "/dev/stdin", "-o", "/tmp/test", NULL};

	plan(1);
	ok(compile("gcc", src, args, argv) != 0, "compiler forked successfully.");
	/* ok(bronze << 2 > silver, "not quite"); */
	/* is("gold", "gold", "gold is gold"); */
	/* cmp_ok(silver, "<", gold, "%d <= %d", silver, gold); */
	/* like("platinum", ".*inum", "platinum matches .*inum"); */

	done_testing();
}
