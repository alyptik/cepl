/*
libtap - Write tests in C
Copyright 2012 Jake Gelbman <gelbman@gmail.com>
This file is licensed under the LGPL
*/

#ifndef __TAP_H__
#define __TAP_H__

#ifdef __INTEL_COMPILER
#  define float_t long double
#  define _Float128 float_t
#else
#  include <complex.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef va_copy
#ifdef __va_copy
#define va_copy __va_copy
#else
#define va_copy(d, s) ((d) = (s))
#endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

int     vok_at_loc      (char const *file, int line, int test, char const *fmt, va_list args);
int     ok_at_loc       (char const *file, int line, int test, char const *fmt, ...);
int     is_at_loc       (char const *file, int line, char const *got, char const *expected, char const *fmt, ...);
int     isnt_at_loc     (char const *file, int line, char const *got, char const *expected, char const *fmt, ...);
int     cmp_ok_at_loc   (char const *file, int line, int a, char const *op, int b, char const *fmt, ...);
int     cmp_mem_at_loc  (char const *file, int line, void const *got, void const *expected, size_t n, char const *fmt, ...);
int     bail_out        (int ignore, char const *fmt, ...);
void    tap_plan        (int tests, char const *fmt, ...);
int     diag            (char const *fmt, ...);
int     exit_status     (void);
void    tap_skip        (int n, char const *fmt, ...);
void    tap_todo        (int ignore, char const *fmt, ...);
void    tap_end_todo    (void);

#define NO_PLAN          -1
#define SKIP_ALL         -2
#define ok(...)          ok_at_loc(__FILE__, __LINE__, __VA_ARGS__, NULL)
#define is(...)          is_at_loc(__FILE__, __LINE__, __VA_ARGS__, NULL)
#define isnt(...)        isnt_at_loc(__FILE__, __LINE__, __VA_ARGS__, NULL)
#define cmp_ok(...)      cmp_ok_at_loc(__FILE__, __LINE__, __VA_ARGS__, NULL)
#define cmp_mem(...)     cmp_mem_at_loc(__FILE__, __LINE__, __VA_ARGS__, NULL)
#define plan(...)        tap_plan(__VA_ARGS__, NULL)
#define done_testing()   return exit_status()
#define BAIL_OUT(...)    bail_out(0, "" __VA_ARGS__, NULL)
#define pass(...)        ok(1, "" __VA_ARGS__)
#define fail(...)        ok(0, "" __VA_ARGS__)

#define skip(test, ...)  do {if (test) {tap_skip(__VA_ARGS__, NULL); break;}
#define end_skip         } while (0)

#define todo(...)        tap_todo(0, "" __VA_ARGS__, NULL)
#define end_todo         tap_end_todo()

#define dies_ok(...)     dies_ok_common(1, __VA_ARGS__)
#define lives_ok(...)    dies_ok_common(0, __VA_ARGS__)

#ifdef _WIN32
#define like(...)        tap_skip(1, "like is not implemented on Windows")
#define unlike           tap_skip(1, "unlike is not implemented on Windows")
#define dies_ok_common(...) tap_skip(1, "Death detection is not supported on Windows")
#else
#define like(...)        like_at_loc(1, __FILE__, __LINE__, __VA_ARGS__, NULL)
#define unlike(...)      like_at_loc(0, __FILE__, __LINE__, __VA_ARGS__, NULL)
int     like_at_loc     (int for_match, char const *file, int line, char const *got, char const *expected, char const *fmt, ...);
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
int tap_test_died (int status);
#define dies_ok_common(for_death, code, ...) \
	do { \
		int cpid; \
		int it_died; \
		tap_test_died(1); \
		cpid = fork(); \
		switch (cpid) { \
		case -1: \
			perror("fork error"); \
			exit(1); \
		case 0: \
			close(1); \
			close(2); \
			code \
			tap_test_died(0); \
			exit(0); \
		} \
		if (waitpid(cpid, NULL, 0) < 0) { \
			perror("waitpid error"); \
			exit(1); \
		} \
		it_died = tap_test_died(0); \
		if (!it_died) \
			{code} \
		ok(for_death ? it_died : !it_died, "" __VA_ARGS__); \
	} while (0)
#endif

#ifdef __cplusplus
}
#endif

#endif
