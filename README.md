# CEPL - *C Read-Eval-Print Loop*

**CEPL** is a command-line, interactive C11 Read-Eval-Print Loop,
useful for instantaneous prototyping, mathematical calculations, and
general algorithm exploration.

**CEPL** currently supports multiple compilers, readline
key-bindings/tab-completion, and the ability to specify additional
shared libraries or headers with the `-l` or `-I` switches respectively.

A list of completions can also be generated on-demand for each `-l` library
using `nm` and `perl` by passing the `-p` switch.

## Dependencies

* gcc (GNU C Compiler)
* readline library
> **Optional:**
> * perl
> * nm

## Usage
```bash
./cepl [-hpvw] [-c<compiler>] [-l<library name>] [-I<additional header directory>] [-o<output.c>]
```

Run `make` then `./cepl` to start the interactive REPL.

#### CEPL understands the following control sequences:

Input lines prefixed with a `;` are used to control internal state.

	;f[unction]:		Define a function (e.g. “;f void foo(void) { … }”)
	;h[elp]:		Show help
	;i[nclude]:		Define an include (e.g. “;i #include <crypt.h>”)
	;m[acro]:		Define a macro (e.g. “;m #define ZERO(x) (x ^ x)”)
	;p[arse]:		Toggle -p (shared library parsing) flag
	;q[uit]:		Exit CEPL
	;r[eset]:		Reset CEPL to its initial program state
	:w[arnings]:		Toggle -w (warnings) flag

#### CEPL understands the following options:

	-h,--help:		Show help/usage information.
	-p,--parse:		Add symbols from dynamic libraries to readline completion.
	-v,--version:		Show version information.
	-w,--warnings:		Compile with “-pedantic-errors -Wall -Wextra” flags.
	-c,--compiler:		Specify alternate compiler.
	-l:			Link against specified library (flag can be repeated).
	-I:			Search directory for header files (flag can be repeated).
	-o:			Name of the file to output source to.

## Libraries used:

* readline
* libtap ([zorgnax/libtap](https://github.com/zorgnax/libtap))
