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
CFLAGS ?= -pipe -fstack-protector-strong
LDFLAGS ?= -pipe -fstack-protector-strong

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
DEBUG := -D_DEBUG -p -pg -Og -ggdb3 -no-pie -fno-inline -Wfloat-equal -Wrestrict -Wshadow
CPPFLAGS := -D_FORTIFY_SOURCE=2 -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -MMD -MP
LIBS := -lelf -lhistory -lreadline
TARGET := cepl
MANPAGE := cepl.1
TAP := t/tap
BINDIR := bin
MANDIR := share/man/man1
MKALL += Makefile debug.mk
DEBUG += -fno-builtin -fno-common -fprofile-generate=./p -fsanitize=address,alignment,leak,undefined -fverbose-asm
CFLAGS += -fPIC -fuse-ld=gold -flto -fuse-linker-plugin
CFLAGS += -pedantic-errors -std=c11 -Wall -Wextra -Wno-missing-field-initializers -Wstrict-overflow
CFLAGS += -fno-align-functions -fno-align-jumps -fno-align-labels -fno-align-loops -fno-strict-aliasing
LDFLAGS += -fPIC -fuse-ld=gold -flto -fuse-linker-plugin
LDFLAGS += -Wl,-O2,-z,relro,-z,now,--sort-common,--as-needed
LDFLAGS += -fno-align-functions -fno-align-jumps -fno-align-labels -fno-align-loops -fno-strict-aliasing

# vi:ft=make:
