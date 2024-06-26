AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/3rdparty @PS_FLAGS@
LDADD = $(top_builddir)/src/libchimbuko.la -lgtest -lstdc++fs 

testdir = $(prefix)/test/unit_tests/param
test_PROGRAMS = sstd_param hbos_param copod_param

sstd_param_SOURCES = sstd_param.cpp ../unit_test_main_mpi.cpp
sstd_param_LDADD = $(LDADD)

hbos_param_SOURCES = hbos_param.cpp ../unit_test_main_mpi.cpp
hbos_param_LDADD = $(LDADD)

copod_param_SOURCES = copod_param.cpp ../unit_test_main_mpi.cpp
copod_param_LDADD = $(LDADD)
