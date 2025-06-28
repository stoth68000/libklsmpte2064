
AUTOMAKE_OPTIONS = foreign
GIT_VERSION := $(shell git describe --abbrev=8 --dirty --always --tags)

AM_CPPFLAGS = -DVERSION=\"$(VERSION)\" -DPROG="\"$(PACKAGE)\"" -D_FILE_OFFSET_BITS=64 \
	-D_BSD_SOURCE \
	-DGIT_VERSION=\"$(GIT_VERSION)\" \
	-I$(top_srcdir)/src \
	-D__STDC_FORMAT_MACROS

AM_CFLAGS = -Wall -O3 -g

LDADD = ../src/libklsmpte2064.la -lpthread -ldl

if DEBUG
	CFLAGS += -g
endif

SRC  = util.c

bin_PROGRAMS  = klsmpte2064_util

klsmpte2064_util_SOURCES = $(SRC)

libklsmpte2064_noinst_includedir = $(includedir)
