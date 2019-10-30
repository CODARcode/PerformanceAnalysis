#!/bin/bash
#
# this is only for test purpose and not for job submmission. 
#

module load gcc/8.1.1
module load curl/7.63.0
module load zlib
module load zeromq
module load bzip2
module load python/3.7.0-anaconda3-5.3.0
module unload darshan-runtime

# set -x

source activate chimbuko_viz

CODAR=/ccs/proj/csc299/codar
SW=$CODAR/sw

ADIOS_MODE=SST
BP_PREFIX=tau-metrics
NMPIS=3

export NWCHEM=$CODAR/NWChem/nwchem-1-tau
export NWCHEM_TOP=$NWCHEM
# export NWCHEM_DAT=$CODAR/NWX_TA/TM_BI_4PGS/nwc_md
export NWCHEM_DAT=$NWCHEM_TOP/QA/tests/ethanol

# export CHIMBUKO_ROOT=$SW/chimbuko
export CHIMBUKO_ROOT=/ccs/proj/csc299/codar/downloads/PerformanceAnalysis/cpp

export ADIOS_ROOT=$SW/adios2
export TAU_ROOT=$SW/tau2/ibm64linux
export PDT_ROOT=$SW/pdt-3.25/ibm64linux
export PAPI_ROOT=$SW/papi-5.6.0

export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:${ADIOS_ROOT}/lib64
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:${TAU_ROOT}/lib
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:${PDT_ROOT}/lib
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:${PAPI_ROOT}/lib

export TAU_MAKEFILE=${TAU_ROOT}/lib/Makefile.tau-papi-gnu-mpi-pthread-pdt-adios2
export TAU_PLUGINS_PATH=${TAU_ROOT}/lib/shared-papi-gnu-mpi-pthread-pdt-adios2
export TAU_PLUGINS=libTAU-adios2-trace-plugin.so

rm -rf test

mkdir -p test
cd test
mkdir -p BP
mkdir -p logs
WORK_DIR=`pwd`

export TAU_ADIOS2_PERIODIC=1
export TAU_ADIOS2_PERIOD=1000000
# export TAU_ADIOS2_USE_SELECTION_DEFAULT=true
export TAU_ADIOS2_SELECTION_FILE=${WORK_DIR}/sos_filter.txt
export TAU_ADIOS2_ENGINE=$ADIOS_MODE
export TAU_ADIOS2_FILENAME=${WORK_DIR}/BP/tau-metrics

# copy binaries
cp $NWCHEM/bin/LINUX64/nwchem .
cp -r $CHIMBUKO_ROOT/bin .
cp -r $CHIMBUKO_ROOT/lib .
cp $CHIMBUKO_ROOT/scripts/summit/sos_filter.txt .
# copy required data for the preparation
cp $NWCHEM_DAT/ethanol_md.nw .
cp $NWCHEM_DAT/*.pdb .
cp $NWCHEM_DAT/ethanol_md.rst .
cp $NWCHEM_DAT/ethanol_md.out .
sed -i 's/coord 0/coord 1/' ethanol_md.nw
sed -i 's/scoor 0/scoor 1/' ethanol_md.nw
#sed -i 's/step 0.001/step 0.001/' ethanol_md.nw
#sed -i 's/data 1000/data 2000/' ethanol_md.nw
NWCHEM_NW=ethanol_md.nw

# cp $NWCHEM_DAT/4pgs-1M-amber-manual-lipid-ion-dat.top .
# cp $NWCHEM_DAT/4pgs-1M-amber-manual-lipid-ion-dat.db .
# cp $NWCHEM_DAT/4pgs-1M-amber-manual-lipid-ion-dat_md.rst .
# cp $NWCHEM_DAT/4pgs-1M-prepare-equil-md.nw .
# sed -i 's/data 100/data 10/' 4pgs-1M-prepare-equil-md.nw
# NWCHEM_NW=4pgs-1M-prepare-equil-md.nw

date
hostname
ls -l

HOST=`hostname`
WS_ADDR="http://${HOST}:5001"

# run web server
python3 bin/ws_flask_stat.py $HOST "5001" "${WORK_DIR}/logs/webserver.log" &
sleep 1

# run parameter server
mpirun -n 1 bin/pserver 2 pserver.log $NMPIS "${WS_ADDR}/post" &
ps_pid=$!
sleep 1

if [ "$ADIOS_MODE" == "SST" ]
then
     # run ad modules
     mpirun -n $NMPIS bin/driver $ADIOS_MODE $WORK_DIR/BP $BP_PREFIX $WORK_DIR/BP "tcp://${HOST}:5559" >"${WORK_DIR}/logs/ad.log" 2>&1 &
     sleep 1
     # run nwchem
     mpirun -n $NMPIS nwchem $NWCHEM_NW >"${WORK_DIR}/logs/nwchem.log" 2>&1 
else
     # run nwchem
     mpirun -n $NMPIS nwchem $NWCHEM_NW >"${WORK_DIR}/logs/nwchem.log" 2>&1 

     # run ad modules
     mpirun -n $NMPIS bin/driver $ADIOS_MODE $WORK_DIR/BP $BP_PREFIX $WORK_DIR/BP "tcp://${HOST}:5559" >"${WORK_DIR}/logs/ad.log" 2>&1 
fi

# shutdown parameter server
# bin/pshutdown "tcp://0.0.0.0:5559"
wait $ps_pid

# shutdown webserver
curl -X POST "${WS_ADDR}/shutdown"
