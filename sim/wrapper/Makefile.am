AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/3rdparty -I$(top_srcdir)/sim/include @PS_FLAGS@

simwraplibdir = $(prefix)/sim/lib

simwraplib_LTLIBRARIES = libchimbuko_simwrap.la
libchimbuko_simwrap_la_SOURCES = wrapper.cpp
libchimbuko_simwrap_la_LDFLAGS = -version-info 1:0:0
