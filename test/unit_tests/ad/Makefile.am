AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/3rdparty @PS_FLAGS@
LDADD = $(top_builddir)/src/libchimbuko.la -lgtest -lstdc++fs 

testdir = $(prefix)/test/unit_tests/ad
test_PROGRAMS = ADEvent ADio

ADEvent_SOURCES = ADEvent.cpp ../unit_test_main.cpp
ADEvent_LDADD = $(LDADD)

ADio_SOURCES = ADio.cpp ../unit_test_main.cpp
ADio_LDADD = $(LDADD)

#mainSimple_SOURCES = mainSimple.cpp test_utils.cpp test_param.cpp
#mainSimple_LDADD = $(LDADD)

#mainNet_SOURCES = mainNet.cpp test_net.cpp
#mainNet_LDADD = $(LDADD)

#mainAd_SOURCES = mainAD.cpp test_ad.cpp
#mainAd_LDADD = $(LDADD)
