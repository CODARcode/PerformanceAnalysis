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
    ad_opts=$(cat chimbuko/vars/chimbuko_ad_opts.var)
fi

#Instantiate the application alongside an instance of the AD
#We should use different filenames for the main app to prevent them from writing to the same file (the rank index is 0 always as no MPI)
#Alternatively could use different directories
instances=2

for (( i=0; i<${instances}; i++ ));
do
    fn_tau=${TAU_ADIOS2_PATH}/tau-metrics-${i}
    fn_driver=tau-metrics-${i}-main

    if (( 1 )); then
	#We will ask Chimbuko to associate a rank index with the trace data according to the instance index
	#This can be achieved by combining -rank with -override_rank <orig rank index = 0>
	echo "Starting instance ${i}"
	driver ${TAU_ADIOS2_ENGINE} ${TAU_ADIOS2_PATH} ${fn_driver} ${ad_opts} -rank ${i} -override_rank 0 2>&1 | tee chimbuko/logs/ad_${i}.log &
    fi

    cycles=5000
    anom_freq=200
    base_time=10
    anom_time=100

    TAU_ADIOS2_FILENAME=${fn_tau} ${TAU_EXEC} ./main ${i} ${cycles} ${base_time} ${anom_time} ${anom_freq} 2>&1 | tee chimbuko/logs/main_${i}.log &
    pids[$i]=$!
done

for (( i=0; i<${instances}; i++ ));
do
    echo "Waiting on " ${pids[$i]}
    wait ${pids[$i]}
done

wait
