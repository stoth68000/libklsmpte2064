
AUTOMAKE_OPTIONS = foreign

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libklsmpte2064.pc
libklsmpte2064_la_LDFLAGS  = -Wl,--no-as-needed -lm

lib_LTLIBRARIES = libklsmpte2064.la

libklsmpte2064_la_SOURCES  = core.c
libklsmpte2064_la_SOURCES += core-audio.c
libklsmpte2064_la_SOURCES += core-video.c
libklsmpte2064_la_SOURCES += core-encapsulation.c
libklsmpte2064_la_SOURCES += core-csc.c

libklsmpte2064_la_CFLAGS = -Wall -DVERSION=\"$(VERSION)\" -DPROG="\"$(PACKAGE)\"" \
	-D_FILE_OFFSET_BITS=64 -O3 -D_BSD_SOURCE -I$(top_srcdir)/include

#if DEBUG
  libklsmpte2064_la_CFLAGS += -g
#endif

libklsmpte2064_includedir = $(includedir)/libklsmpte2064

libklsmpte2064_include_HEADERS  = libklsmpte2064/klsmpte2064.h
libklsmpte2064_include_HEADERS += libklsmpte2064/core-csc.h
libklsmpte2064_include_HEADERS += libklsmpte2064/core-encapsulation.h
libklsmpte2064_include_HEADERS += libklsmpte2064/core-video.h
libklsmpte2064_include_HEADERS += libklsmpte2064/core-audio.h
libklsmpte2064_include_HEADERS += libklsmpte2064/core.h

