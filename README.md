# CEPL - *C Read-Eval-Print Loop*

**CEPL** is a command-line, interactive **C11 REPL**, useful
for instantaneous prototyping, mathematical calculations, and
general algorithm exploration. Currently supports readline
key-bindings/tab-completion and ability to specify additional
libraries or headers with command-line switches.

> **WIP**: Some features not fully implemented yet.

## Dependencies

Recent versions of the `readline` library and `gcc`.

## Usage
```bash
./cepl [-hv] [-l<library name>] [-I<additional header directory>] [-o<output.c>]
```

Run `make` then `./cepl` to start the interactive REPL.

#### CEPL understands the following control sequences:

Any input lines prefixed with a `;` are used to control the internal state.

	;r[eset]: Reset CEPL to its initial program state

#### CEPL understands the following options:

	-h: Show help/usage information.
	-v: Show version information.
	-l: Link against specified library (flag can be repeated).
	-I: Search directory for header files (flag can be repeated).
	-o: Name of the file to output source to.

## Libraries used:

* readline
* libtap ([zorgnax/libtap](https://github.com/zorgnax/libtap))
