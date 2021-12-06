#!/bin/bash

set -e
set -u
set -o pipefail

if [[ "${CHIMBUKO_CONFIG}x" == "x" ]]; then
    echo "Chimbuko Services: Error: CHIMBUKO_CONFIG must be set"
    exit 1
fi
echo "CHIMBUKO_CONFIG=${CHIMBUKO_CONFIG}"
if [[ ! -f ${CHIMBUKO_CONFIG} ]]; then
    echo "Chimbuko Services: Error: CHIMBUKO_CONFIG is not accessible"
    exit 1
fi

source ${CHIMBUKO_CONFIG}

ps_extra_args=$1
extra_args=$2

#Set absolute paths
base=$(pwd)
viz_dir=${base}/chimbuko/viz
log_dir=${base}/chimbuko/logs
provdb_dir=${base}/chimbuko/provdb
var_dir=${base}/chimbuko/vars
bp_dir=${base}/chimbuko/adios2

#Get head node IP
ip=$(ip -4 addr show ${service_node_iface} | grep -oP '(?<=inet\s)\d+(\.\d+){3}')
echo "Chimbuko Services main: Launching Chimbuko services on interface ${service_node_iface} with host IP" ${ip}

#Committer
if (( ${use_provdb} == 1 )); then
    echo "==========================================="
    echo "Instantiating committer"
    echo "==========================================="
    for((i=0;i<provdb_ninstances;i++)); do
	echo "Chimbuko services main launching provDB committer ${i} of ${provdb_ninstances}"
	provdb_commit "${provdb_dir}" -instance ${i} -ninstances ${provdb_ninstances} -nshards ${provdb_nshards} -freq_ms ${provdb_commit_freq} 2>&1 | tee ${log_dir}/committer_${i}.log &
	sleep 1
    done
    sleep 3
fi

#Visualization
if (( ${use_viz} == 1 )); then
    echo "Chimbuko Services main: Error - viz currently does not support multiple provDB instances"
    exit 1
fi

if (( ${use_pserver} == 1 )); then
    pserver_addr="tcp://${ip}:${pserver_port}"  #address for parameter server in format "tcp://IP:PORT"    

    echo "==========================================="
    echo "Chimbuko Services main: Instantiating pserver"
    echo "==========================================="
    echo "Chimbuko Services main: Pserver $pserver_addr"

    pserver_alg=${ad_alg} #Pserver AD algorithm choice must match that used for the driver
    pserver_addr="tcp://${ip}:${pserver_port}"  #address for parameter server in format "tcp://IP:PORT"
    pserver -ad ${pserver_alg} -nt ${pserver_nt} -logdir ${log_dir} -port ${pserver_port} ${ps_extra_args} 2>&1 | tee ${log_dir}/pserver.log &

    ps_pid=$!
    extra_args+=" -pserver_addr ${pserver_addr}"
    sleep 2
fi

echo ${extra_args} > extra_args.out

wait

echo "Chimbuko Services main: Service script complete" $(date)
