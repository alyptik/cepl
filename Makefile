#
# cepl - C REPL
#
# AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
# See LICENSE file for copyright and license details.

CC ?= gcc
LD ?= $(CC)
PREFIX ?= $(DESTDIR)/usr/local
TARGET_ARCH ?= -march=x86-64 -mtune=generic
CFLAGS = -O2 -pipe -MMD -I. -fPIC -fstack-protector-strong -Wall -Wextra -std=c11 -pedantic-errors -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L
LDFLAGS = -Wl,-O1,--sort-common,--as-needed,-z,relro -Wl,-z,now
LDLIBS = -lreadline $(shell pkg-config --cflags --libs libpcre)
TAP = t/tap
SRC = $(wildcard src/*.c)
TSRC = $(wildcard t/*.c)
OBJ = $(patsubst %.c, %.o, $(SRC))
TOBJ = $(patsubst %, %.o, $(TESTS))
HDR = $(wildcard src/*.h) $(wildcard t/*.h)

TESTS = $(filter-out $(TAP), $(patsubst %.c, %, $(TSRC)))
TARGET = cepl

%:
	$(CC) $(LDFLAGS) $(LDLIBS) $(TARGET_ARCH) $(filter %.o, $^) -o $@

%.o:
	$(CC) $(CFLAGS) $(LDLIBS) $(TARGET_ARCH) -c $(filter %.c, $^) -o $@

all: $(TARGET) tests

$(TARGET): $(OBJ)

$(OBJ): %.o: %.c $(HDR)

check test: tests
	./t/testcompile

tests: $(TESTS)

$(TESTS): %: %.o $(TAP).o $(filter $(subst t/test, src/, %), $(filter-out src/$(TARGET).o, $(OBJ)))

$(TOBJ): %.o: %.c $(HDR)

install: $(TARGET)
	@printf "%s\n" "installing"
	@mkdir -pv $(PREFIX)/bin
	install -c $(TARGET) $(prefix)/bin

uninstall:
	@rm -fv $(PREFIX)/bin/$(TARGET)

dist: clean
	@printf "%s\n" "creating dist tarball"
	@mkdir -pv $(TARGET)
	@cp -Rv LICENSE Makefile README.md $(HDR) $(SRC) $(TSRC) $(TARGET)
	tar -czf $(TARGET).tar.gz $(TARGET)
	@rm -rfv $(TARGET)

clean:
	@printf "%s\n" "cleaning"
	@rm -fv $(TARGET) $(OBJ) $(TOBJ) $(TESTS) $(TARGET).tar.gz $(wildcard t/*.d) $(wildcard src/*.d)

-include $(wildcard src/*.d) $(wildcard t/*.d)
.PHONY: all clean install uninstall dist check test tests
