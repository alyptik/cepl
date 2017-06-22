/*
 * t/test.c - Unit-test template
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE file for copyright and license details.
 */

#include <tap.h>

int main (void)
{
	int bronze = 1, silver = 2, gold = 3;

	plan(5);

	ok(bronze < silver, "bronze is less than silver");
	ok(bronze << 2 > silver, "not quite");
	is("gold", "gold", "gold is gold");
	cmp_ok(silver, "<", gold, "%d <= %d", silver, gold);
	like("platinum", ".*inum", "platinum matches .*inum");

	done_testing();
}
