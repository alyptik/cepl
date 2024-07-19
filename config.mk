#
# config.mk - Makefile configuration variables
#
# AUTHOR: Joey Pabalinas <joeypabalinas@gmail.com>
# See LICENSE.md file for copyright and license details.

# add -Wrestrict if using gcc7 or higher
RESTRICT := $(shell gcc -v 2>&1 | awk '/version/ { if (substr($$3, 1, 1) > 6) { print "-Wrestrict"; } }')

# optional
DESTDIR ?=
PREFIX ?= /usr/local
CC ?= gcc
OLVL ?= -O3

# mandatory
LD = $(CC)
DEBUG_CFLAGS = $(CFLAGS) $(DEBUG)
DEBUG_LDFLAGS = $(LDFLAGS) $(DEBUG)
MKALL = $(MKCFG) $(DEP)
OBJ = $(SRC:.c=.o)
TOBJ = $(TSRC:.c=.o)
DEP = $(SRC:.c=.d) $(TSRC:.c=.d)
TEST = $(TSRC:.c=)
UTEST = $(filter-out src/$(TARGET).o,$(SRC:.c=.o))
SRC := $(wildcard src/*.c)
TSRC := $(wildcard t/*.c)
HDR := $(wildcard src/*.h) $(wildcard t/*.h)
ASAN := -fsanitize=address,alignment,leak,undefined
CPPFLAGS := -D_FORTIFY_SOURCE=2 -D_GNU_SOURCE -MMD -MP
VENDOR := vendor
CONTRIB := contrib
BINDIR := bin
MANDIR := share/man/man1
COMPDIR := share/zsh/site-functions
TARGET := cepl
MANPAGE := cepl.1
COMPLETION := _cepl
TAP := $(CONTRIB)/libtap
WARNINGS := $(RESTRICT) -Wall -Wextra -pedantic				\
		-Wcast-align -Wfloat-equal -Wmissing-declarations	\
		-Wmissing-prototypes -Wnested-externs -Wpointer-arith	\
		-Wshadow -Wstrict-overflow
IGNORES := -Wno-conversion -Wno-cpp -Wno-implicit-fallthrough		\
	   	-Wno-inline -Wno-long-long				\
		-Wno-missing-field-initializers -Wno-redundant-decls	\
		-Wno-sign-conversion -Wno-strict-prototypes		\
		-Wno-unused-variable -Wno-write-strings
LDLIBS += -lreadline -lhistory -lelf
LDLIBS += $(shell pkg-config ncursesw --cflags --libs || pkg-config ncurses --cflags --libs)
DEBUG += -O1 -D_DEBUG
DEBUG += -fno-builtin -fno-inline
DEBUG += -I$(TAP)
CFLAGS += -std=gnu2x -fstack-protector-strong
CFLAGS += $(WARNINGS) $(IGNORES) -I$(TAP)

# vi:ft=make:
