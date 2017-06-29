# CEPL - *C Read-Eval-Print Loop*

**CEPL** is a command-line, interactive C11 Read-Eval-Print Loop,
useful for instantaneous prototyping, mathematical calculations, and
general algorithm exploration. It currently supports readline
key-bindings/tab-completion and the ability to specify additional
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
./cepl [-hpvw] [-l<library name>] [-I<additional header directory>] [-o<output.c>]
```

Run `make` then `./cepl` to start the interactive REPL.

#### CEPL understands the following control sequences:

Input lines prefixed with a `;` are used to control the internal state.

	;r[eset]: Reset CEPL to its initial program state

#### CEPL understands the following options:

	-h: Show help/usage information.
	-p: Add symbols from dynamic libraries to readline completion.
	-v: Show version information.
	-w: Compile with “-pedantic-errors -Wall -Wextra” flags.
	-l: Link against specified library (flag can be repeated).
	-I: Search directory for header files (flag can be repeated).
	-o: Name of the file to output source to.

## Libraries used:

* readline
* libtap ([zorgnax/libtap](https://github.com/zorgnax/libtap))
