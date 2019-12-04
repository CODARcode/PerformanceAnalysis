#!/bin/bash

ROOT_DIR=`pwd`
BATCH_DIR="${ROOT_DIR}/bp-504"
mkdir -p $BATCH_DIR
cp run_webserver.sh ${BATCH_DIR}
cp shutdown_webserver.sh ${BATCH_DIR}
cp run_pserver.sh ${BATCH_DIR}

APPNAME=nwchem-504

# true to use TAU instrumented NWChem
USE_TAU=true
# true to use full TAU instrumented NWChem
USE_FULL=true

ADIOS=BPFile
ADIOS_INTERVAL=1000000
USE_C2=false

mkdir -p $BATCH_DIR


for (( nnodes=4; nnodes<=128; nnodes*=2 ))
do
  cd $ROOT_DIR
  # given condition, it modifies run.lsf file from top to bottom.
  # so that, it is important to check the below codes whenever
  # run.lsf file is modified.

  # number of MPI processors
  n_mpi=$(( nnodes * 20 ))
  if $USE_TAU; then
    jobname="${APPNAME}-${nnodes}-${ADIOS}"
  else
    jobname="${APPNAME}-${nnodes}"
  fi
  actual_nnodes=$nnodes
  if [ $USE_TAU == false ] || [ "${ADIOS}" == "BPFile"  ]; then
    actual_nnodes=$nnodes
  else
    actual_nnodes=$(( $nnodes + 2 ))
  fi
  
  cp run.lsf "${BATCH_DIR}/${jobname}.lsf"
  lsf="${jobname}.lsf"
  if $USE_C2; then
    n_rs=$(( $n_mpi / 10 ))
    ad_arg="-n ${n_rs} -a 10 -c 1 -g 0 -r 2"
    app_arg="-n ${n_mpi} -a 1 -c 2 -g 0 -r 20"
  else
    ad_arg="-n ${n_mpi} -a 1 -c 1 -g 0 -r 20"
    app_arg="-n ${n_mpi} -a 1 -c 1 -g 0 -r 20"
  fi
  cd $BATCH_DIR
  # LSF directives
  sed -i "5s|nnodes 1|nnodes ${actual_nnodes}|" $lsf
  sed -i "6s|single-node|${jobname}|" $lsf
  sed -i "7s|single-node|${jobname}|" $lsf
  sed -i "8s|single-node|${jobname}|" $lsf
  # Select NWChem binary
  echo -n "MPI: ${n_mpi}, "
  if [ $USE_TAU == false ]; then
    echo "USE non-TAU instrumented NWChem"
    sed -i "28s|nwchem-1-tau-ga|nwchem-1|" $lsf
  elif [ $USE_FULL == false ]; then
    echo "USE TAU instrumented NWChem (filtered)"
  else
    echo "USE TAU instrumented NWChem (full)"
    sed -i "28s|nwchem-1-tau-ga|nwchem-1-tau-full|" $lsf
  fi  
  # Select Working directory
  sed -i "55s|single_node|${jobname}|" $lsf
  sed -i "56s|single_node|${jobname}|" $lsf
  sed -i "57s|single_node|${jobname}|" $lsf  
  # Set ADIOS-TAU mode and interval
  sed -i "69s|1000000|${ADIOS_INTERVAL}|" $lsf
  sed -i "70s|SST|${ADIOS}|" $lsf
  sed -i "71s|APP_NMPIS=20|APP_NMPIS=${n_mpi}|" $lsf
  if [ $USE_TAU == false ] || [ "${ADIOS}" = "BPFile" ]; then
    # disable visualization server
    sed -i "132s|true|false|" $lsf
    # disable parameter server
    sed -i "157s|true|false|" $lsf
    # disable anomaly detector
    sed -i "191s|true|false|" $lsf
    # finalize
    sed -i "199s|wait|#wait|" $lsf
    sed -i "201s|true|false|" $lsf
  else
    # run visualization server with maximum celery concurrency
    sed -i "138s|-c 1|-c 42|" $lsf
    #sed -i "152s|--concurrency=10|--concurrency=100|" $lsf
    # run parameter server
    sed -i "162s|-c 1| -c 42|" $lsf
    sed -i "162s|run_pserver.sh 4|run_pserver.sh 160|" $lsf
    # run anomaly detector
    sed -i "192s|-n 20 -a 1 -c 1 -g 0 -r 20|${ad_arg}|" $lsf
  fi
  # nwchem
  sed -i "197s|-n 20 -a 1 -c 1 -g 0 -r 20|${app_arg}|" $lsf

  bsub $lsf
  sleep 1
done
