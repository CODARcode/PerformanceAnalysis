#!/bin/bash

# --------------------------------------------------------
# Input arguments
# --------------------------------------------------------

# nwchem arguments
# - NMPIS: the number of MPI processes 
#          (also applied to AD modules)
# - data steps: the number of data steps
#               (the more larger steps, the more longer processing)
NMPIS=${1:-5}
DATA_STEPS=${2:-50000}

# chimbuko arguments
# - ADIOS_MODE: [SST, BPFile]
# - HAS_BPFILE: true if BPFile is available 
#               (currently, it must be false for docker run)
# - AD_SIGMA: AD sigma value
#              (the larger value, the fewer anomalies or fewer false positives)
# - AD_WINSZ: AD time window size
#              (the larger window, the more informative calls stack view in viz)
# - AD_INTERVAL: AD time interval
#              (only for debugging purpose with BPFile adios mode)
# - BATCH_DIR: batch directory
ADIOS_MODE=${3:-SST}
HAS_BPFILE=${4:-false}
AD_SIGMA=${5:-6}
AD_WINSZ=${6:-10}
AD_INTERVAL=${7:-1000}
BATCH_DIR=${8:-/test}

echo "============================"
echo "NMPIS: ${NMPIS}"
echo "NWCHEM DATA STEPS: ${DATA_STEPS}"
echo "ADIOS MODE: ${ADIOS_MODE}"
echo "HAS BPFile: ${HAS_BPFILE}"
echo "AD SIGMA: ${AD_SIGMA}"
echo "AD WINSZ: ${AD_WINSZ}"
echo "AD INTERVAL: ${AD_INTERVAL} msec"
echo "BATCH DIR: ${BATCH_DIR}"
echo "============================"
sleep 10

# NWChem environments
export NWCHEM_TOP=/Codar/nwchem-1
export NWCHEM_DAT=$NWCHEM_TOP/QA/tests/ethanol

# Chimbuko environments
export AD_ROOT=/opt/chimbuko/ad
export VIZ_ROOT=/opt/chimbuko/viz

# TAU environments
export TAU_ROOT=/opt/tau2/x86_64
export TAU_MAKEFILE=$TAU_ROOT/lib/Makefile.tau-papi-mpi-pthread-pdt-adios2
export TAU_PLUGINS_PATH=$TAU_ROOT/lib/shared-papi-mpi-pthread-pdt-adios2
export TAU_PLUGINS=libTAU-adios2-trace-plugin.so

# Create work directory under the given batch directory
mkdir -p $BATCH_DIR
cd $BATCH_DIR
rm -rf DB executions logs
mkdir -p logs
mkdir -p DB
mkdir -p BP
mkdir -p executions
WORK_DIR=`pwd`

# TAU plug-in environments
BP_PREFIX=tau-metrics-nwchem
export TAU_ADIOS2_PERIODIC=1
export TAU_ADIOS2_PERIOD=1000000
export TAU_ADIOS2_SELECTION_FILE=$WORK_DIR/sos_filter.txt
export TAU_ADIOS2_ENGINE=$ADIOS_MODE
export TAU_ADIOS2_FILENAME=$WORK_DIR/BP/tau-metrics
#export TAU_VERBOSE=1

# visualization server
export SERVER_CONFIG="production"
export DATABASE_URL="sqlite:///${WORK_DIR}/DB/main.sqlite"
export ANOMALY_STATS_URL="sqlite:///${WORK_DIR}/DB/anomaly_stats.sqlite"
export ANOMALY_DATA_URL="sqlite:///${WORK_DIR}/DB/anomaly_data.sqlite"
export FUNC_STATS_URL="sqlite:///${WORK_DIR}/DB/func_stats.sqlite"
export EXECUTION_PATH=$WORK_DIR/executions

# copy binaries and data to WORK_DIR
cp $NWCHEM_TOP/bin/LINUX64/nwchem .
cp $NWCHEM_DAT/ethanol_md.nw .
cp $NWCHEM_DAT/*.pdb .
cp $NWCHEM_DAT/ethanol_md.rst .
cp $NWCHEM_DAT/ethanol_md.out .
cp /sos_filter.txt .
cp -r $AD_ROOT .
cp -r $VIZ_ROOT .

# modify NWChem script
sed -i 's/coord 0/coord 1/' ethanol_md.nw
sed -i 's/scoor 0/scoor 1/' ethanol_md.nw
sed -i 's/step 0.001/step 0.001/' ethanol_md.nw
sed -i '21s|set|#set|' ethanol_md.nw
sed -i '22s|#set|set|' ethanol_md.nw
sed -i "s|data 1000|data ${DATA_STEPS}|" ethanol_md.nw


echo ""
echo "=========================================="
echo "Launch Chimbuko visualization server"
echo "=========================================="
cd $WORK_DIR/viz

echo "run redis ..."
webserver/run-redis.sh &
sleep 30

echo "run celery ..."
# only for docker
export C_FORCE_ROOT=1
python3 manager.py celery --loglevel=info --concurrency=4 --logfile=${WORK_DIR}/logs/celery.log &
sleep 10

echo "create database @ ${DATABASE_URL}"
python3 manager.py createdb
sleep 10

echo "run webserver ..."
python3 manager.py runserver --host 0.0.0.0 --port 5000 \
    >"${WORK_DIR}/logs/webserver.log" 2>&1 &
sleep 10

echo ""
echo "=========================================="
echo "Launch Chimbuko parameter server"
echo "=========================================="
cd $WORK_DIR/ad
echo "run parameter server ..."
bin/app/pserver 2 "${WORK_DIR}/logs/parameters.log" $NMPIS "http://0.0.0.0:5000/api/anomalydata" &
ps_pid=$!
sleep 5

if [ "$ADIOS_MODE" == "SST" ]
then
    echo "Launch Application with anomaly detectors"
    cd $WORK_DIR/ad
    mpirun --allow-run-as-root -n $NMPIS bin/app/driver $ADIOS_MODE \
        $WORK_DIR/BP $BP_PREFIX "${WORK_DIR}/executions" "tcp://0.0.0.0:5559" ${AD_SIGMA} ${AD_WINSZ} 0 &
    sleep 5

    cd $WORK_DIR
    mpirun --allow-run-as-root -n $NMPIS nwchem ethanol_md.nw 
else
    echo "Use BP mode"
    if ! $HAS_BPFILE
    then
        echo "Run NWChem"
        cd $WORK_DIR
        mpirun --allow-run-as-root -n $NMPIS nwchem ethanol_md.nw >logs/nwchem.log 2>&1 
    fi
    echo "Run anomaly detectors"
    cd $WORK_DIR/ad
    mpirun --allow-run-as-root -n $NMPIS bin/app/driver $ADIOS_MODE $WORK_DIR/BP $BP_PREFIX \
        "${WORK_DIR}/executions" "tcp://0.0.0.0:5559" ${AD_SIGMA} ${AD_WINSZ} ${AD_INTERVAL}
fi

wait $ps_pid

# wait about 10 min. so that users can keep interacting with visualization. 
sleep 600
echo ""
echo "=========================================="
echo "Shutdown Chimbuko visualization server"
echo "=========================================="
cd $WORK_DIR/viz
curl -X GET http://0.0.0.0:5000/tasks/inspect
echo "shutdown webserver ..."
curl -X GET http://0.0.0.0:5000/stop
echo "shutdown celery workers ..."
pkill -9 -f 'celery worker'
echo "shutdown redis server ..."
webserver/shutdown-redis.sh

sleep 30
cd $WORK_DIR
echo "Bye~~!!"
