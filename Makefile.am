AUTOMAKE_OPTIONS = foreign
SUBDIRS = src tools tests
EXTRA_DIST = doxygen/libklsmpte2064.doxyconf doxygen/include

docs:
	cd doxygen && doxygen libklsmpte2064.doxyconf

test:
	cd tools && make test
