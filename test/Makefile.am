SUBDIRS = unit_tests

AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/3rdparty @PS_FLAGS@
LDADD = ../src/libchimbuko.la -lgtest -lstdc++fs 

testdir = $(prefix)/test
test_PROGRAMS = mainSimple mainNet mainAd mainStatSender

mainSimple_SOURCES = mainSimple.cpp test_utils.cpp test_param.cpp
mainSimple_LDADD = $(LDADD)

mainNet_SOURCES = mainNet.cpp test_net.cpp
mainNet_LDADD = $(LDADD)

mainAd_SOURCES = mainAD.cpp test_ad.cpp
mainAd_LDADD = $(LDADD)

mainStatSender_SOURCES = test_stat_sender.cpp
mainStatSender_LDADD = $(LDADD)



test_DATA = run_all.sh run_ad.sh run_net.sh run_stat_sender.sh

if ENABLE_PROVDB
test_PROGRAMS += mainProvDBclient mainADwithProvDB provDBclientConnectDisconnect

mainProvDBclient_SOURCES = mainProvDBclient.cpp
mainProvDBclient_LDADD = $(LDADD)

mainADwithProvDB_SOURCES = mainADwithProvDB.cpp
mainADwithProvDB_LDADD = $(LDADD)

provDBclientConnectDisconnect_SOURCES = provDBclientConnectDisconnect.cpp
provDBclientConnectDisconnect_LDADD = $(LDADD)

test_DATA += run_provdb_client_test.sh run_ad_with_provdb.sh run_provdb_autoshutdown_test.sh
endif

install-data-hook:
	cp -rd $(srcdir)/data $(DESTDIR)$(testdir)/
	chmod u+x $(DESTDIR)$(testdir)/*.sh
