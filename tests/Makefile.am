AUTOMAKE_OPTIONS = foreign

AM_CPPFLAGS = -I$(top_srcdir)/src
AM_CFLAGS = -Wall -O2

TESTS = test_core test_util.sh
check_PROGRAMS = test_core
noinst_PROGRAMS = bench_core
EXTRA_DIST = test_util.sh

test_core_SOURCES = test_core.c
test_core_LDADD = ../src/libklsmpte2064.la -lm

bench_core_SOURCES = bench_core.c
bench_core_LDADD = ../src/libklsmpte2064.la -lm

bench: bench_core
	./bench_core

check-local:
	@total=0; passed=0; failed=0; \
	for log in *.log; do \
		if test -f "$$log"; then \
			if test "$$log" = "test-suite.log"; then \
				continue; \
			fi; \
			echo ""; \
			echo "---- $$log ----"; \
			cat "$$log"; \
			set -- `sed -n 's/^Summary: total=\([0-9][0-9]*\) passed=\([0-9][0-9]*\) failed=\([0-9][0-9]*\)$$/\1 \2 \3/p' "$$log"`; \
			if test "$$#" -eq 3; then \
				total=`expr $$total + $$1`; \
				passed=`expr $$passed + $$2`; \
				failed=`expr $$failed + $$3`; \
			fi; \
		fi; \
	done; \
	echo ""; \
	echo "Detailed check summary: total=$$total passed=$$passed failed=$$failed"
