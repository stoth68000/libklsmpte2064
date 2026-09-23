AUTOMAKE_OPTIONS = foreign

AM_CPPFLAGS = -I$(top_srcdir)/src
AM_CFLAGS = -Wall -O2

TESTS = test_core
check_PROGRAMS = test_core

test_core_SOURCES = test_core.c
test_core_LDADD = ../src/libklsmpte2064.la -lm
