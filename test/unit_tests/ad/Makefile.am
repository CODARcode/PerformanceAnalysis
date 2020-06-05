AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/3rdparty @PS_FLAGS@
LDADD = $(top_builddir)/src/libchimbuko.la -lgtest -lstdc++fs 

testdir = $(prefix)/test/unit_tests/ad
test_PROGRAMS = ADEvent ADio ADParser ADOutlier ADNetClient ADLocalFuncStatistics ADMetadataParser ADCounter

ADEvent_SOURCES = ADEvent.cpp ../unit_test_main.cpp
ADEvent_LDADD = $(LDADD)

ADio_SOURCES = ADio.cpp ../unit_test_main.cpp
ADio_LDADD = $(LDADD)

ADParser_SOURCES = ADParser.cpp ../unit_test_main_mpi.cpp
ADParser_LDADD = $(LDADD)

ADOutlier_SOURCES = ADOutlier.cpp ../unit_test_main_mpi.cpp
ADOutlier_LDADD = $(LDADD)

ADNetClient_SOURCES = ADNetClient.cpp ../unit_test_main_mpi.cpp
ADNetClient_LDADD = $(LDADD)

ADLocalFuncStatistics_SOURCES = ADLocalFuncStatistics.cpp ../unit_test_main_mpi.cpp
ADLocalFuncStatistics_LDADD = $(LDADD)

ADMetadataParser_SOURCES = ADMetadataParser.cpp ../unit_test_main.cpp
ADMetadataParser_LDADD = $(LDADD)

ADCounter_SOURCES = ADCounter.cpp ../unit_test_main.cpp
ADCounter_LDADD = $(LDADD)
