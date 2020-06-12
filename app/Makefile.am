AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/3rdparty @PS_FLAGS@
LDADD = ../src/libchimbuko.la -lstdc++fs 

bin_PROGRAMS = driver pclient pclient_stats pserver pshutdown sstSinker sst_view bpfile_replay

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

sst_view_SOURCES = sst_view.cpp
sst_view_LDADD = $(LDADD)

bpfile_replay_SOURCES = bpfile_replay.cpp
bpfile_replay_LDADD = $(LDADD)


if ENABLE_PROVDB
bin_PROGRAMS += provdb_admin provdb_query

provdb_admin_SOURCES = provdb_admin.cpp
provdb_admin_LDADD = $(LDADD)

provdb_query_SOURCES = provdb_query.cpp
provdb_query_LDADD = $(LDADD)

else
	echo "Provenance DB not being built"
endif


bindata_DATA = ws_flask_stat.py sst_view_parse.pl
bindatadir = $(bindir)

install-data-hook:
	chmod u+x $(DESTDIR)$(bindatadir)/sst_view_parse.pl
