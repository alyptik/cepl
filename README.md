# CEPL - *C Read-Eval-Print Loop*

![cepl](https://raw.githubusercontent.com/alyptik/cepl/master/cepl.gif)

An interactive C (ISO/IEC 9899:2011) read–eval–print loop (REPL) currently
supporting multiple compilers, readline key-bindings/tab-completion,
incremental undo, and assembler source code output.

## Dependencies

* `gcc` (GNU C Compiler)
* `readline` library
* `libelf` ELF access library

## Usage
```bash
./cepl [-hptvw] [-(a|i)<asm.s>] [-c<compiler>] [-e<code>] [-l<libs>] [-I<includes>] [-o<out.c>]
```

Run `make` then `./cepl` to start the interactive REPL.

Command history is read from and saved to `~/.cepl_history`.

#### CEPL understands the following options:

	-a, --att		Name of the file to output AT&T-dialect assembler code to
	-c, --cc		Specify alternate compiler
	-e, --eval		Evaluate the following argument as C code
	-h, --help		Show help/usage information
	-i, --intel		Name of the file to output Intel-dialect assembler code to
	-o, --output		Name of the file to output C source code to
	-p, --parse		Disable addition of dynamic library symbols to readline completion
	-t, --tracking		Toggle variable tracking
	-v, --version		Show version information
	-w, --warnings		Compile with "-pedantic -Wall -Wextra" flags
	-l			Link against specified library (flag can be repeated)
	-I			Search directory for header files (flag can be repeated)

#### CEPL understands the following control sequences:

Lines prefixed with a `;` are interpreted as commands (`[]` text is optional).

	;a[tt]			Toggle -a (output AT&T-dialect assembler code) flag
	;f[unction]		Define a function (e.g. ";f void bork(void) { puts("wark"); }")
	;h[elp]			Show help
	;i[ntel]		Toggle -i (output Intel-dialect assembler code) flag
	;m[acro]		Define a macro (e.g. ";m #define SWAP2(X) ((((X) >> 8) & 0xff) | (((X) & 0xff) << 8))")
	;o[utput]		Toggle -o (output C source code) flag
	;p[arse]		Toggle -p (shared library parsing) flag
	;q[uit]			Exit CEPL
	;r[eset]		Reset CEPL to its initial program state
	;t[racking]		Toggle variable tracking
	;u[ndo]			Incremental undo (can be repeated)
	;w[arnings]		Toggle -w (pedantic warnings) flag

## Libraries used:

* libtap ([zorgnax/libtap](https://github.com/zorgnax/libtap))
* libelf
* libreadline
