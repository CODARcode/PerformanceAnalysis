AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/3rdparty @PS_FLAGS@
LDADD = ../src/libchimbuko.la -lstdc++fs 


bin_PROGRAMS = driver pclient pclient_stats pserver pshutdown sstSinker

driver_SOURCES = driver.cpp
driver_LDADD = $(LDADD)

pclient_SOURCES = pclient.cpp
pclient_LDADD = $(LDADD)

pclient_stats_SOURCES = pclient_stats.cpp
pclient_stats_LDADD = $(LDADD)

pserver_SOURCES = pserver.cpp
pserver_LDADD = $(LDADD)

pshutdown_SOURCES = pshutdown.cpp
pshutdown_LDADD = $(LDADD)

sstSinker_SOURCES = sstSinker.cpp
sstSinker_LDADD = $(LDADD)


