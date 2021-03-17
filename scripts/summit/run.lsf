#!/bin/bash
# BEGIN LSF Directives
#BSUB -P CSC299
#BSUB -W 60
#BSUB -nnodes 1
#BSUB -J single-node
#BSUB -o single-node.o.%J
#BSUB -e single-node.e.%J
#BSUB -alloc_flags "smt4"

module load gcc/8.1.1
module load curl/7.63.0
module load zlib
module load zeromq
module load bzip2
module load python/3.7.0-anaconda3-5.3.0
module unload darshan-runtime
source activate chimbuko_viz

conda env list

#set -x

CODAR=/ccs/proj/csc299/codar
SW=$CODAR/sw

# Application: NWChem (use nwchem-1-tau-ga or nwchem-1-tau-full)
export NWCHEM=$CODAR/NWChem/nwchem-1-tau-ga
export NWCHEM_TOP=$NWCHEM
export NWCHEM_DAT=$CODAR/NWX_TA/TM_BI_4PGS/nwc_md

# CHIMBUKO (analysis and visualization)
export CHIMBUKO_AD_ROOT=$SW/chimbuko
export CHIMBUKO_VIS_ROOT=$CODAR/downloads/ChimbukoVisualizationII

# Required libraries (ADIOS2, TAU2, etc)
export ADIOS_ROOT=$SW/adios2
export TAU_ROOT=$SW/tau2/ibm64linux
export PDT_ROOT=$SW/pdt-3.25/ibm64linux
export PAPI_ROOT=$SW/papi-5.6.0
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:${ADIOS_ROOT}/lib64
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:${TAU_ROOT}/lib
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:${PDT_ROOT}/lib
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:${PAPI_ROOT}/lib

echo ""
echo "==========================================="
echo "Set working directory"
echo "==========================================="
BATCH_DIR=`pwd`

cd $MEMBERWORK/csc299
mkdir -p IPDPS19
cd IPDPS19
rm -rf single_node
mkdir -p single_node
cd single_node
mkdir logs
mkdir BP
mkdir db
mkdir executions
WORK_DIR=`pwd`

echo ""
echo "=========================================="
echo "User inputs"
echo "=========================================="
export BP_PREFIX=tau-metrics
export BP_INTERVAL=1000000
export BP_ENGINE=SST
export APP_NMPIS=20

echo ""
echo "==========================================="
echo "Config TAU environment"
echo "==========================================="
export TAU_MAKEFILE=${TAU_ROOT}/lib/Makefile.tau-papi-gnu-mpi-pthread-pdt-adios2
export TAU_PLUGINS_PATH=${TAU_ROOT}/lib/shared-papi-gnu-mpi-pthread-pdt-adios2
export TAU_PLUGINS=libTAU-adios2-trace-plugin.so
export TAU_ADIOS2_PERIODIC=1
export TAU_ADIOS2_PERIOD=${BP_INTERVAL}
export TAU_ADIOS2_SELECTION_FILE=${WORK_DIR}/sos_filter.txt
export TAU_ADIOS2_ENGINE=${BP_ENGINE}
export TAU_ADIOS2_FILENAME=${WORK_DIR}/BP/${BP_PREFIX}

echo ""
echo "==========================================="
echo "Config VIS SERVER"
echo "==========================================="
export SERVER_CONFIG="production"
export DATABASE_URL="sqlite:///${WORK_DIR}/db/main.sqlite"
export ANOMALY_STATS_URL="sqlite:///${WORK_DIR}/db/anomaly_stats.sqlite"
export ANOMALY_DATA_URL="sqlite:///${WORK_DIR}/db/anomaly_data.sqlite"
export FUNC_STATS_URL="sqlite:///${WORK_DIR}/db/func_stats.sqlite"
export CELERY_BROKER_URL="redis://"
export EXECUTION_PATH="${WORK_DIR}/executions"


echo ""
echo "==========================================="
echo "Config Anomaly Detection"
echo "==========================================="
export AD_SIGMA=6
export AD_WINSZ=5
export AD_INTERVAL=0


