#
# cepl - C Read-Eval-Print Loop
#
# AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
# See LICENSE.md file for copyright and license details.

DESTDIR ?=
PREFIX ?= /usr/local
CC := gcc
LD := $(CC)
TARGET_ARCH ?= -march=x86-64 -mtune=generic
CFLAGS := -pipe -MMD -flto -fPIC -fstack-protector-strong -fuse-linker-plugin -std=c11 -Wall -Wextra -Wimplicit-fallthrough=1 -pedantic-errors -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700
LDFLAGS := -pipe -MMD -flto -fPIC -fstack-protector-strong -fuse-linker-plugin -Wl,-O1,-zrelro,-znow,--sort-common,--as-needed
LDLIBS := -lreadline
DEBUG := -Og -ggdb
RELEASE := -O2
TARGET := cepl
ELF_SCRIPT := elfsyms
MANPAGE := cepl.7
TAP := t/tap

SRC := $(wildcard src/*.c)
TSRC := $(wildcard t/*.c)
OBJ := $(patsubst %.c,%.o,$(SRC))
TOBJ := $(patsubst %.c,%.o,$(TSRC))
HDR := $(wildcard src/*.h) $(wildcard t/*.h)
TESTS := $(filter-out $(TAP),$(patsubst %.c,%,$(TSRC)))

all: $(TARGET) check

%:
	$(LD) $(LDLIBS) $(LDFLAGS) $(TARGET_ARCH) $(filter %.o,$^) -o $@

%.o:
	$(CC) $(LDLIBS) $(CFLAGS) $(TARGET_ARCH) -c $(filter %.c,$^) -o $@

debug: CFLAGS := $(DEBUG) $(CFLAGS)
debug: $(OBJ)
	$(LD) $(LDLIBS) $(DEBUG) $(LDFLAGS) $(TARGET_ARCH) $(filter src/%.o,$^) -o $(TARGET)

$(TARGET): CFLAGS := $(RELEASE) $(CFLAGS)
$(TARGET): $(OBJ)

$(TESTS): %: %.o $(TAP).o $(filter $(subst t/test,src/,%),$(filter-out src/$(TARGET).o,$(OBJ)))

$(OBJ): %.o: %.c $(HDR)

$(TOBJ): %.o: %.c $(HDR)

check test: tests
	./t/testreadline <<<"test string."
	./t/testcompile
	./t/testparseopts

tests: $(TESTS)

install: $(TARGET)
	@printf "%s\n" "installing"
	@mkdir -pv $(DESTDIR)$(PREFIX)/bin
	@mkdir -pv $(DESTDIR)$(PREFIX)/share/man/man7
	install -c $(TARGET) $(DESTDIR)$(PREFIX)/bin
	install -c $(ELF_SCRIPT) $(DESTDIR)$(PREFIX)/bin
	install -c $(MANPAGE) $(DESTDIR)$(PREFIX)/share/man/man7

uninstall:
	@rm -fv $(DESTDIR)$(PREFIX)/bin/$(TARGET)
	@rm -fv $(DESTDIR)$(PREFIX)/bin/$(ELF_SCRIPT)
	@rm -fv $(DESTDIR)$(PREFIX)/share/man/man7/$(MANPAGE)

dist: clean
	@printf "%s\n" "creating dist tarball"
	@mkdir -pv $(TARGET)/
	@cp -Rv LICENSE Makefile README.md $(HDR) $(SRC) $(TSRC) $(ELF_SCRIPT) $(MANPAGE) $(TARGET)/
	tar -czf $(TARGET).tar.gz $(TARGET)/
	@rm -rfv $(TARGET)/

clean:
	@printf "%s\n" "cleaning"
	@rm -fv $(TARGET) $(TESTS) $(OBJ) $(TOBJ) $(TARGET).tar.gz $(wildcard t/*.d) $(wildcard src/*.d)

-include $(wildcard src/*.d) $(wildcard t/*.d)
.PHONY: all clean install uninstall dist debug check test tests
