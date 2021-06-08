#!/bin/bash
set -e

rm -rf chimbuko
rm -f provider.address

export CHIMBUKO_CONFIG=chimbuko_config.sh
source ${CHIMBUKO_CONFIG}

if (( 1 )); then
    echo "Running services"
    ${backend_root}/scripts/launch/run_services.sh 2>&1 | tee services.log &
    echo "Waiting"
    while [ ! -f chimbuko/vars/chimbuko_ad_cmdline.var ]; do sleep 1; done
    ad_opts=$(cat chimbuko/vars/chimbuko_ad_opts.var)
else
    mkdir -p chimbuko/logs chimbuko/adios2   
fi

log_dir=chimbuko/logs

if (( 1 )); then
    ad_run_server="driver ${TAU_ADIOS2_ENGINE} ${TAU_ADIOS2_PATH} ${TAU_ADIOS2_FILE_PREFIX}-server -program_idx 0 ${ad_opts} 2>&1 | tee ${log_dir}/ad_server.log"
    echo "Running AD for server with command:"
    echo ${ad_run_server}
    eval "${ad_run_server} &"
fi

${TAU_EXEC} ./server 9876  2>&1 | tee chimbuko/logs/server.log &
spid=$!

ip=$(ip -4 addr show ${service_node_iface} | grep -oP '(?<=inet\s)\d+(\.\d+){3}')
ip_port="tcp://${ip}:9876"

sleep 4

cycles=100
cycle_time=50 #ms
anom_freq=30 
anom_mult=20

if (( 1 )); then
    ad_run_client="driver ${TAU_ADIOS2_ENGINE} ${TAU_ADIOS2_PATH} ${TAU_ADIOS2_FILE_PREFIX}-client -program_idx 1 ${ad_opts} 2>&1 | tee ${log_dir}/ad_client.log"
    echo "Running AD for client"
    eval "${ad_run_client} &"
fi

export TAU_VERBOSE=1

${TAU_EXEC} ./client ${ip_port} ${cycles} ${cycle_time} ${anom_freq} ${anom_mult} 2>&1 | tee chimbuko/logs/client.log
