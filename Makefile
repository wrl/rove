SHELL = /bin/sh

export PROJECT = rove

ifeq ($(wildcard config.mk),config.mk)
	include config.mk
endif

export LD = $(CC)
export CFLAGS  += -ggdb -D_GNU_SOURCE -Wall -Werror
export LDFLAGS += -ggdb
export INSTALL = install

export BINDIR = $(PREFIX)/bin
export LIBDIR = $(PREFIX)/lib
export INCDIR = $(PREFIX)/include
export PKGCONFIGDIR = $(LIBDIR)/pkgconfig

.SILENT:
.SUFFIXES:
.SUFFIXES: .c .o
.PHONY: all clean install

all:
ifdef VERSION
ifneq ($(wildcard rove),rove)
	echo    "";
	echo -e "  \033[1mbuilding \033[34mrove\033[0;1m $(VERSION):\033[0m";
endif
	cd src; $(MAKE)
	cp src/rove .

	echo    "";
	echo    "  build finished!";
	echo -e "  run \033[1;34mmake install\033[0m as root to install rove systemwide,";
	echo -e "  or, run it from here by typing \033[1;34m./rove\033[0m.";
	echo    "";
else
	echo "";
	echo "  hey, you're jumping the gun here!"
	echo "  you'll need to run ./configure first before you can build rove."
	echo "";
endif

clean:
	echo "";
	cd src; $(MAKE) clean
	rm -f rove
	echo "";

install:
	echo "";
	cd src; $(MAKE) install
	echo "";

distclean: clean
	rm -f config.mk