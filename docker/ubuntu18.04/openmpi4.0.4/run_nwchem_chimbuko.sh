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
sleep 3

# NWChem environments
export NWCHEM_TOP=/Codar/nwchem-1
export NWCHEM_DAT=$NWCHEM_TOP/QA/tests/ethanol
export AD_ROOT=/opt/chimbuko/ad

# Chimbuko environments
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
mkdir -p logs DB BP executions

LOG_DIR=${BATCH_DIR}/logs
DB_DIR=${BATCH_DIR}/DB
BP_DIR=${BATCH_DIR}/BP

# TAU plug-in environments
BP_PREFIX=tau-metrics-nwchem
export TAU_ADIOS2_PERIODIC=1
export TAU_ADIOS2_PERIOD=1000000
export TAU_ADIOS2_SELECTION_FILE=${BATCH_DIR}/sos_filter.txt     #filter out some irrelevant functions
export TAU_ADIOS2_ENGINE=$ADIOS_MODE
export TAU_ADIOS2_FILENAME=${BP_DIR}/tau-metrics
#export TAU_VERBOSE=1

# visualization server
export DATABASE_URL="sqlite:///${DB_DIR}/main.sqlite"
export ANOMALY_STATS_URL="sqlite:///${DB_DIR}/anomaly_stats.sqlite"
export ANOMALY_DATA_URL="sqlite:///${DB_DIR}/anomaly_data.sqlite"
export FUNC_STATS_URL="sqlite:///${DB_DIR}/func_stats.sqlite"
export SHARDED_NUM=1  #Number of provdb shards
export C_FORCE_ROOT=1

#Chimbuko and NWChem environments
source /spack/spack/share/spack/setup-env.sh && spack load py-mochi-sonata
export PATH=${NWCHEM_TOP}/bin/LINUX64/:${PATH}
export PATH=${AD_ROOT}/bin/:${PATH}
export LD_LIBRARY_PATH=${AD_ROOT}/lib:${LD_LIBRARY_PATH}

# copy binaries and data
#cp $NWCHEM_TOP/bin/LINUX64/nwchem .
cp $NWCHEM_DAT/ethanol_md.nw .
cp $NWCHEM_DAT/*.pdb .
cp $NWCHEM_DAT/ethanol_md.rst .
cp $NWCHEM_DAT/ethanol_md.out .
cp /sos_filter.txt .

# modify NWChem script
sed -i 's/coord 0/coord 1/' ethanol_md.nw
sed -i 's/scoor 0/scoor 1/' ethanol_md.nw
sed -i 's/step 0.001/step 0.001/' ethanol_md.nw
sed -i '21s|set|#set|' ethanol_md.nw
sed -i '22s|#set|set|' ethanol_md.nw
sed -i "s|data 1000|data ${DATA_STEPS}|" ethanol_md.nw

extra_args=""
ps_extra_args=""
if (( 1 )); then
    echo ""
    echo "=========================================="
    echo "Launch Chimbuko provenance database"
    echo "=========================================="

    cd ${DB_DIR}
    
    rm -f provdb.*.unqlite
    ip=$(hostname -i)
    provdb_admin ${ip}:5555 2>&1 | tee ${LOG_DIR}/provdb.log &
    provdb_pid=$!

    sleep 1
    if ! [[ -f provider.address ]]; then
        echo "Provider address file not created after 1 second"
        exit 1
    fi

    prov_add=$(cat provider.address)
    extra_args="-provdb_addr ${prov_add}"
    ps_extra_args="-provdb_addr ${prov_add}"

    #For viz
    export PROVENANCE_DB="${DB_DIR}/"
    export PROVDB_ADDR=${prov_add}

    echo "Enabling provenance database with arg: ${extra_args}"
    sleep 2
    cd ${BATCH_DIR}
fi

using_viz=0
if (( 1 )); then
    echo ""
    echo "=========================================="
    echo "Launch Chimbuko visualization server"
    echo "=========================================="

    using_viz=1
    ip=$(hostname -i)

    cd ${VIZ_ROOT}

    echo "run redis ..."
    redis-stable/src/redis-server redis-stable/redis.conf 2>&1 | tee ${LOG_DIR}/redis.log &
    sleep 5

    echo "run celery ..."
    python3 manager.py celery --loglevel=info 2>&1 | tee ${LOG_DIR}/celery.log &
    sleep 5

    echo "create db ..."
    python3 manager.py createdb 2>&1 | tee ${LOG_DIR}/create_db.log
    sleep 2

    echo "run webserver (server config ${SERVER_CONFIG}) with provdb on ${PROVDB_ADDR}...  Logging  to ${LOG_DIR}/viz.log"
    python3 manager.py runserver --host 0.0.0.0 --port 5002 --debug 2>&1 | tee ${LOG_DIR}/viz.log &
    sleep 2

    cd -

    ws_addr="http://${ip}:5002/api/anomalydata"
    ps_extra_args+=" -ws_addr ${ws_addr}"
fi

if (( 1 )); then
    echo ""
    echo "=========================================="
    echo "Launch Chimbuko parameter server"
    echo "=========================================="
    ip=$(hostname -i)
    pserver_port=9999
    pserver_addr=tcp://${ip}:${pserver_port}

    pserver -nt 2 -logdir "${LOG_DIR}" -port ${pserver_port} ${ps_extra_args} 2>&1 | tee ${LOG_DIR}/pserver.log  &    
    ps_pid=$!
    sleep 2
    extra_args+=" -pserver_addr ${pserver_addr}"
fi


if [ "$ADIOS_MODE" == "SST" ]
then
    echo "Launch Application with anomaly detectors"
    mpirun --allow-run-as-root -n $NMPIS driver $ADIOS_MODE \
           ${BP_DIR} $BP_PREFIX -prov_outputdir "${BATCH_DIR}/executions" ${extra_args} -outlier_sigma ${AD_SIGMA} -anom_win_size ${AD_WINSZ} 2>&1 | tee ${LOG_DIR}/ad.log  &
    ad_pid=$!
    sleep 5

    cd $BATCH_DIR
    mpirun --allow-run-as-root -n $NMPIS nwchem ethanol_md.nw 2>&1 | tee logs/nwchem.log

    wait ${ad_pid}
else
    echo "Use BP mode"
    if ! $HAS_BPFILE
    then
        echo "Run NWChem"
        cd $BATCH_DIR
        mpirun --allow-run-as-root -n $NMPIS nwchem ethanol_md.nw 2>&1 | tee  logs/nwchem.log 
    fi
    echo "Run anomaly detectors"
    cd $WORK_DIR/ad
    mpirun --allow-run-as-root -n $NMPIS driver $ADIOS_MODE \
	$WORK_DIR/BP $BP_PREFIX -prov_outputdir "${WORK_DIR}/executions" ${extra_args} -outlier_sigma ${AD_SIGMA} -anom_win_size ${AD_WINSZ} -interval_msec ${AD_INTERVAL} 2>&1 | tee ${LOG_DIR}/ad.log
fi

echo "Waiting for services to finish"
wait $ps_pid
wait $provdb_pid

# wait about 10 min. so that users can keep interacting with visualization. 
#sleep 600

if (( ${using_viz} == 1 )); then
    echo ""
    echo "=========================================="
    echo "Shutdown Chimbuko visualization server"
    echo "=========================================="

    cd ${VIZ_ROOT}
    ./webserver/shutdown_webserver.sh
fi

echo "Bye~~!!"
