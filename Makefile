#
# cepl - C Read-Eval-Print Loop
#
# AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
# See LICENSE.md file for copyright and license details.

DESTDIR ?=
PREFIX ?= /usr/local
CC ?= gcc
LD := $(CC)
CPPFLAGS := -D_FORTIFY_SOURCE=2 -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700
CFLAGS := -pipe -MMD -fPIC -fstack-protector-strong -std=c11 -Wall -Wextra -Wimplicit-fallthrough -Wno-unused-parameter -pedantic-errors
LDFLAGS := -pipe -MMD -fPIC -fstack-protector-strong -Wl,-O1,-z,relro,-z,now,--sort-common,--as-needed
LIBS := -lelf -lhistory -lreadline
DEBUG := -O1 -ggdb
RELEASE := -O2
TARGET := cepl
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
	$(LD) $(LDFLAGS) $(filter %.o,$^) $(LIBS) -o $@

%.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $(filter %.c,$^) -o $@

debug: CFLAGS := $(DEBUG) $(CFLAGS)
debug: LDFLAGS := $(DEBUG) $(LDFLAGS)
debug: $(OBJ)
	$(LD) $(LDFLAGS) $(filter src/%.o,$^) $(LIBS) -o $(TARGET)

$(TARGET): CFLAGS := $(RELEASE) $(CFLAGS)
$(TARGET): LDFLAGS := $(RELEASE) $(LDFLAGS)
$(TARGET): $(OBJ)

$(TESTS): CFLAGS := $(DEBUG) $(CFLAGS)
$(TESTS): LDFLAGS := $(DEBUG) $(LDFLAGS)
$(TESTS): %: %.o $(TAP).o $(filter $(subst t/test,src/,%),$(filter-out src/$(TARGET).o,$(OBJ)))

$(OBJ): %.o: %.c $(HDR)

$(TOBJ): %.o: %.c $(HDR)

check test: tests
	printf "test string\n" | ./t/testreadline
	./t/testcompile
	./t/testparseopts

tests: $(TESTS)

install: $(TARGET)
	@printf "%s\n" "installing"
	@mkdir -pv $(DESTDIR)$(PREFIX)/bin
	@mkdir -pv $(DESTDIR)$(PREFIX)/share/man/man7
	install -c $(TARGET) $(DESTDIR)$(PREFIX)/bin
	install -c $(MANPAGE) $(DESTDIR)$(PREFIX)/share/man/man7

uninstall:
	@rm -fv $(DESTDIR)$(PREFIX)/bin/$(TARGET)
	@rm -fv $(DESTDIR)$(PREFIX)/share/man/man7/$(MANPAGE)

dist: clean
	@printf "%s\n" "creating dist tarball"
	@mkdir -pv $(TARGET)/
	@cp -Rv LICENSE.md Makefile README.md $(HDR) $(SRC) $(TSRC) $(MANPAGE) $(TARGET)/
	tar -czf $(TARGET).tar.gz $(TARGET)/
	@rm -rfv $(TARGET)/

clean:
	@printf "%s\n" "cleaning"
	@rm -fv $(TARGET) $(TESTS) $(OBJ) $(TOBJ) $(TARGET).tar.gz $(wildcard t/*.d) $(wildcard src/*.d)

-include $(wildcard src/*.d) $(wildcard t/*.d)
.PHONY: all clean install uninstall dist debug check test tests
