#Generate a Makefile for the src directory (run from inside the directory)

out=Makefile.am
echo 'AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/3rdparty @PS_FLAGS@' > ${out}
echo 'lib_LTLIBRARIES = libchimbuko.la' >> ${out}
echo -n 'libchimbuko_la_SOURCES = ' >> ${out}

for i in $(find . -name '*.cpp' | sed 's/^\.\///'); do
    echo -n "$i " >> ${out}
done
echo '' >> ${out}

echo 'libchimbuko_la_LDFLAGS = -version-info 3:0:0' >> ${out}
