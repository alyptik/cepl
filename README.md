# cepl - *a C and C++ REPL*

A readline C and C++ REPL with history, tab-completion, and undo.

## Dependencies

* `gcc` GNU C compiler
* `readline` Console display library
* `libelf` ELF access library

## Usage
```bash
./cepl [-hpvw] [-c<compiler>] [-e<code to evaluate>] [-f<file> ] [-l<library>] [-I<include directory>] [-L<library directory>] [-s<standard>] [-o<out.c>]
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

When the `-l` flag is passed, the library argument is scanned for symbols
which are then added to readline completion.

#### Command line options:

	-c, --compiler		Specify alternate compiler
	-e, --eval		    Evaluate the following argument as C/C++ code
	-h, --help		    Show help/usage information
	-o, --output		Name of the file to output C/C++ code to
	-p, --parse		    Disable addition of dynamic library symbols to readline completion
	-s, --std		    Specify which C/C++ standard to use
	-v, --version		Show version information
	-w, --warnings		Compile with "-Wall -Wextra -pedantic" flags
	-l			        Link against specified library (flag can be repeated)
	-I			        Search directory for header files (flag can be repeated)
	-L			        Search directory for libraries (flag can be repeated)

#### Lines prefixed with a `;` are interpreted as commands (`[]` text is optional).

	;f[unction]		Line is defined outside of main() (e.g. ;m #define SWAP2(X) ((((X) >> 8) & 0xff) | (((X) & 0xff) << 8)))
	;h[elp]			Show help
	;m[an]			Show manpage for argument (e.g. ;m strpbrk)
	;q[uit]			Exit CEPL
	;r[eset]		Reset CEPL to its initial program state
	;u[ndo]			Incremental undo (can be repeated)
