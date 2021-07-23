#!/bin/bash

################# START OF USER INPUT ##############
cycles=100 #Number of loop samples
freq=5 #Cycle frequency of anomalies
startcyc=20 #Cycle to introduce first anomaly
device=0 #Which GPU to use
anom_stream=0 #Which stream to put the anomalies on
streams=3 #Number of streams
base_cycles=10000000 #Number of clock cycles in normal kernel
anom_mult=100 #Mult factor for anomalies
################# END OF USER INPUT ##############

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
    mpirun --allow-run-as-root -n 1 ${TAU_EXEC} ./main ${cycles} ${freq} ${startcyc} -device ${device} -stream ${anom_stream} -streams ${streams} -mult ${anom_mult} -base ${base_cycles} 2>&1 | tee main.log  
fi

wait
