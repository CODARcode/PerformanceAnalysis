AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/3rdparty @PS_FLAGS@
LDADD = $(top_builddir)/src/libchimbuko.la -lgtest -lstdc++fs 

testdir = $(prefix)/test/unit_tests/pserver
test_PROGRAMS = PSglobalFunctionIndexMap AggregateFuncStats GlobalAnomalyStats

PSglobalFunctionIndexMap_SOURCES = PSglobalFunctionIndexMap.cpp ../unit_test_main.cpp
PSglobalFunctionIndexMap_LDADD = $(LDADD)

GlobalAnomalyStats_SOURCES = GlobalAnomalyStats.cpp ../unit_test_main.cpp
GlobalAnomalyStats_LDADD = $(LDADD)

AggregateFuncStats_SOURCES = AggregateFuncStats.cpp ../unit_test_main.cpp
AggregateFuncStats_LDADD = $(LDADD)
