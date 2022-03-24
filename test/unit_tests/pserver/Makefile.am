AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/3rdparty @PS_FLAGS@
LDADD = $(top_builddir)/src/libchimbuko.la -lgtest -lstdc++fs 

testdir = $(prefix)/test/unit_tests/pserver
test_PROGRAMS = PSglobalFunctionIndexMap AggregateFuncStats AggregateAnomalyData GlobalAnomalyStats AggregateFuncAnomalyMetrics GlobalCounterStats GlobalAnomalyMetrics PSparamManager

PSglobalFunctionIndexMap_SOURCES = PSglobalFunctionIndexMap.cpp ../unit_test_main.cpp
PSglobalFunctionIndexMap_LDADD = $(LDADD)

GlobalAnomalyStats_SOURCES = GlobalAnomalyStats.cpp ../unit_test_main.cpp
GlobalAnomalyStats_LDADD = $(LDADD)

AggregateFuncStats_SOURCES = AggregateFuncStats.cpp ../unit_test_main.cpp
AggregateFuncStats_LDADD = $(LDADD)

AggregateAnomalyData_SOURCES = AggregateAnomalyData.cpp ../unit_test_main.cpp
AggregateAnomalyData_LDADD = $(LDADD)

AggregateFuncAnomalyMetrics_SOURCES = AggregateFuncAnomalyMetrics.cpp ../unit_test_main.cpp
AggregateFuncAnomalyMetrics_LDADD = $(LDADD)

GlobalCounterStats_SOURCES = GlobalCounterStats.cpp ../unit_test_main.cpp
GlobalCounterStats_LDADD = $(LDADD)

GlobalAnomalyMetrics_SOURCES = GlobalAnomalyMetrics.cpp ../unit_test_main.cpp
GlobalAnomalyMetrics_LDADD = $(LDADD)

PSparamManager_SOURCES = PSparamManager.cpp ../unit_test_main.cpp
PSparamManager_LDADD = $(LDADD)
