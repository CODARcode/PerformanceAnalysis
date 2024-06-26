AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/3rdparty @PS_FLAGS@
LDADD = $(top_builddir)/src/libchimbuko.la -lgtest -lstdc++fs 

testdir = $(prefix)/test/core/unit_tests/net
test_PROGRAMS = ZMQMENet ZMQNet LocalNet

ZMQMENet_SOURCES = ZMQMENet.cpp ../../../unit_tests/unit_test_main_mpi.cpp
ZMQMENet_LDADD = $(LDADD)

ZMQNet_SOURCES = ZMQNet.cpp ../../../unit_tests/unit_test_main_mpi.cpp
ZMQNet_LDADD = $(LDADD)

LocalNet_SOURCES = LocalNet.cpp ../../../unit_tests/unit_test_main_mpi.cpp
LocalNet_LDADD = $(LDADD)

