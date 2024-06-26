AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/3rdparty @PS_FLAGS@
LDADD = $(top_builddir)/src/libchimbuko.la -lgtest -lstdc++fs

testdir = $(prefix)/test/unit_tests/ad
test_PROGRAMS = COPODOutlier COPODOutlierADs HBOSOutlier HBOSOutlierADs HBOSOutlierDistributions ADEvent ADParser ADOutlier ADNetClient ADLocalFuncStatistics ADMetadataParser ADCounter ADLocalCounterStatistics ADAnomalyProvenance ADglobalFunctionIndexMap ADNormalEventProvenance AnomalyData ADcombinedPSdata FuncAnomalyMetrics ADLocalAnomalyMetrics ADMonitoring ADExecDataInterface

HBOSOutlier_SOURCES = HBOSOutlier.cpp ../unit_test_main_mpi.cpp
HBOSOutlier_LDADD = $(LDADD)

HBOSOutlierADs_SOURCES = HBOSOutlierADs.cpp ../unit_test_main_mpi.cpp
HBOSOutlierADs_LDADD = $(LDADD)

COPODOutlier_SOURCES = COPODOutlier.cpp ../unit_test_main_mpi.cpp
COPODOutlier_LDADD = $(LDADD)

COPODOutlierADs_SOURCES = COPODOutlierADs.cpp ../unit_test_main_mpi.cpp
COPODOutlierADs_LDADD = $(LDADD)

HBOSOutlierDistributions_SOURCES = HBOSOutlierDistributions.cpp ../unit_test_main_mpi.cpp
HBOSOutlierDistributions_LDADD = $(LDADD)

ADEvent_SOURCES = ADEvent.cpp ../unit_test_main_mpi.cpp
ADEvent_LDADD = $(LDADD)

ADParser_SOURCES = ADParser.cpp ../unit_test_main_mpi.cpp
ADParser_LDADD = $(LDADD)

ADOutlier_SOURCES = ADOutlier.cpp ../unit_test_main_mpi.cpp
ADOutlier_LDADD = $(LDADD)

ADNetClient_SOURCES = ADNetClient.cpp ../unit_test_main_mpi.cpp
ADNetClient_LDADD = $(LDADD)

ADLocalFuncStatistics_SOURCES = ADLocalFuncStatistics.cpp ../unit_test_main_mpi.cpp
ADLocalFuncStatistics_LDADD = $(LDADD)

ADMetadataParser_SOURCES = ADMetadataParser.cpp ../unit_test_main_mpi.cpp
ADMetadataParser_LDADD = $(LDADD)

ADCounter_SOURCES = ADCounter.cpp ../unit_test_main_mpi.cpp
ADCounter_LDADD = $(LDADD)

ADLocalCounterStatistics_SOURCES = ADLocalCounterStatistics.cpp ../unit_test_main_mpi.cpp
ADLocalCounterStatistics_LDADD = $(LDADD)

ADAnomalyProvenance_SOURCES = ADAnomalyProvenance.cpp ../unit_test_main_mpi.cpp
ADAnomalyProvenance_LDADD = $(LDADD)

ADglobalFunctionIndexMap_SOURCES = ADglobalFunctionIndexMap.cpp ../unit_test_main_mpi.cpp
ADglobalFunctionIndexMap_LDADD = $(LDADD)

ADNormalEventProvenance_SOURCES = ADNormalEventProvenance.cpp ../unit_test_main_mpi.cpp
ADNormalEventProvenance_LDADD = $(LDADD)

AnomalyData_SOURCES = AnomalyData.cpp ../unit_test_main_mpi.cpp
AnomalyData_LDADD = $(LDADD)

ADcombinedPSdata_SOURCES = ADcombinedPSdata.cpp ../unit_test_main_mpi.cpp
ADcombinedPSdata_LDADD = $(LDADD)

FuncAnomalyMetrics_SOURCES = FuncAnomalyMetrics.cpp ../unit_test_main_mpi.cpp
FuncAnomalyMetrics_LDADD = $(LDADD)

ADLocalAnomalyMetrics_SOURCES = ADLocalAnomalyMetrics.cpp ../unit_test_main_mpi.cpp
ADLocalAnomalyMetrics_LDADD = $(LDADD)

ADMonitoring_SOURCES = ADMonitoring.cpp ../unit_test_main_mpi.cpp
ADMonitoring_LDADD = $(LDADD)

ADExecDataInterface_SOURCES = ADExecDataInterface.cpp ../unit_test_main_mpi.cpp
ADExecDataInterface_LDADD = $(LDADD)
