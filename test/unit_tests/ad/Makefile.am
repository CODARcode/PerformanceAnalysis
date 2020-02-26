AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/3rdparty @PS_FLAGS@
LDADD = $(top_builddir)/src/libchimbuko.la -lgtest -lstdc++fs 

testdir = $(prefix)/test/unit_tests/ad
test_PROGRAMS = ADEvent ADio ADParser

ADEvent_SOURCES = ADEvent.cpp ../unit_test_main.cpp
ADEvent_LDADD = $(LDADD)

ADio_SOURCES = ADio.cpp ../unit_test_main.cpp
ADio_LDADD = $(LDADD)

ADParser_SOURCES = ADParser.cpp ../unit_test_main_mpi.cpp
ADParser_LDADD = $(LDADD)
