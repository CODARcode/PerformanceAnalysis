#!/bin/bash

set -e

####################### START OF USER VARIABLES ####################
cycles=1000 #Total number of loop iterations
freq=100 #How often to introduce anomalies into the loop
startcyc=20  #which cycle to introduce the first anomaly
base_cycles=10000000 #number of clock cycles in a normal kernel
anom_mult=100 #multiplier for anomalies
device_max=2 #maximum number of GPUs to run on
####################### END OF USER VARIABLES ####################

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

if (( 1 )); then
    echo "Instantiating AD"
    eval "mpirun --allow-run-as-root -n 1 ${ad_cmd} &"
    sleep 2
fi

#Run the main program
if (( 1 )); then
    echo "Running main"
    mpirun --allow-run-as-root -n 1 ${TAU_EXEC} ./main ${cycles} ${freq} ${startcyc} -device_max ${device_max} -mult ${anom_mult} -base ${base_cycles} 2>&1 | tee chimbuko/logs/main.log
fi

wait
