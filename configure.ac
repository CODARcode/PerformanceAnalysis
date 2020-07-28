AC_INIT(Chimbuko Performance Data Analysis, 3.0)
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])
AC_LANG(C++)
AC_PROG_CXX
AM_PROG_AR
LT_INIT

CXXFLAGS+=" -O3"

# Check for c++17
CXXFLAGS+=" -std=c++17"
AC_MSG_CHECKING([for c++17 compatibility])
AC_COMPILE_IFELSE( [AC_LANG_PROGRAM([[]],
                                    [[]])],
                   [AC_MSG_RESULT([yes])],
                   [AC_MSG_FAILURE(["Could not compile with CXXFLAGS $CXXFLAGS"])]
                  )


#Check we can compile with MPI
AC_CHECK_HEADER(mpi.h, [], [AC_MSG_FAILURE([["Cannot find MPI header"]])] )

#Check for curl config and library
AC_CHECK_PROG(HAVE_CURLCONFIG, curl-config, [yes],[no])
if test "x$HAVE_CURLCONFIG" = "xno"; then
   AC_MSG_FAILURE(["Cannot find curl-config"])
else
   LDFLAGS+=" `curl-config --libs`"
fi


#Check for adios2 library
AC_ARG_WITH([adios2], AS_HELP_STRING([--with-adios2], [Specify ADIOS2 install directory]),[],[])
if test "x$with_adios2" != "x"; then
  CPPFLAGS+=" -I$with_adios2/include"
  LDFLAGS+=" -L$with_adios2/lib `$with_adios2/bin/adios2-config --cxx-libs`"
fi

AC_CHECK_LIB([adios2], [adios2_close], [], [HAVE_LIBadios2=no], [])
if test "x$HAVE_LIBadios2" = "xno"; then
   AC_MSG_FAILURE(["ADIOS2 is required"])
fi

#Choose network
PS_FLAGS=
AC_ARG_WITH([network], AS_HELP_STRING([--with-network], [Specify network layer: MPI or ZMQ]),[],[with_network=no])
if test "x$with_network" = "xMPI"; then
   #AC_DEFINE([_USE_MPINET],[1],[Use MPI network layer])
   PS_FLAGS+=" -D_USE_MPINET"
elif test "x$with_network" = "xZMQ"; then
   #AC_DEFINE([_USE_ZMQNET],[1],[Use ZMQ network layer])
   PS_FLAGS+=" -D_USE_ZMQNET"
   AC_CHECK_LIB([zmq], [zmq_close], [],
           [AC_MSG_FAILURE([Could not link to 0MQ])])
else
   AC_MSG_FAILURE(["Must set a network layer using --with-network"])
fi

#Choose to build provenance database client/provider
#Require Mochi/Sonata to be installed and useable
AC_ARG_ENABLE([provenancedb],
  [AS_HELP_STRING([--enable-provenancedb], [Enable build of the provenance DB client/providers @<:@check@:>@])],
  [:],
  [enable_provenancedb=check])

#Build provenance DB unless explicitly disabled or cannot link to MOCHI libraries
if test "$enable_provenancedb" != "no"; then
   AC_CHECK_LIB([sonata-client], [_init], [],  [AC_MSG_WARN([Could not link to Sonata: Provenance DB will not be built]); enable_provenancedb=no]  )
   AC_CHECK_LIB([sonata-server], [_init], [],  [AC_MSG_WARN([Could not link to Sonata: Provenance DB will not be built]); enable_provenancedb=no]  )
   AC_CHECK_LIB([sonata-admin], [_init], [],  [AC_MSG_WARN([Could not link to Sonata: Provenance DB will not be built]); enable_provenancedb=no]  )
   AC_CHECK_LIB([thallium], [_init], [],  [AC_MSG_WARN([Could not link to Thallium: Provenance DB will not be built]); enable_provenancedb=no]  )
   AC_CHECK_LIB([mercury], [_init], [],  [AC_MSG_WARN([Could not link to Mercury: Provenance DB will not be built]); enable_provenancedb=no]  )
   AC_CHECK_LIB([margo], [_init], [],  [AC_MSG_WARN([Could not link to Margo: Provenance DB will not be built]); enable_provenancedb=no]  )
   AC_CHECK_LIB([abt], [_init], [],  [AC_MSG_WARN([Could not link to Argobots: Provenance DB will not be built]); enable_provenancedb=no]  )
fi

AM_CONDITIONAL(ENABLE_PROVDB, test "$enable_provenancedb" != "no")

if test "$enable_provenancedb" != "no"; then
   AC_MSG_NOTICE([Provenance DB will be built])
   AC_DEFINE([ENABLE_PROVDB], [1], [Enable the provenance database])
fi


#Enable performance metrics dump
#AC_ARG_WITH([perf-metric], AS_HELP_STRING([--with-perf-metric], [Specify use of performance metrics]),[AC_DEFINE([_PERF_METRIC],[1],[Use performance metrics])],[])
AC_ARG_WITH([perf-metric], AS_HELP_STRING([--with-perf-metric], [Specify use of performance metrics]),[PS_FLAGS+=" -D_PERF_METRIC"],[])



AC_CONFIG_HEADERS([config.h])
AC_SUBST([PS_FLAGS])

AC_CONFIG_SRCDIR([src/chimbuko.cpp])
AC_CONFIG_FILES([Makefile src/Makefile app/Makefile sphinx/Makefile test/Makefile test/unit_tests/Makefile test/unit_tests/ad/Makefile test/unit_tests/util/Makefile])

AC_CONFIG_FILES([app/ws_flask_stat.py:app/ws_flask_stat.py app/sst_view_parse.pl:app/sst_view_parse.pl])
AC_CONFIG_FILES([run_test.sh:run_test.sh test/run_all.sh:test/run_all.sh test/run_ad.sh:test/run_ad.sh test/run_net.sh:test/run_net.sh test/run_provdb_client_test.sh:test/run_provdb_client_test.sh test/run_provdb_autoshutdown_test.sh:test/run_provdb_autoshutdown_test.sh test/unit_tests/run_all.sh:test/unit_tests/run_all.sh test/run_ad_with_provdb.sh:test/run_ad_with_provdb.sh test/run_stat_sender.sh:test/run_stat_sender.sh], [chmod u+x $(echo $ac_tag | sed s/.*\://)])

AC_OUTPUT
