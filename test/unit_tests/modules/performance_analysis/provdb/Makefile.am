AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/3rdparty @PS_FLAGS@
LDADD = $(top_builddir)/src/libchimbuko.la -lgtest -lstdc++fs

testdir = $(prefix)/test/unit_tests/modules/performance_analysis/provdb
test_PROGRAMS = ProvDBpruneOutlierInterface

ProvDBpruneOutlierInterface_SOURCES = ProvDBpruneOutlierInterface.cpp ../../../unit_test_main_mpi.cpp ProvDBtester.cpp
ProvDBpruneOutlierInterface_LDADD = $(LDADD)
