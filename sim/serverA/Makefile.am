AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/3rdparty -I$(top_srcdir)/sim/include @PS_FLAGS@
LDADD = ../src/libchimbuko_sim.la ../../src/libchimbuko.la -lstdc++fs 

serverAdir = $(prefix)/sim/serverA
serverA_PROGRAMS = serverA

serverA_SOURCES = server.cpp
serverA_LDADD = $(LDADD)
