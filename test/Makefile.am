AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/3rdparty @PS_FLAGS@
LDADD = ../src/libchimbuko.la -lgtest -lstdc++fs 

testdir = $(prefix)/test
test_PROGRAMS = mainSimple mainNet mainAd

mainSimple_SOURCES = mainSimple.cpp test_utils.cpp test_param.cpp
mainSimple_LDADD = $(LDADD)

mainNet_SOURCES = mainNet.cpp test_net.cpp
mainNet_LDADD = $(LDADD)

mainAd_SOURCES = mainAD.cpp test_ad.cpp
mainAd_LDADD = $(LDADD)
