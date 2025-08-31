#
# cepl - a C and C++ REPL
#
# AUTHOR: Joey Pabalinas <joeypabalinas@gmail.com>
# See LICENSE file for copyright and license details.

# optional
DESTDIR ?=
PREFIX ?= /usr/local
CC ?= gcc

# mandatory
LD = $(CC)
DEBUG_CFLAGS = $(CFLAGS) $(DEBUG)
DEBUG_LDFLAGS = $(LDFLAGS)
OBJ = $(SRC:.c=.o)
DEP = $(SRC:.c=.d)
SRC := $(wildcard src/*.c)
HDR := $(wildcard src/*.h)
CPPFLAGS := -D_GNU_SOURCE -MMD -MP
BINDIR := bin
MANDIR := share/man/man1
COMPDIR := share/zsh/site-functions
TARGET := cepl
MANPAGE := cepl.1
COMPLETION := _cepl
WARNINGS := -Wrestrict -Wall -Wextra -pedantic				\
		-Wcast-align -Wfloat-equal -Wmissing-declarations	\
		-Wmissing-prototypes -Wnested-externs -Wpointer-arith	\
		-Wshadow -Wstrict-overflow
IGNORES := -Wno-conversion -Wno-cpp -Wno-implicit-fallthrough		\
	   	-Wno-inline -Wno-long-long				\
		-Wno-missing-field-initializers -Wno-redundant-decls	\
		-Wno-sign-conversion -Wno-strict-prototypes		\
		-Wno-unused-variable -Wno-write-strings
LIBS += -lreadline -lhistory -lelf
DEBUG += -g3 -D_DEBUG
DEBUG += -fno-builtin -fno-inline
CFLAGS += $(WARNINGS) $(IGNORES)

# include deps
-include $(DEP)
.PHONY: all clean debug dist install uninstall

# targets
all:
	$(MAKE) $(TARGET)

debug:
	CFLAGS="$(DEBUG_CFLAGS)" LDFLAGS="$(DEBUG_LDFLAGS)" $(MAKE) $(TARGET)

$(TARGET): %: $(OBJ)
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@
%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

clean:
	@echo "[cleaning]"
	$(RM) $(TARGET) $(OBJ) $(DEP) cscope.* tags TAGS \
		cepl-$(shell sed '1!d; s/.*cepl-\([0-9][0-9]*\)\\\&\.\([0-9][0-9]*\)\\\&\.\([0-9][0-9]*\).*/\1.\2.\3/' cepl.1).tar.gz
install: $(TARGET)
	@echo "[installing]"
	mkdir -p $(DESTDIR)$(PREFIX)/$(BINDIR)
	mkdir -p $(DESTDIR)$(PREFIX)/$(MANDIR)
	mkdir -p $(DESTDIR)$(PREFIX)/$(COMPDIR)
	install -c $(TARGET) $(DESTDIR)$(PREFIX)/$(BINDIR)
	bzip2 -c $(MANPAGE) > $(DESTDIR)$(PREFIX)/$(MANDIR)/$(MANPAGE).bz2
	cat $(COMPLETION) > $(DESTDIR)$(PREFIX)/$(COMPDIR)/$(COMPLETION)
uninstall:
	@echo "[uninstalling]"
	$(RM) $(DESTDIR)$(PREFIX)/$(BINDIR)/$(TARGET)
	$(RM) $(DESTDIR)$(PREFIX)/$(MANDIR)/$(MANPAGE).bz2
	$(RM) $(DESTDIR)$(PREFIX)/$(COMPDIR)/$(COMPLETION)
dist: clean
	@echo "[creating source tarball]"
	tar cf cepl-$(shell sed '1!d; s/.*cepl-\([0-9][0-9]*\)\\\&\.\([0-9][0-9]*\)\\\&\.\([0-9][0-9]*\).*/\1.\2.\3/' cepl.1).tar \
		LICENSE Makefile README.md _cepl cepl.1 cepl.gif cepl.json src
	gzip cepl-$(shell sed '1!d; s/.*cepl-\([0-9][0-9]*\)\\\&\.\([0-9][0-9]*\)\\\&\.\([0-9][0-9]*\).*/\1.\2.\3/' cepl.1).tar
cscope:
	@echo "[creating cscope database]"
	$(RM) cscope.*
	cscope -Rub
tags TAGS:
	@echo "[creating tag file]"
	$(RM) tags TAGS
	ctags -Rf $@ --tag-relative=yes --langmap=c:+.h.C.H --fields=+l --c-kinds=+p --c++-kinds=+p --fields=+iaS --extras=+q .
