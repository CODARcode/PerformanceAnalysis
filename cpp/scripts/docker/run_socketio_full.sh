#!/bin/bash

export NWCHEM_TOP=/Codar/nwchem-1
export NWCHEM_DAT=$NWCHEM_TOP/QA/tests/ethanol

export CHIMBUKO_ROOT=/PerformanceAnalysis
export CHIMBUKO_VIS_ROOT=/ChimbukoVisualization

export TAU_ROOT=/opt/tau2/x86_64
export TAU_MAKEFILE=$TAU_ROOT/lib/Makefile.tau-papi-mpi-pthread-pdt-adios2
export TAU_PLUGINS_PATH=$TAU_ROOT/lib/shared-papi-mpi-pthread-pdt-adios2
export TAU_PLUGINS=libTAU-adios2-trace-plugin.so

HAS_BPFILE=false

mkdir -p test
cd test
rm -rf logs
mkdir -p logs
mkdir -p BP
WORK_DIR=`pwd`

ADIOS_MODE=BPFile
BP_PREFIX=tau-metrics-nwchem
export TAU_ADIOS2_PERIODIC=1
export TAU_ADIOS2_PERIOD=1000000
export TAU_ADIOS2_SELECTION_FILE=$WORK_DIR/sos_filter.txt
export TAU_ADIOS2_ENGINE=$ADIOS_MODE
export TAU_ADIOS2_FILENAME=$WORK_DIR/BP/tau-metrics
#export TAU_VERBOSE=1

# visualization server
export SERVER_CONFIG="production"
export DATABASE_URL="sqlite:///${WORK_DIR}/logs/test_db.sqlite"

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
sed -i '21s|set|#set|' ethanol_md.nw
sed -i '22s|#set|set|' ethanol_md.nw
sed -i 's/data 1000/data 50000/' ethanol_md.nw

date
hostname
ls -l

NMPIS=5

# echo ""
# echo "=========================================="
# echo "Launch Chimbuko visualization server"
# echo "=========================================="
cd $CHIMBUKO_VIS_ROOT

echo "run redis ..."
webserver/run-redis.sh &
#>"${WORK_DIR}/logs/redis.log" &
#2>&1 &
sleep 10

echo "run celery ..."
# only for docker
export C_FORCE_ROOT=1
# python3 manager.py celery --loglevel=info --concurrency=10 -f "${WORK_DIR}/logs/celery.log" &
python3 manager.py celery --loglevel=info --concurrency=4 --logfile=${WORK_DIR}/logs/celery.log &
#    >"${WORK_DIR}/logs/celery.log" 2>&1 &
sleep 10

echo "create database @ ${DATABASE_URL}"
python3 manager.py createdb

echo "run webserver ..."
python3 manager.py runserver --host 0.0.0.0 --port 5000 \
    >"${WORK_DIR}/logs/webserver.log" 2>&1 &
sleep 10

cd $WORK_DIR
echo ""
echo "=========================================="
echo "Launch Chimbuko parameter server"
echo "=========================================="
echo "run parameter server ..."
bin/pserver 2 "${WORK_DIR}/logs/parameters.log" $NMPIS "http://0.0.0.0:5000/api/anomalydata" &
    # >"${WORK_DIR}/logs/ps.log" 2>&1 &
ps_pid=$!
sleep 5

echo ""
echo "=========================================="
echo "Launch Application with anomaly detectors"
echo "=========================================="
if [ "$ADIOS_MODE" == "SST" ]
then
    echo "Use SST mode: NWChem + AD"
    mpirun --allow-run-as-root -n $NMPIS \
        bin/driver $ADIOS_MODE $WORK_DIR/BP $BP_PREFIX $WORK_DIR/BP &
        # >logs/ad.log 2>&1 &
    sleep 5
    mpirun --allow-run-as-root -n $NMPIS nwchem ethanol_md.nw 
        # >logs/nwchem.log 2>&1 
else
    echo "Use BP mode"
    if ! $HAS_BPFILE
    then
        echo "Run NWChem"
        mpirun --allow-run-as-root -n $NMPIS nwchem ethanol_md.nw >logs/nwchem.log 2>&1 
    fi
    echo "Run anomaly detectors"
    mpirun --allow-run-as-root -n $NMPIS bin/driver $ADIOS_MODE $WORK_DIR/BP $BP_PREFIX \
        "http://0.0.0.0:5000/api/executions"  "tcp://0.0.0.0:5559" 10000
        # >logs/ad.log 2>&1 
fi

echo ""
echo "=========================================="
echo "Shutdown Chimbuko parameter server"
echo "=========================================="
# bin/pshutdown "tcp://0.0.0.0:5559"
echo "shutdown parameter server ..."
wait $ps_pid

sleep 30
cd $CHIMBUKO_VIS_ROOT
echo ""
echo "=========================================="
echo "Shutdown Chimbuko visualization server"
echo "=========================================="
echo "shutdown webserver ..."
curl -X GET http://0.0.0.0:5000/stop
echo "shutdown celery workers ..."
pkill -9 -f 'celery worker'
echo "shutdown redis server ..."
webserver/shutdown-redis.sh

echo "Bye~~!!"
