AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/3rdparty -I$(top_srcdir)/sim/include @PS_FLAGS@
LDADD = ../src/libchimbuko_sim.la ../../src/libchimbuko.la -lstdc++fs 

serverJdir = $(prefix)/sim/serverJ
serverJ_PROGRAMS = serverJ

serverJ_SOURCES = server.cpp
serverJ_LDADD = $(LDADD)
