#
# config.mk - Makefile configuration variables
#
# AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
# See LICENSE.md file for copyright and license details.

# optional
DESTDIR ?=
PREFIX ?= /usr/local
CC ?= gcc
OLVL ?= -O2
LIBS ?= $(READLINE)/lib/libreadline.a $(READLINE)/lib/libhistory.a \
		$(shell pkg-config ncursesw --libs --cflags 2>/dev/null || \
			pkg-config ncurses --libs --cflags 2>/dev/null || \
			printf '%s' '-D_GNU_SOURCE -D_DEFAULT_SOURCE -lncursesw -ltinfo')
ifneq "$(origin LIBS)" "command line"
	CFLAGS += -I$(READLINE)/include
else
	CFLAGS += -DRL_OVERRIDE
endif

# mandatory
LD = $(CC)
DEBUG_CFLAGS = $(CFLAGS) $(DEBUG)
DEBUG_LDFLAGS = $(LDFLAGS) $(DEBUG)
MKALL = $(MKCFG) $(DEP)
OBJ = $(SRC:.c=.o)
TOBJ = $(TSRC:.c=.o)
DEP = $(SRC:.c=.d) $(TSRC:.c=.d)
TEST = $(filter-out $(TAP),$(TSRC:.c=))
UTEST = $(filter-out src/$(TARGET).o,$(SRC:.c=.o))
SRC := $(wildcard src/*.c)
TSRC := $(wildcard t/*.c)
HDR := $(wildcard src/*.h) $(wildcard t/*.h)
# check for gcc7
DEBUG := $(shell gcc -v 2>&1 | awk '/version/ { if (substr($$3, 1, 1) == 7) { print "-Wrestrict" } }')
ASAN := -fsanitize=address,alignment,leak,undefined
CPPFLAGS := -D_FORTIFY_SOURCE=2 -D_GNU_SOURCE -MMD -MP
TARGET := cepl
MANPAGE := cepl.1
COMPLETION := _cepl
TAP := t/tap
BINDIR := bin
MANDIR := share/man/man1
COMPDIR := share/zsh/site-functions
READLINE := readline
ELF_LIBS := -lelf
WARNINGS := -Wall -Wextra -pedantic \
		-Wcast-align -Wfloat-equal -Winline -Wmissing-declarations \
		-Wmissing-prototypes -Wnested-externs -Wpointer-arith \
		-Wshadow -Wstrict-overflow -Wwrite-strings
IGNORES := -Wno-conversion -Wno-cpp -Wno-discarded-qualifiers \
		-Wno-implicit-fallthrough -Wno-long-long \
		-Wno-missing-field-initializers -Wno-redundant-decls \
		-Wno-sign-conversion -Wno-strict-prototypes \
		-Wno-unused-variable
MKALL += Makefile asan.mk
DEBUG += -O0 -g3 -no-pie -D_DEBUG
DEBUG += -fno-inline -fno-builtin -fno-common
DEBUG += -fverbose-asm
CFLAGS += -g3 -std=c11 $(WARNINGS) $(IGNORES)
CFLAGS += -fPIC -fuse-ld=gold -flto -fuse-linker-plugin -fno-strict-aliasing
LDFLAGS += -Wl,-O2,-z,relro,-z,now,--sort-common,--as-needed
LDFLAGS += -fPIC -fuse-ld=gold -flto -fuse-linker-plugin -fno-strict-aliasing

# vi:ft=make:
