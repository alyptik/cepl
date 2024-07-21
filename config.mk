#
# config.mk - Makefile configuration variables
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

# vi:ft=make:
