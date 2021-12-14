#!/bin/bash
set -e

rm -rf chimbuko
export CHIMBUKO_CONFIG=chimbuko_config.sh
source ${CHIMBUKO_CONFIG}

if (( 1 )); then
    echo "Running services"
    ${chimbuko_services} 2>&1 | tee services.log &
    echo "Waiting"
    while [ ! -f chimbuko/vars/chimbuko_ad_cmdline.var ]; do sleep 1; done
    ad_cmd=$(cat chimbuko/vars/chimbuko_ad_cmdline.var)
fi

provdb_addr_dir=chimbuko/provdb
exe=./benchmark_client
cycles=100
callstack_size=2
ncounters=5
winsize=5
comm_messages_per_winevent=1
anomalies_per_cycle=1
normal_events_per_cycle=1
cycle_time_ms=1000
nshards=${provdb_nshards}
ninstances=${provdb_ninstances}
perf_write_freq=5   #cycles
perf_dir=chimbuko/logs
do_state_dump=0
ranks=4

client_cmd="$exe ${provdb_addr_dir} -cycles ${cycles} -callstack_size ${callstack_size} -ncounters ${ncounters} -winsize ${winsize} -comm_messages_per_winevent ${comm_messages_per_winevent} -anomalies_per_cycle ${anomalies_per_cycle} -normal_events_per_cycle ${normal_events_per_cycle} -cycle_time_ms ${cycle_time_ms} -nshards ${nshards} -ninstances ${ninstances} -perf_write_freq ${perf_write_freq} -perf_dir ${perf_dir} -do_state_dump ${do_state_dump} 2>&1 | tee chimbuko/logs/client.log"

if (( 1 )); then
    echo "Instantiating client"
    echo "Command is " $client_cmd
    eval "mpirun --allow-run-as-root -n ${ranks} ${client_cmd} &"
fi

wait
