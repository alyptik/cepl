#!/bin/perl

use strict;
use warnings;

map { s/.*\bT\b//g && print if /\bT\s[^_]/; }
	map { split /\n/, `nm -D $_`; }
	keys %{{map { $_ => 1 }
		map { s|.*|/lib/lib$&.so|r; } @ARGV}};

print "\n";
