#!/bin/bash

module load gcc/8.1.1
module load cmake/3.13.4
module load zlib/1.2.11
module load bzip2/1.0.6
module load zeromq/4.2.5
module load python/3.7.0

CODAR_ROOT=`pwd`
install_pdt=false
install_papi=false
install_tau2=true

if $install_pdt
then
   echo "Install pdt ..."
   rm -rf ./downloads/pdtoolkit-3.25
   rm -rf ./sw/pdt-3.25
   cd downloads
   tar -xvf pdt.tar
   cd pdtoolkit-3.25
   ./configure -prefix=$CODAR_ROOT/sw/pdt-3.25
   make
   make install
   cd $CODAR_ROOT
   echo "Installed pdt!"
fi

if $install_papi
then
   echo "Install papi ..."
   rm -rf ./downloads/papi-5.6.0
   rm -rf ./sw/papi-5.6.0
   cd downloads
   wget http://icl.utk.edu/projects/papi/downloads/papi-5.6.0.tar.gz
   tar -xzf papi-5.6.0.tar.gz
   rm papi-5.6.0.tar.gz
   cd papi-5.6.0/src
   ./configure --prefix=$CODAR_ROOT/sw/papi-5.6.0
   make
   make install
   cd $CODAR_ROOT
   echo "Installed papi!"
fi

# TAU2 installation will end up with an error saying that it couldn't find adios2.
# The reason is that tau assumes that the library is located under lib folder, but
# it is actually located under lib64. Need to manually fix this in include/Makefile.
if $install_tau2
then
   echo "Install tau2 ..."
   rm -rf ./downloads/tau2
   rm -rf ./sw/tau2
   cd downloads
   git clone https://github.com/UO-OACISS/tau2.git
   cd tau2
   ./configure -cc=gcc -c++=g++ -fortran=gfortran -mpi \
	-pthread -bfd=download -unwind=download \
  	-pdt=$CODAR_ROOT/sw/pdt-3.25 -pdt_c++=g++ \
	-adios=$CODAR_ROOT/sw/adios2 \
	-papi=$CODAR_ROOT/sw/papi-5.6.0 \
	-prefix=$CODAR_ROOT/sw/tau2
   make install
   cd $CODAR_ROOT
   echo "Installed tau2!"
fi
