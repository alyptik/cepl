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
CFLAGS ?=
LDFLAGS ?=

# mandatory
LD = $(CC)
DEBUG_CFLAGS = $(DEBUG)
DEBUG_LDFLAGS = $(DEBUG)
MKALL = $(MKCFG) $(DEP)
OBJ = $(SRC:.c=.o)
TOBJ = $(TSRC:.c=.o)
DEP = $(SRC:.c=.d) $(TSRC:.c=.d)
TEST = $(filter-out $(TAP),$(TSRC:.c=))
UTEST = $(filter-out src/$(TARGET).o,$(SRC:.c=.o))
SRC := $(wildcard src/*.c)
TSRC := $(wildcard t/*.c)
HDR := $(wildcard src/*.h) $(wildcard t/*.h)
ASAN := -fsanitize=address,alignment,leak,undefined
DEBUG := -D_DEBUG -Og -ggdb3 -no-pie -fno-inline -Wfloat-equal -Wshadow
CPPFLAGS := -D_FORTIFY_SOURCE=2 -D_GNU_SOURCE -MMD -MP
LIBS := -lelf -lhistory -lreadline
TARGET := cepl
MANPAGE := cepl.1
COMPLETION := _cepl
TAP := t/tap
BINDIR := bin
MANDIR := share/man/man1
COMPDIR := share/zsh/site-functions
MKALL += Makefile asan.mk
DEBUG += -fno-builtin -fno-common -fverbose-asm
CFLAGS += -pedantic-errors -std=c11 -Wall -Wextra -Wstrict-overflow
CFLAGS += -Wno-gnu-zero-variadic-macro-arguments -Wno-unused-variable
CFLAGS += -Wno-implicit-fallthrough -Wno-missing-field-initializers
CFLAGS += -fuse-ld=gold -flto -fuse-linker-plugin -fno-strict-aliasing
LDFLAGS += -Wl,-O2,-z,relro,-z,now,--sort-common,--as-needed
LDFLAGS += -fuse-ld=gold -flto -fuse-linker-plugin -fno-strict-aliasing

# vi:ft=make:
