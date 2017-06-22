# CEPL - *C11 Read-Eval-Print Loop*

**CEPL** is a command-line, interactive **C11 REPL**, useful
for instantaneous prototyping, mathematical calculations, and
general algorithm exploration.

**WIP**

Some parts not fully implemented yet.

## Usage
```sh
./cepl [-hv] [-l<library>] [-I<include dir>] [-o<sessionlog.c>]
```

Run `make` then `./cepl` to start the interactive REPL.

#### CEPL understands the following options:

* -h: Show help.
* -v: Show version.
* -l: Link with specified library (can be repeated).
* -I: Search directory for header files (can be repeated).
* -o: Name of the file where the session transcript will be written.

## Libraries
> Uses the libtap unit-testing framework ([zorgnax/libtap](https://github.com/zorgnax/libtap))
* GNU Readline
* PCRE
