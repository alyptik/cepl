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
CFLAGS := -pipe -MMD -MP -fstack-protector-strong -fuse-ld=gold -std=c11 -pedantic-errors -Wall -Wextra -O2
LDFLAGS := -pipe -MMD -MP -fstack-protector-strong -fuse-ld=gold -Wl,-O2,-z,relro,-z,now,--sort-common,--as-needed -O2
LIBS := -lelf -lhistory -lreadline
DEBUG := -ggdb3 -no-pie -Wfloat-equal -Wrestrict -Wshadow -fsanitize=address,alignment,leak,undefined -Og
TARGET := cepl
MANPAGE := cepl.7
TAP := t/tap

SRC := $(wildcard src/*.c)
TSRC := $(wildcard t/*.c)
HDR := $(wildcard src/*.h) $(wildcard t/*.h)
TESTS := $(filter-out $(TAP),$(TSRC:.c=))
OBJ := $(SRC:.c=.o)
TOBJ := $(TSRC:.c=.o)
DEPS := $(TARGET:=.d) $(SRC:.c=.d) $(TSRC:.c=.d)
BINDIR := $(DESTDIR)$(PREFIX)/bin
MANDIR := $(DESTDIR)$(PREFIX)/share/man/man7

-include $(DEPS)
.PHONY: all check checks clean debug dist install Makefile test tests uninstall

all: $(TARGET)
	$(MAKE) check
debug: CFLAGS := $(CFLAGS) $(DEBUG)
debug: LDFLAGS := $(LDFLAGS) $(DEBUG)
debug: %: $(OBJ)
	$(LD) $(LDFLAGS) $(LIBS) -o $(TARGET) $^
tests checks: $(TESTS)
test check: $(OBJ) tests
	./t/testcompile
	./t/testhist
	./t/testparseopts
	echo "test string" | ./t/testreadline
	./t/testvars

clean:
	@echo "cleaning"
	@rm -fv $(DEPS) $(TARGET) $(TESTS) $(OBJ) $(TOBJ) $(TARGET).tar.gz
install: $(TARGET)
	@echo "installing"
	@mkdir -pv $(DESTDIR)$(PREFIX)/bin
	@mkdir -pv $(DESTDIR)$(PREFIX)/share/man/man7
	install -c $(TARGET) $(BINDIR)
	install -c $(MANPAGE) $(MANDIR)
uninstall:
	@rm -fv $(DESTDIR)$(PREFIX)/bin/$(TARGET)
	@rm -fv $(DESTDIR)$(PREFIX)/share/man/man7/$(MANPAGE)
dist: clean
	@echo "creating dist tarball"
	@mkdir -pv $(TARGET)/
	@cp -Rv LICENSE.md Makefile README.md $(HDR) $(SRC) $(TSRC) $(MANPAGE) $(TARGET)/
	tar -czf $(TARGET).tar.gz $(TARGET)/
	@rm -rfv $(TARGET)/

$(TARGET): %: $(OBJ)
	$(LD) $(LDFLAGS) $(LIBS) -o $@ $^
$(TESTS): %: %.o $(TAP).o $(filter $(subst t/test,src/,%),$(filter-out src/$(TARGET).o,$(OBJ)))
	$(LD) $(LDFLAGS) $(LIBS) -o $@ $^
$(OBJ): %.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<
$(TOBJ): %.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<
