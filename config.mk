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
LDFLAGS ?= -pipe -fstack-protector-strong -Wl,-O2,-z,relro,-z,now,--sort-common,--as-needed

# mandatory
LD = $(CC)
MKALL = $(MKCFG) $(DEP)
OBJ = $(SRC:.c=.o)
TOBJ = $(TSRC:.c=.o)
DEP = $(SRC:.c=.d) $(TSRC:.c=.d)
TEST = $(filter-out $(TAP),$(TSRC:.c=))
UTEST = $(filter-out src/$(TARGET).o,$(SRC:.c=.o))
CPPFLAGS := -D_FORTIFY_SOURCE=2 -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -MMD -MP
DEBUG := -Og -g3 -no-pie -Wfloat-equal -Wrestrict -Wshadow -fsanitize=address,alignment,leak,undefined
LIBS := -lelf -lhistory -lreadline
TARGET := cepl
MANPAGE := cepl.7
TAP := t/tap
BINDIR := bin
MANDIR := share/man/man7
SRC := $(wildcard src/*.c)
TSRC := $(wildcard t/*.c)
HDR := $(wildcard src/*.h) $(wildcard t/*.h)
MKALL += Makefile debug.mk
CFLAGS += -fuse-ld=gold -std=c11 -pedantic-errors -Wall -Wextra
LDFLAGS += -fuse-ld=gold

# vi:ft=make:
