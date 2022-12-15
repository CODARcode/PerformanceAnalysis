#!/bin/bash
set -e

rm -rf chimbuko

export CHIMBUKO_CONFIG=chimbuko_config.sh
source ${CHIMBUKO_CONFIG}

log_dir=chimbuko/logs

use_chimbuko=1
use_tau=1

#Launch Chimbuko services
if (( ${use_chimbuko} == 1 )); then
    echo "Running services"
    ${chimbuko_services} 2>&1 | tee services.log &
    echo "Waiting"
    while [ ! -f chimbuko/vars/chimbuko_ad_opts.var ]; do sleep 1; done
    ad_opts=$(cat chimbuko/vars/chimbuko_ad_opts.var)
else
    mkdir -p chimbuko/logs chimbuko/adios2
fi

if (( ${use_tau} == 0 )); then
    TAU_EXEC=
fi

#Launch AD on server
if (( ${use_chimbuko} == 1 )); then
    ad_run_server=$(cat chimbuko/vars/chimbuko_ad_cmdline.server.var)
    echo "Running AD for server with command:"
    echo ${ad_run_server}
    eval "${ad_run_server} &"
fi

#Launch server
export TAU_VERBOSE=0
${TAU_EXEC} ./server 9876  2>&1 | tee chimbuko/logs/server.log &
spid=$!

ip=$(ip -4 addr show ${service_node_iface} | grep -oP '(?<=inet\s)\d+(\.\d+){3}')
ip_port="tcp://${ip}:9876"

sleep 4

cycles=100
cycle_time=50 #ms
anom_freq=30 
anom_mult=20

#Launch AD on client
if (( ${use_chimbuko} == 1 )); then
    ad_run_client=$(cat chimbuko/vars/chimbuko_ad_cmdline.client.var)
    echo "Running AD for client"
    eval "${ad_run_client} &"
fi

#Launch client
export TAU_VERBOSE=0
${TAU_EXEC} ./client ${ip_port} ${cycles} ${cycle_time} ${anom_freq} ${anom_mult} 2>&1 | tee chimbuko/logs/client.log

wait