echo ""
echo "==========================================="
echo "Copy binaries & data to ${WORK_DIR}"
echo "==========================================="
cp $NWCHEM/bin/LINUX64/nwchem .
cp -r $CHIMBUKO_AD_ROOT/bin .
cp -r $CHIMBUKO_AD_ROOT/lib .
cp $NWCHEM_DAT/4pgs-1M-amber-manual-lipid-ion-dat.top .
cp $NWCHEM_DAT/4pgs-1M-amber-manual-lipid-ion-dat.db .
cp $NWCHEM_DAT/4pgs-1M-amber-manual-lipid-ion-dat_md.rst .
cp $NWCHEM_DAT/4pgs-1M-prepare-equil-md.nw .
cp $CHIMBUKO_AD_ROOT/scripts/summit/sos_filter.txt .
cp -r $CHIMBUKO_VIS_ROOT/redis-stable/redis.conf .
cp -r $CHIMBUKO_VIS_ROOT .
mv ChimbukoVisualizationII viz

sed -i "263s|dir .|dir ${WORK_DIR}|" redis.conf
sed -i "69s|bind 127.0.0.1|bind 0.0.0.0|" redis.conf
sed -i "136s|daemonize no|daemonize yes|" redis.conf
sed -i "158s|pidfile /var/run/redis_6379.pid|pidfile ${WORK_DIR}/redis.pid|" redis.conf
sed -i "171s|logfile "\"\""|logfile ${WORK_DIR}/logs/redis.log|" redis.conf

#sed -i "s|data 100|data 20|" 4pgs-1M-prepare-equil-md.nw

if true; then
  echo ""
  echo "==========================================="
  echo "Launch Chimbuko visualization server"
  echo "==========================================="
  cd $WORK_DIR/viz
  jsrun -n 1 -a 1 -c 1 -g 0 -r 1 ${BATCH_DIR}/run_webserver.sh "${WORK_DIR}/logs" \
  	  "--loglevel=info" \
          5001 ${WORK_DIR}/redis.conf &

  while [ ! -f webserver.host ];
  do
    sleep 1
  done
  WS_HOST=$(<webserver.host)
  while [ ! -f webserver.port ];
  do
    sleep 1
  done
  WS_PORT=$(<webserver.port)
  echo "WS_HOST: $WS_HOST"
  echo "WS_PORT: $WS_PORT"
fi

cd ${WORK_DIR}
if true; then
  echo ""
  echo "==========================================="
  echo "Launch Chimbuko parameter server"
  echo "==========================================="
  jsrun -n 1 -a 1 -c 1 -g 0 -r 1 ${BATCH_DIR}/run_pserver.sh 4 "${WORK_DIR}/parameters.txt" \
     $APP_NMPIS "http://${WS_HOST}:${WS_PORT}/api/anomalydata" &

  while [ ! -f ps.host ];
  do
     sleep 1
  done
  PS_HOST=$(<ps.host)
  while [ ! -f ps.port ];
  do
    sleep 1
  done
  PS_PORT=$(<ps.port)
  echo "PS_HOST: ${PS_HOST}"
  echo "PS_PORT: ${PS_PORT}"
  PS_TCP="tcp://${PS_HOST}:${PS_PORT}"
  echo "PS_TCP: ${PS_TCP}"
  while [ ! -f ps.pid ];
  do
     sleep 1
  done
  ps_pid=$(<ps.pid)
  echo "PS_PID: ${ps_pid}"
fi

echo ""
echo "=========================================="
echo "Launch Application with anomaly detectors"
echo "=========================================="
if true; then
  jsrun -n 20 -a 1 -c 1 -g 0 -r 20 -b none bin/driver $BP_ENGINE $WORK_DIR/BP $BP_PREFIX \
     "${EXECUTION_PATH}" $PS_TCP ${AD_SIGMA} ${AD_WINSZ} ${AD_INTERVAL} &
  sleep 1
fi

jsrun -n 20 -a 1 -c 1 -g 0 -r 20 -b none ./nwchem 4pgs-1M-prepare-equil-md.nw 

wait $ps_pid

if true; then
echo ""
echo "==========================================="
echo "Shutdown Chimbuko visualization server"
echo "==========================================="
cd $WORK_DIR/viz
jsrun -n 1 -a 1 -c 1 -g 0 -r 1 ${BATCH_DIR}/shutdown_webserver.sh ${WS_HOST} ${WS_PORT}
fi

cd $WORK_DIR
echo "BP size: `du -sh BP`"
echo "DB size: `du -sh db`"
echo "execution: `du -sh executions`"

rm -f *.top *.db *.rst nwchem
rm -rf bin lib viz

echo "Bye~~!!"


























