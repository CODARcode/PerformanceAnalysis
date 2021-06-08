AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/3rdparty -I$(top_srcdir)/sim/include @PS_FLAGS@

simlibdir = $(prefix)/sim/lib

simlib_LTLIBRARIES = libchimbuko_sim.la
libchimbuko_sim_la_SOURCES = provdb.cpp id_map.cpp ad_params.cpp pserver.cpp event_id_map.cpp ad.cpp thread_execution_api.cpp
libchimbuko_sim_la_LDFLAGS = -version-info 1:0:0

