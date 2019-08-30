#!/bin/bash

export NWCHEM_TOP=/Codar/nwchem-1
export NWCHEM_DAT=$NWCHEM_TOP/QA/tests/ethanol

export CHIMBUKO_ROOT=/PerformanceAnalysis

export TAU_ROOT=/opt/tau2/x86_64
export TAU_MAKEFILE=$TAU_ROOT/lib/Makefile.tau-papi-mpi-pthread-pdt-adios2
export TAU_PLUGINS_PATH=$TAU_ROOT/lib/shared-papi-mpi-pthread-pdt-adios2
export TAU_PLUGINS=libTAU-adios2-trace-plugin.so

rm -rf test
mkdir -p test
cd test
mkdir -p BP
WORK_DIR=`pwd`

ADIOS_MODE=SST
BP_PREFIX=tau-metrics
export TAU_ADIOS2_PERIODIC=1
export TAU_ADIOS2_PERIOD=1000000
export TAU_ADIOS2_SELECTION_FILE=$WORK_DIR/sos_filter.txt
export TAU_ADIOS2_ENGINE=$ADIOS_MODE
export TAU_ADIOS2_FILENAME=$WORK_DIR/BP/$BP_PREFIX
#export TAU_VERBOSE=1

cp $NWCHEM_TOP/bin/LINUX64/nwchem .
cp $NWCHEM_DAT/ethanol_md.nw .
cp $NWCHEM_DAT/*.pdb .
cp $NWCHEM_DAT/ethanol_md.rst .
cp $NWCHEM_DAT/ethanol_md.out .
cp $CHIMBUKO_ROOT/scripts/docker/sos_filter.txt .
cp -r $CHIMBUKO_ROOT/bin .
cp -r $CHIMBUKO_ROOT/lib .

sed -i 's/coord 0/coord 1/' ethanol_md.nw
sed -i 's/scoor 0/scoor 1/' ethanol_md.nw
sed -i 's/step 0.001/step 0.001/' ethanol_md.nw
sed -i 's/data 1000/data 500/' ethanol_md.nw

date
hostname
ls -l

NMPIS=2

# run web server
python3 bin/ws_flask_stat.py "0.0.0.0" "5001" "webserver.log" &
sleep 1

# run parameter server
bin/pserver 2 pserver.log $NMPIS "http://0.0.0.0:5001/post" &
ps_pid=$!
sleep 1

if [ "$ADIOS_MODE" == "SST" ]
then
    # run ad modules
    mpirun --allow-run-as-root -n $NMPIS bin/driver $ADIOS_MODE $WORK_DIR/BP $BP_PREFIX $WORK_DIR/BP "tcp://0.0.0.0:5559" >ad.log 2>&1 &
    sleep 1
    # run nwchem
    mpirun --allow-run-as-root -n $NMPIS nwchem ethanol_md.nw >nwchem.log 2>&1 
else
    # run nwchem
    mpirun --allow-run-as-root -n $NMPIS nwchem ethanol_md.nw >nwchem.log 2>&1 

    # run ad modules
    mpirun --allow-run-as-root -n $NMPIS bin/driver $ADIOS_MODE $WORK_DIR/BP $BP_PREFIX $WORK_DIR/BP "tcp://0.0.0.0:5559" >ad.log 2>&1 
fi

# shutdown parameter server
# bin/pshutdown "tcp://0.0.0.0:5559"
wait $ps_pid

# shutdown webserver
curl -X POST "http://0.0.0.0:5001/shutdown"


# mpirun --allow-run-as-root -n 2 nwchem ethanol_md.nw
