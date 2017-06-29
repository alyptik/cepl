#
# cepl - C Read-Eval-Print Loop
#
# AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
# See LICENSE file for copyright and license details.

CC ?= gcc
LD ?= $(CC)
PREFIX ?= $(DESTDIR)/usr/local
TARGET_ARCH ?= -march=x86-64 -mtune=generic
CFLAGS = -O2 -pipe -MMD -Isrc -fPIC -fstack-protector-strong -Wall -Wextra -std=c11 -pedantic-errors -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE
LDFLAGS = -Wl,-O1,--sort-common,--as-needed,-z,relro,-z,now
DEBUG = -Og -ggdb -pipe -MMD -Isrc -fPIC -fstack-protector-strong -Wall -Wextra -std=c11 -pedantic-errors -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE
LDLIBS = -lreadline
TAP = t/tap
SRC = $(wildcard src/*.c)
TSRC = $(wildcard t/*.c)
OBJ = $(patsubst %.c, %.o, $(SRC))
TOBJ = $(patsubst %.c, %.o, $(TSRC))
HDR = $(wildcard src/*.h) $(wildcard t/*.h)
TESTS = $(filter-out $(TAP), $(patsubst %.c, %, $(TSRC)))

TARGET = cepl
ELF_SCRIPT = elfsyms
MANPAGE = $(TARGET).7

all: $(TARGET) check

debug: CFLAGS = $(DEBUG)
debug: $(OBJ) $(TOBJ)
	$(CC) $(LDLIBS) $(LDFLAGS) $(TARGET_ARCH) $(filter src/%.o, $^) -o $(TARGET)

%:
	$(CC) $(LDLIBS) $(LDFLAGS) $(TARGET_ARCH) $(filter %.o, $^) -o $@

%.o:
	$(CC) $(CFLAGS) $(LDLIBS) $(TARGET_ARCH) -c $(filter %.c, $^) -o $@

$(TARGET): $(OBJ)

$(OBJ): %.o: %.c $(HDR)

$(TESTS): %: %.o $(TAP).o $(filter $(subst t/test, src/, %), $(filter-out src/$(TARGET).o, $(OBJ)))

$(TOBJ): %.o: %.c $(HDR)

check test: tests
	./t/testreadline <<<"test string."
	./t/testcompile
	./t/testparseopts

tests: $(TESTS)

install: $(TARGET)
	@printf "%s\n" "installing"
	@mkdir -pv $(PREFIX)/bin
	@mkdir -pv $(PREFIX)/share/man/man7
	install -c $(TARGET) $(PREFIX)/bin
	install -c $(ELF_SCRIPT) $(PREFIX)/bin
	install -c $(MANPAGE) $(PREFIX)/share/man/man7

uninstall:
	@rm -fv $(PREFIX)/bin/$(TARGET)
	@rm -fv $(PREFIX)/bin/$(ELF_SCRIPT)
	@rm -fv $(PREFIX)/share/man/man7/$(MANPAGE)

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
