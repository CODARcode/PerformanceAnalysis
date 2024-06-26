AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/3rdparty @PS_FLAGS@
LDADD = $(top_builddir)/src/libchimbuko.la -lgtest -lstdc++fs

testdir = $(prefix)/test/core/unit_tests/ad
test_PROGRAMS = ADio utils

ADio_SOURCES = ADio.cpp ../../../unit_tests/unit_test_main_mpi.cpp
ADio_LDADD = $(LDADD)

utils_SOURCES = utils.cpp ../../../unit_tests/unit_test_main_mpi.cpp
utils_LDADD = $(LDADD)
