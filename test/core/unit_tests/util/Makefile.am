AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/3rdparty @PS_FLAGS@
LDADD = $(top_builddir)/src/libchimbuko.la -lgtest -lstdc++fs 

testdir = $(prefix)/test/core/unit_tests/util
test_PROGRAMS = DispatchQueue commandLineParser RunStats error memutils PerfStats environment Histogram pointerRegistry chunkAllocator

DispatchQueue_SOURCES = DispatchQueue.cpp ../../../unit_tests/unit_test_main_mpi.cpp
DispatchQueue_LDADD = $(LDADD)

commandLineParser_SOURCES = commandLineParser.cpp ../../../unit_tests/unit_test_main_mpi.cpp
commandLineParser_LDADD = $(LDADD)

RunStats_SOURCES = RunStats.cpp ../../../unit_tests/unit_test_main_mpi.cpp
RunStats_LDADD = $(LDADD)

error_SOURCES = error.cpp ../../../unit_tests/unit_test_main_mpi.cpp
error_LDADD = $(LDADD)

memutils_SOURCES = memutils.cpp ../../../unit_tests/unit_test_main_mpi.cpp
memutils_LDADD = $(LDADD)

PerfStats_SOURCES = PerfStats.cpp ../../../unit_tests/unit_test_main_mpi.cpp
PerfStats_LDADD = $(LDADD)

environment_SOURCES = environment.cpp ../../../unit_tests/unit_test_main_mpi.cpp
environment_LDADD = $(LDADD)

Histogram_SOURCES = Histogram.cpp ../../../unit_tests/unit_test_main_mpi.cpp
Histogram_LDADD = $(LDADD)

pointerRegistry_SOURCES = pointerRegistry.cpp ../../../unit_tests/unit_test_main_mpi.cpp
pointerRegistry_LDADD = $(LDADD)

chunkAllocator_SOURCES = chunkAllocator.cpp ../../../unit_tests/unit_test_main_mpi.cpp
chunkAllocator_LDADD = $(LDADD)
