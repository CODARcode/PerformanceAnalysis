#!/bin/bash

ulimit -c unlimited
set -e
set -o pipefail


exe=./benchmark
cycles=100
nfuncs=100
algorithm="hbos"
hbos_bins=200

export OMPI_ALLOW_RUN_AS_ROOT=1
export OMPI_ALLOW_RUN_AS_ROOT_CONFIRM=1

export TAU_VERBOSE=1
#client_cmd="mpirun -n 1 valgrind --tool=callgrind $exe -cycles ${cycles} -nfuncs ${nfuncs} -algorithm ${algorithm} -hbos_bins ${hbos_bins} 2>&1 | tee run.log"
client_cmd="mpirun -n 1 $exe -cycles ${cycles} -nfuncs ${nfuncs} -algorithm ${algorithm} -hbos_bins ${hbos_bins} 2>&1 | tee run.log"
#export CHIMBUKO_VERBOSE=1
#

echo "Instantiating client"
echo "Command is " $client_cmd
eval "${client_cmd}"

echo "Done"
date
