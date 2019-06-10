#!/bin/bash

module load gcc/8.1.1
module load zlib/1.2.11
module load bzip2
module load zeromq

SW=/ccs/proj/csc299/codar/sw
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:$SW/libfabric-1.7.0/lib
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:$SW/blosc/lib
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:$SW/pdt-3.25/ibm64linux/lib
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:$SW/papi-5.6.0/lib
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:$SW/sz-1.4.13.0/lib
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:$SW/adios2/lib64
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:$SW/tau2/ibm64linux/lib

export PATH=${PATH}:$SW/adios2/bin
export PATH=${PATH}:$SW/tau2/ibm64linux/bin

# common nwchem settings
#export ARMCI_NETWORK=MPI_TS
export ARMCI_NETWORK=MPI-PR
export BLASOPT="`adios2-config -c -f`"
export NWCHEM_MODULES="md"
nwchem_root=./nwchem-1

install_with_tau=true
install_clean=false
if $install_with_tau
then
   nwchem_root=./nwchem-1-tau
   export TAU_MAKEFILE=$SW/tau2/ibm64linux/lib/Makefile.tau-papi-gnu-mpi-pthread-pdt-adios2
   export USE_TAU="source"
fi

cd $nwchem_root
echo "`pwd`"

sleep 10
rm -f ./bin/*/nwchem
if $install_clean
then
   echo "***** CLEAN NWCHEM *****"
   sleep 1
   ./contrib/distro-tools/build_nwchem realclean
fi

echo "***** INSTALL NWCHEM *****"
sleep 1
./contrib/distro-tools/build_nwchem 2>&1 | tee build_nwchem.log
