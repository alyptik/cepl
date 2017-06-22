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
	char *src = "#include <stdio.h>\nint main(void) { return 0; }";
	char *const args[] = {"gcc", "-std=c11", "-xc", "/dev/stdin", "-o", "/dev/stdout", NULL};

	plan(1);
	ok(compile("gcc", src, args, argv) == 0, "compiler forked successfully.");
	/* TODO: find better way to wait on compiler */
	usleep(500000);
	putchar('\n');

	done_testing();
}
