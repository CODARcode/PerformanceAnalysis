AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/3rdparty @PS_FLAGS@
LDADD = $(top_builddir)/src/libchimbuko.la -lgtest -lstdc++fs 

testdir = $(prefix)/test/unit_tests/pserver
test_PROGRAMS = PSglobalFunctionIndexMap global_anomaly_stats

PSglobalFunctionIndexMap_SOURCES = PSglobalFunctionIndexMap.cpp ../unit_test_main.cpp
PSglobalFunctionIndexMap_LDADD = $(LDADD)

global_anomaly_stats_SOURCES = global_anomaly_stats.cpp ../unit_test_main.cpp
global_anomaly_stats_LDADD = $(LDADD)
