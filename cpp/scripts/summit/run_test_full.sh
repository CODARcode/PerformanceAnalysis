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

# starting from here ...
export CHIMBUKO_VIS_ROOT=/ChimbukoVisualization

export TAU_ROOT=/opt/tau2/x86_64
export TAU_MAKEFILE=$TAU_ROOT/lib/Makefile.tau-papi-mpi-pthread-pdt-adios2
export TAU_PLUGINS_PATH=$TAU_ROOT/lib/shared-papi-mpi-pthread-pdt-adios2
export TAU_PLUGINS=libTAU-adios2-trace-plugin.so

rm -rf test
mkdir -p test
cd test
mkdir -p BP
mkdir -p logs
WORK_DIR=`pwd`

ADIOS_MODE=BPFile
HAS_BPFILE=false
BP_PREFIX=tau-metrics
export TAU_ADIOS2_PERIODIC=1
export TAU_ADIOS2_PERIOD=1000000
export TAU_ADIOS2_SELECTION_FILE=$WORK_DIR/sos_filter.txt
export TAU_ADIOS2_ENGINE=$ADIOS_MODE
export TAU_ADIOS2_FILENAME=$WORK_DIR/BP/$BP_PREFIX
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
sed -i 's/data 1000/data 10000/' ethanol_md.nw

date
hostname
ls -l

NMPIS=2

echo ""
echo "=========================================="
echo "Launch Chimbuko visualization server"
echo "=========================================="
cd $CHIMBUKO_VIS_ROOT
echo "run redis ..."
webserver/run-redis.sh >"${WORK_DIR}/logs/redis.log" 2>&1 &
sleep 1

echo "run celery ..."
# only for docker
export C_FORCE_ROOT=1
# python3 manager.py celery --loglevel=info --concurrency=10 -f "${WORK_DIR}/logs/celery.log" &
python3 manager.py celery --loglevel=info --concurrency=10 \
    >"${WORK_DIR}/logs/celery.log" 2>&1 &
sleep 1

echo "run webserver ..."
uwsgi --gevent 100 --http 127.0.0.1:5001 --wsgi-file server/wsgi.py \
    --pidfile "${WORK_DIR}/logs/webserver.pid" \
    >"${WORK_DIR}/logs/webserver.log" 2>&1 &

echo "create database @ ${DATABASE_URL}"
python3 manager.py createdb

cd $WORK_DIR

echo ""
echo "=========================================="
echo "Launch Chimbuko parameter server"
echo "=========================================="
echo "run parameter server ..."
bin/pserver 2 "${WORK_DIR}/logs/parameters.log" $NMPIS \
    "http://127.0.0.1:5001/api/anomalydata" \
    >"${WORK_DIR}/logs/ps.log" 2>&1 &
ps_pid=$!
sleep 1

echo ""
echo "=========================================="
echo "Launch Application with anomaly detectors"
echo "=========================================="
if [ "$ADIOS_MODE" == "SST" ]
then
    echo "Use SST mode: NWChem + AD"
    mpirun --allow-run-as-root -n $NMPIS \
        bin/driver $ADIOS_MODE $WORK_DIR/BP $BP_PREFIX $WORK_DIR/BP "tcp://0.0.0.0:5559" \
        >logs/ad.log 2>&1 &
    sleep 1
    mpirun --allow-run-as-root -n $NMPIS nwchem ethanol_md.nw \
        >logs/nwchem.log 2>&1 
else
    echo "Use BP mode"
    if ! $HAS_BPFILE
    then
        echo "Run NWChem"
        mpirun --allow-run-as-root -n $NMPIS nwchem ethanol_md.nw \
            >logs/nwchem.log 2>&1 
    fi
    echo "Run anomaly detectors"
    mpirun --allow-run-as-root -n $NMPIS \
        bin/driver $ADIOS_MODE $WORK_DIR/BP $BP_PREFIX $WORK_DIR/BP "tcp://0.0.0.0:5559" \
        >logs/ad.log 2>&1 
fi

echo ""
echo "=========================================="
echo "Shutdown Chimbuko parameter server"
echo "=========================================="
# bin/pshutdown "tcp://0.0.0.0:5559"
echo "shutdown parameter server ..."
wait $ps_pid

cd $CHIMBUKO_VIS_ROOT
echo ""
echo "=========================================="
echo "Shutdown Chimbuko visualization server"
echo "=========================================="
echo "shutdown webserver ..."
uwsgi --stop "${WORK_DIR}/logs/webserver.pid"
echo "shutdown celery workers ..."
pkill -9 -f 'celery worker'
echo "shutdown redis server ..."
webserver/shutdown-redis.sh

echo "Bye~~!!"
