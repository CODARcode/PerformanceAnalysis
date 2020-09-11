SUBDIRS = ad util pserver net

AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/3rdparty @PS_FLAGS@
LDADD = $(top_builddir)/src/libchimbuko.la -lgtest -lstdc++fs 

unittestdir=$(prefix)/test/unit_tests
unittest_DATA = run_all.sh

install-data-hook:
	chmod u+x $(DESTDIR)$(unittestdir)/*.sh
