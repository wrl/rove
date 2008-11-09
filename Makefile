SHELL = /bin/sh

export PROJECT = rove
export VERSION = 0.1a

export CC = gcc
export LD = gcc
export CFLAGS  += -ggdb -D_GNU_SOURCE -Wall -Werror
export LDFLAGS += -ggdb
export INSTALL = install

export PREFIX = /usr
export BINDIR = $(PREFIX)/bin
export LIBDIR = $(PREFIX)/lib
export INCDIR = $(PREFIX)/include
export PKGCONFIGDIR = $(LIBDIR)/pkgconfig

.SILENT:
.SUFFIXES:
.SUFFIXES: .c .o
.PHONY: all clean install

all:
	cd src; $(MAKE)

clean:
	cd src; $(MAKE) clean

install:
	cd src; $(MAKE) install