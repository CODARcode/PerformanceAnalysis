#!/bin/bash
set -e

cmd=$1
config=$2
ad_opts=$3
rank=${OMPI_COMM_WORLD_RANK}

source ${config}

ad_cmd="driver ${TAU_ADIOS2_ENGINE} ${TAU_ADIOS2_PATH} ${TAU_ADIOS2_FILE_PREFIX}-${EXE_NAME} ${ad_opts} -rank ${rank} 2>&1 | tee chimbuko/logs/ad.${rank}.log"

echo "Starting AD rank ${rank} with command \"${ad_cmd}\""
eval "${ad_cmd} &"

echo "Starting application rank ${rank}"
eval "${cmd}"

wait
