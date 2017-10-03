#
# cepl - C Read-Eval-Print Loop
#
# AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
# See LICENSE.md file for copyright and license details.

DESTDIR ?=
PREFIX ?= /usr/local
CC ?= gcc
LD := $(CC)
CPPFLAGS ?= -D_FORTIFY_SOURCE=2 -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -MMD -MP
CFLAGS ?= -pipe -fstack-protector-strong -fuse-ld=gold
CFLAGS += -fuse-ld=gold -Wl,-O2,-z,relro,-z,now,--sort-common,--as-needed
LDFLAGS ?= -pipe -fstack-protector-strong
LDFLAGS += -fuse-ld=gold -Wl,-O2,-z,relro,-z,now,--sort-common,--as-needed
OLVL ?= -O2
LIBS := -lelf -lhistory -lreadline
DEBUG := -Og -ggdb3 -no-pie -Wfloat-equal -Wrestrict -Wshadow -fsanitize=address,alignment,leak,undefined
TARGET := cepl
MANPAGE := cepl.7
TAP := t/tap
BINDIR := $(DESTDIR)$(PREFIX)/bin
MANDIR := $(DESTDIR)$(PREFIX)/share/man/man7
SRC := $(wildcard src/*.c)
TSRC := $(wildcard t/*.c)
HDR := $(wildcard src/*.h) $(wildcard t/*.h)
TEST := $(filter-out $(TAP),$(TSRC:.c=))
DEP := $(SRC:.c=.d) $(TSRC:.c=.d)
OBJ := $(SRC:.c=.o)
TOBJ := $(TSRC:.c=.o)
UOBJ := $(filter-out src/$(TARGET).o,$(OBJ))

all: $(TARGET)
	$(MAKE) check
debug:
	@touch debug.mk
	$(MAKE) check OLVL="$(DEBUG)"

# if previously built with `-fsanitize=address` we have to use `DEBUG` flags
OPT != test -f debug.mk
ifeq ($(.SHELLSTATUS),0)
	OLVL = $(DEBUG)
endif
-include $(DEP)
.PHONY: all check clean debug dist install test uninstall Makefile debug.mk $(DEP)

$(TARGET): %: $(OBJ)
	$(LD) $(LDFLAGS) $(OLVL) $(LIBS) -o $@ $^
$(TEST): %: %.o $(TAP).o $(UOBJ)
	$(LD) $(LDFLAGS) $(OLVL) $(LIBS) -o $@ $^
%.d %.o: %.c
	$(CC) $(CFLAGS) $(OLVL) $(CPPFLAGS) -c -o $@ $<

test check: $(TOBJ) $(TEST)
	./t/testcompile
	./t/testhist
	./t/testparseopts
	echo "test string" | ./t/testreadline
	./t/testvars
clean:
	@echo "cleaning"
	@rm -fv $(DEP) $(TARGET) $(TEST) $(OBJ) $(TOBJ) $(TARGET).tar.gz
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

