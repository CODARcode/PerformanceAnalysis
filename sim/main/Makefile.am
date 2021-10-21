AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/3rdparty -I$(top_srcdir)/sim/include @PS_FLAGS@
LDADD = ../src/libchimbuko_sim.la ../../src/libchimbuko.la -lstdc++fs 

simmaindir = $(prefix)/sim/main
simmain_PROGRAMS = server

#example1_SOURCES = example1.cpp
#example1_LDADD = $(LDADD)

#example2_SOURCES = example2.cpp
#example2_LDADD = $(LDADD)

server_SOURCES = server.cpp
server_LDADD = $(LDADD)

#simonedir = $(prefix)/sim/main/server_a
#simone_PROGRAMS = server_a

#server_a_SOURCES = server.cpp
#server_a_LDADD = $(LDADD)

#test_SOURCES = server_sock.cpp
#test_LDADD = $(LDADD)
