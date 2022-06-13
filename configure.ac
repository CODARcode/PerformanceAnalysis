AC_INIT(Chimbuko Performance Data Analysis, 3.0)
: ${CXXFLAGS=""}   #Disable default optimization
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])
AC_LANG(C++)
AC_PROG_CXX
AM_PROG_AR
LT_INIT

#Enable pkg-config
AC_ARG_WITH([pkg-config], AS_HELP_STRING([--with-pkg-config], [Use pkg-config to determine configuration of some dependencies]),[use_pkgconfig="yes"],[use_pkgconfig="no"])

#Check if pkg-config is installed
no_pkgconfig=1
if test "x$use_pkgconfig" != "xno"; then
    no_pkgconfig=$(echo $(pkg-config 2>&1) | grep -c "not found")
    if [[ "${no_pkgconfig}" -eq 0 ]]; then
	AC_MSG_NOTICE([pkg-config was detected])
    fi
fi

#spdlog propagates its compile flags through cmake targets and pkg-config but doesn't for autotools
#thus if pkg-config is available we try to get the flags there
if [[ "${no_pkgconfig}" -eq 0 ]]; then
   SPDLOG_CFLAGS=$(pkg-config --cflags spdlog 2>&1)
   if [[ $(echo ${SPDLOG_CFLAGS} | grep -c "not found") -eq 1 ]]; then
       AC_MSG_FAILURE(["Cannot find spdlog configuration with pkg-config"])
   fi
   CXXFLAGS+="${SPDLOG_CFLAGS}"
   SPDLOG_LDFLAGS=$(pkg-config --libs spdlog 2>&1)
   LDFLAGS+="${SPDLOG_LDFLAGS}" 
   AC_MSG_NOTICE([Retrieved configuration for spdlog from pkg-config])
fi


# Check if compiling to generate a coverage report using gcov
# Once built, run the tests in the install/test directory, then from the build directory run
#(pip3 install gcovr)
#gcovr -j 8 -v -r ${src_dir} --object-directory=. -e '.*3rdparty/' -e '.*app/'
#where src_dir is the source directory
AC_ARG_ENABLE([gcov],
  [AS_HELP_STRING([--enable-gcov], [Enable code coverage report using gcov (requires gcc)])],
  [:],
  [enable_gcov=no]
)

if test "x$enable_gcov" != "xno"; then
AC_MSG_NOTICE([Compiling with coverage report])
CXXFLAGS+=" -O0 --coverage -g"
LDFLAGS+=" --coverage"
else
AC_MSG_NOTICE([Compiling with optimization])
CXXFLAGS+=" -O3 -g"
LDFLAGS+=""
fi

# Check for c++17
CXXFLAGS+=" -std=c++17"
AC_MSG_CHECKING([for c++17 compatibility])
AC_COMPILE_IFELSE( [AC_LANG_PROGRAM([[]],
                                    [[]])],
                   [AC_MSG_RESULT([yes])],
                   [AC_MSG_FAILURE(["Could not compile with CXXFLAGS $CXXFLAGS"])]
                  )


AC_ARG_ENABLE([mpi],
    AS_HELP_STRING([--disable-mpi], [Disable MPI. User will need to manually assign the rank index to the AD instances.]), [], [enable_mpi=yes])

AS_IF(
[test "x$enable_mpi" != "xno"], [
    #Check we can compile with MPI
    AC_MSG_NOTICE([MPI use is enabled, checking compilation with MPI is possible])
    AC_CHECK_HEADER(mpi.h, [], [AC_MSG_FAILURE([["Cannot find MPI header"]])] )
    AC_DEFINE([USE_MPI],[1],[Use MPI])
    LIBS+="-lpthread"
],
[test "x$enable_mpi" == "xno"], [
    AC_MSG_NOTICE([MPI use is disabled])
    LIBS+="-lpthread"    		       
]
)


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
   AC_CHECK_PROG(HAVE_ADIOS2CONFIG, adios2-config, [yes],[no], "${with_adios2}/bin")
   if test "x$HAVE_ADIOS2CONFIG" = "xno"; then
      AC_MSG_FAILURE(["Cannot find adios2-config"])
   fi

   CPPFLAGS+=" -I$with_adios2/include `$with_adios2/bin/adios2-config --cxx-flags -s`"
   #Between 2.5 and 2.6 they split the library into components. Unfortunately the adios2-config output doesn't play nicely with autotools (the link order gets broken)
   #so we have to manually figure out which situation applies

   LIBS_BAK=${LIBS}

   #2.6.0
   LIBS+=" -L$with_adios2/lib -L$with_adios2/lib64 -ladios2_cxx11 -Wl,-rpath-link,${with_adios2}/lib:${with_adios2}/lib64 -Wl,-rpath,${with_adios2}/lib:${with_adios2}/lib64"
   AC_LINK_IFELSE(
 	[
		AC_LANG_PROGRAM(
			[[ #include<adios2.h>
                            ]], [[ adios2::ADIOS ad(adios2::DebugON);  ]]
			)
	], [adios2_links26=yes], [adios2_links26=no]
	)
   if test	"x${adios2_links26}" = "xno"; then
        #2.5.0
        LIBS=${LIBS_BAK}
        LIBS+=" -L$with_adios2/lib -L$with_adios2/lib64 -ladios2 -Wl,-rpath-link,${with_adios2}/lib:${with_adios2}/lib64 -Wl,-rpath,${with_adios2}/lib:${with_adios2}/lib64"
        AC_LINK_IFELSE(
	   [
		AC_LANG_PROGRAM(
			[[ #include<adios2.h>
                            ]], [[ adios2::ADIOS ad(adios2::DebugON);  ]]
			)
	   ], [adios2_links25=yes], [adios2_links25=no]
	   )

      	if test	"x${adios2_links25}" = "xno"; then
	       AC_MSG_FAILURE(["ADIOS2 is required"])
        else
               AC_MSG_NOTICE([Detected ADIOS <=2.5])
	fi
   else
	   AC_MSG_NOTICE([Detected ADIOS 2.6+])
   fi
