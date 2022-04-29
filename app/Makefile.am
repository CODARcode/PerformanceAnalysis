AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/3rdparty @PS_FLAGS@
LDADD = ../src/libchimbuko.la -lstdc++fs

bin_PROGRAMS = driver driver_multirank pclient pclient_stats hpserver pserver pshutdown sstSinker sst_view bpfile_replay

driver_SOURCES = driver.cpp
driver_LDADD = $(LDADD)

driver_multirank_SOURCES = driver_multirank.cpp
driver_multirank_LDADD = $(LDADD)

pclient_SOURCES = pclient.cpp
pclient_LDADD = $(LDADD)

pclient_stats_SOURCES = pclient_stats.cpp
pclient_stats_LDADD = $(LDADD)

pserver_SOURCES = pserver.cpp
pserver_LDADD = $(LDADD)

hpserver_SOURCES = hpserver.cpp
hpserver_LDADD = $(LDADD)

pshutdown_SOURCES = pshutdown.cpp
pshutdown_LDADD = $(LDADD)

provdb_dump_state_SOURCES = provdb_dump_state.cpp
provdb_dump_state_LDADD = $(LDADD)

sstSinker_SOURCES = sstSinker.cpp
sstSinker_LDADD = $(LDADD)

sst_view_SOURCES = sst_view.cpp
sst_view_LDADD = $(LDADD)

bpfile_replay_SOURCES = bpfile_replay.cpp
bpfile_replay_LDADD = $(LDADD)


if ENABLE_PROVDB
bin_PROGRAMS += provdb_admin provdb_query provdb_shutdown provdb_rebuild_ascii provdb_dump_state provdb_commit

provdb_admin_SOURCES = provdb_admin.cpp
provdb_admin_LDADD = $(LDADD)

provdb_query_SOURCES = provdb_query.cpp
provdb_query_LDADD = $(LDADD)

provdb_shutdown_SOURCES = provdb_shutdown.cpp
provdb_shutdown_LDADD = $(LDADD)

provdb_rebuild_ascii_SOURCES = provdb_rebuild_ascii.cpp
provdb_rebuild_ascii_LDADD = $(LDADD)

provdb_commit_SOURCES = provdb_commit.cpp
provdb_commit_LDADD = $(LDADD)


else
	echo "Provenance DB not being built"
endif


bindata_DATA = ws_flask_stat.py sst_view_parse.pl sonata_profile_parse.pl
bindatadir = $(bindir)

install-data-hook:
	chmod u+x $(DESTDIR)$(bindatadir)/sst_view_parse.pl
	chmod u+x $(DESTDIR)$(bindatadir)/sonata_profile_parse.pl
