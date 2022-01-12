#!/bin/bash

export PATH=/install/AD/develop/bin:${PATH}
export LD_LIBRARY_PATH=/install/AD/develop/lib:${LD_LIBRARY_PATH}

ulimit -c unlimited
set -e
set -o pipefail

export CHIMBUKO_CONFIG=chimbuko_config.sh
source ${CHIMBUKO_CONFIG}

SERVICES=${chimbuko_services}

echo "Job starting"
date
echo "Driver is : " $(which driver)

rm -rf chimbuko

use_hpserver=1
if (( $use_hpserver == 1 )); then
    echo "Using hpserver"
    SERVICES=./run_services_hpserver.sh
fi

if (( 1 )); then
    echo "Launching services"

    #Run the services
    ${SERVICES} &

    #Wait for the services to start and generate their outputs
    while [ ! -f chimbuko/vars/chimbuko_ad_cmdline.var ]; do sleep 1; done
fi

#Get pserver IP
pserver_addr=$(grep -Eo "pserver_addr tcp.*\:[0-9]+" chimbuko/vars/chimbuko_ad_opts.var | sed 's/pserver_addr //')

exe=./benchmark_client
cycles=10
nfuncs=100
ncounters=100
nanomalies_per_func=1
cycle_time_ms=1000
perf_write_freq=5   #cycles
perf_dir=chimbuko/logs
benchmark_pattern="ping"   #"ad"

client_cmd="$exe ${pserver_addr} -cycles ${cycles} -nfuncs ${nfuncs} -ncounters ${ncounters} -nanomalies_per_func ${nanomalies_per_func} -cycle_time_ms ${cycle_time_ms} -benchmark_pattern ${benchmark_pattern} -perf_write_freq ${perf_write_freq} -perf_dir ${perf_dir} 2>&1 | tee chimbuko/logs/client.log"
#export CHIMBUKO_VERBOSE=1

if (( 1 )); then
    echo "Instantiating client"
    echo "Command is " $client_cmd
    eval "${client_cmd}"
fi

echo "Waiting for job completion"
wait

echo "Deleting pserver output data"
rm -f chimbuko/viz/*

echo "Done"
date
