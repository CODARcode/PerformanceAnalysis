AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/3rdparty @PS_FLAGS@
LDADD = $(top_builddir)/src/libchimbuko.la -lgtest -lstdc++fs 

testdir = $(prefix)/test/unit_tests/util
test_PROGRAMS = DispatchQueue commandLineParser RunStats error memory

DispatchQueue_SOURCES = DispatchQueue.cpp ../unit_test_main.cpp
DispatchQueue_LDADD = $(LDADD)

commandLineParser_SOURCES = commandLineParser.cpp ../unit_test_main.cpp
commandLineParser_LDADD = $(LDADD)

RunStats_SOURCES = RunStats.cpp ../unit_test_main.cpp
RunStats_LDADD = $(LDADD)

error_SOURCES = error.cpp ../unit_test_main.cpp
error_LDADD = $(LDADD)

memory_SOURCES = memory.cpp ../unit_test_main.cpp
memory_LDADD = $(LDADD)

