# Compiler settings
CC      ?= cc
CFLAGS  += -Wall -std=gnu99 \
		   -D_GNU_SOURCE -Wno-unused-result \
		   -g
LDFLAGS ?=
LEX     ?= flex
LFLAGS  ?=
YACC    ?= bison
YFLAGS  ?=
LD      ?= ld
PERL    ?= perl -w
MKDIR   ?= mkdir

# Directory to put the compiled executable - Note: Not install path
BIN     ?= ./bin

# Install Paths
PREFIX  := /usr
BINDIR  := $(PREFIX)/bin
MANDIR  := $(PREFIX)/share/man
MAN1DIR := $(MANDIR)/man1
DOCDIR  := $(PREFIX)/share/doc/$(EXE)

# Show all commands executed by the Makefile
# V := y

CONFIG_DEBUG ?= n

