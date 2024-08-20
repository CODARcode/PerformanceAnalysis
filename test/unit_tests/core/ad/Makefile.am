AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/3rdparty @PS_FLAGS@
LDADD = $(top_builddir)/src/libchimbuko.la -lgtest -lstdc++fs

testdir = $(prefix)/test/unit_tests/core/ad
test_PROGRAMS = ADio ADOutlier utils

ADio_SOURCES = ADio.cpp ../../unit_test_main_mpi.cpp
ADio_LDADD = $(LDADD)

ADOutlier_SOURCES = ADOutlier.cpp ../../unit_test_main_mpi.cpp
ADOutlier_LDADD = $(LDADD)

utils_SOURCES = utils.cpp ../../unit_test_main_mpi.cpp
utils_LDADD = $(LDADD)
