# CEPL - *C Read-Eval-Print Loop*

**CEPL** is a command-line, interactive **C11 REPL**, useful
for instantaneous prototyping, mathematical calculations, and
general algorithm exploration. Currently supports readline
key-bindings/tab-completion and linking against arbitrary
libraries with command-line switches.

> **WIP**: Some features not fully implemented yet.

## Dependencies

Recent versions of the `readline` library and `gcc`.

## Usage
```bash
./cepl [-hv] [-l<library>] [-I<include dir>] [-o<output.c>]
```

Run `make` then `./cepl` to start the interactive REPL.

#### CEPL understands the following control sequences:

Any input lines prefixed with a `;` are used to control the internal state.

	;r[eset]: Reset CEPL to its initial program state

#### CEPL understands the following options:

	-h: Show help/usage information.
	-v: Show version.
	-l: Link against specified library (can be repeated).
	-I: Search directory for header files (can be repeated).

_Not yet implemented (WIP):_

	-o: Name of file to save source output in.

## Libraries used:

* readline
* libtap ([zorgnax/libtap](https://github.com/zorgnax/libtap))
