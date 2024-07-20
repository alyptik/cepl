#
# config.mk - Makefile configuration variables
#
# AUTHOR: Joey Pabalinas <joeypabalinas@gmail.com>
# See LICENSE.md file for copyright and license details.

# optional
DESTDIR ?=
PREFIX ?= /usr/local
CC ?= gcc

# mandatory
LD = $(CC)
DEBUG_CFLAGS = $(CFLAGS) $(DEBUG)
DEBUG_LDFLAGS = $(LDFLAGS) $(DEBUG)
MKALL = $(MKCFG) $(DEP)
OBJ = $(SRC:.c=.o)
TOBJ = $(TSRC:.c=.o)
COBJ = $(CSRC:.c=.o)
DEP = $(SRC:.c=.d) $(TSRC:.c=.d) $(wildcard contrib/libtap/*.d)
SU = $(SRC:.c=.su) $(TSRC:.c=.su) $(wildcard contrib/libtap/*.su)
TEST = $(TSRC:.c=)
UTEST = $(filter-out src/$(TARGET).o,$(SRC:.c=.o))
SRC := $(wildcard src/*.c)
TSRC := $(wildcard t/*.c)
CSRC := $(wildcard contrib/libtap/*.c)
HDR := $(wildcard src/*.h) $(wildcard t/*.h) $(wildcard contrib/libtap/*.h)
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
LDLIBS += -lreadline -lhistory -lelf
DEBUG += -g3 -D_DEBUG
DEBUG += -fno-builtin -fno-inline
DEBUG += -Icontrib/libtab
CFLAGS += $(WARNINGS) $(IGNORES) -Icontrib/libtap

# vi:ft=make:
