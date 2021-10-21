AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/3rdparty -I$(top_srcdir)/sim/include @PS_FLAGS@
LDADD = ../src/libchimbuko_sim.la ../../src/libchimbuko.la -lstdc++fs 

simmaindir = $(prefix)/sim/server_j
simmain_PROGRAMS = server

server_SOURCES = server.cpp
server_LDADD = $(LDADD)
