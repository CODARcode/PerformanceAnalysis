AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/3rdparty @PS_FLAGS@
LDADD = $(top_builddir)/src/libchimbuko.la -lgtest -lstdc++fs 

testdir = $(prefix)/test/unit_tests/net
test_PROGRAMS = ZMQMENet

ZMQMENet_SOURCES = ZMQMENet.cpp ../unit_test_main_mpi.cpp
ZMQMENet_LDADD = $(LDADD)