else
	AC_MSG_FAILURE(["ADIOS2 install path must be specified with --with-adios2"])
fi

#Choose network
PS_FLAGS=
AC_ARG_WITH([network], AS_HELP_STRING([--with-network], [Specify network layer: MPI or ZMQ]),[],[with_network=no])
if test "x$with_network" = "xMPI"; then
   AC_DEFINE([_USE_MPINET],[1],[Use MPI network layer])
   AS_IF([test "x$enable_mpi" != "xyes"], [
      AC_MSG_FAILURE([Cannot use MPINet with MPI disabled!])
   ]) 
elif test "x$with_network" = "xZMQ"; then
   AC_DEFINE([_USE_ZMQNET],[1],[Use ZMQ network layer])
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
#   AC_CHECK_LIB([thallium], [_init], [],  [AC_MSG_WARN([Could not link to Thallium: Provenance DB will not be built]); enable_provenancedb=no]  )
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
AC_ARG_WITH([perf-metric], AS_HELP_STRING([--with-perf-metric], [Specify use of performance metrics]),[AC_DEFINE([_PERF_METRIC],[1],[Use performance metrics])],[])

#Enable ProvdDB margo state dump (currently requires master branch of margo)
AC_ARG_ENABLE([margo-state-dump],
  [AS_HELP_STRING([--enable-margo-state-dump], [Enable ProvdDB margo state dump (currently requires master branch of margo)] )],
  [AC_DEFINE([ENABLE_MARGO_STATE_DUMP],[1],[Use margo state dump])],
  [:]
)

#Check for boost
AC_ARG_WITH([boost], AS_HELP_STRING([--with-boost], [Specify Boost install directory]),[],[])
if test "x$with_boost" != "x"; then
   CPPFLAGS+=" -I$with_boost/include"
   LIBS+=" -L$with_boost/lib"
fi
AC_CHECK_HEADERS([boost/math/distributions/normal.hpp], [],    [AC_MSG_ERROR(Boost library is required)])


AC_CONFIG_HEADERS([chimbuko_config.h])
AC_SUBST([PS_FLAGS])

AC_CONFIG_SRCDIR([src/chimbuko.cpp])
AC_CONFIG_FILES([Makefile src/Makefile app/Makefile sphinx/Makefile include/Makefile 3rdparty/Makefile test/Makefile test/unit_tests/Makefile test/unit_tests/ad/Makefile test/unit_tests/util/Makefile test/unit_tests/pserver/Makefile test/unit_tests/net/Makefile test/unit_tests/param/Makefile scripts/Makefile scripts/launch/Makefile sim/Makefile sim/src/Makefile sim/main/Makefile sim/include/Makefile])

AC_CONFIG_FILES([app/ws_flask_stat.py:app/ws_flask_stat.py app/sst_view_parse.pl:app/sst_view_parse.pl])
AC_CONFIG_FILES([run_test.sh:run_test.sh test/run_all.sh:test/run_all.sh test/run_ad.sh:test/run_ad.sh test/run_net.sh:test/run_net.sh test/run_provdb_client_test.sh:test/run_provdb_client_test.sh test/run_provdb_autoshutdown_test.sh:test/run_provdb_autoshutdown_test.sh test/unit_tests/run_all.sh:test/unit_tests/run_all.sh test/run_ad_with_provdb.sh:test/run_ad_with_provdb.sh test/run_stat_sender.sh:test/run_stat_sender.sh scripts/launch/run_services.sh:scripts/launch/run_services.sh], [chmod u+x $(echo $ac_tag | sed s/.*\://)])

AC_OUTPUT
