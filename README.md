# cepl - *a C and C++ REPL*

![cepl](https://raw.githubusercontent.com/alyptik/cepl/master/cepl.gif)

A readline C and C++ REPL with history, tab-completion, and undo.

## Dependencies

* `gcc` (GNU C Compiler)
* `readline` Console display library
* `libelf` ELF access library

## Usage
```bash
./cepl [-hptvw] [-c<compiler>] [-e<code to evaluate>] [-f<file> ] [-l<library>] [-I<include directory>] [-o<out.c>]
```
Run `make` then `./cepl` to start the interactive REPL.

To install after building, set the environment variables `DESTDIR`
and `PREFIX` if needed, then run `make install`:

    PREFIX=/usr make install

to install everything to `/usr`.

The following environment variables are respected: `CFLAGS`, `LDFLAGS`,
`LDLIBS`, and `LIBS`.

Command history is read from and saved to `~/.cepl_history`.

To switch between C/C++ modes, specify your C or C++ compiler
with `-c` such as:

    cepl -cg++ -lboost_filesystem -lboost_context -lboost_system

to run in C++ mode linking against Boost and compiling with g++, or:

    cepl -cclang -lelf -oout.c

to run in C mode linking against libelf and compiling with clang, saving
your code to `out.c` on exit.

#### Command line options:

	-c, --compiler		Specify alternate compiler
	-e, --eval		Evaluate the following argument as C/C++ code
	-f, --file		Source file to import
	-h, --help		Show help/usage information
	-o, --output		Name of the file to output C/C++ code to
	-p, --parse		Disable addition of dynamic library symbols to readline completion
	-t, --tracking		Toggle variable tracking
	-v, --version		Show version information
	-w, --warnings		Compile with "-Wall -Wextra -pedantic" flags
	-l			Link against specified library (flag can be repeated)
	-I			Search directory for header files (flag can be repeated)

#### Lines prefixed with a `;` are interpreted as commands (`[]` text is optional).

	;h[elp]			Show help
	;m[acro]		Define a macro (e.g. ";m #define SWAP2(X) ((((X) >> 8) & 0xff) | (((X) & 0xff) << 8))")
	;o[utput]		Toggle -o (output C source code) flag
	;p[arse]		Toggle -p (shared library parsing) flag
	;q[uit]			Exit CEPL
	;r[eset]		Reset CEPL to its initial program state
	;t[racking]		Toggle variable tracking
	;u[ndo]			Incremental undo (can be repeated)
	;w[arnings]		Toggle -w (pedantic warnings) flag

## Required libraries:

* libelf
* libreadline
