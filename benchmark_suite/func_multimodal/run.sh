#!/bin/bash
set -e

############## USER VARIABLES ####################
ranks=4   #number of MPI ranks
cycles=200 #number of loops in main program
anom_freq=30 #how many loop cycles beween anomalies
modetimes="200,400"  #function execution times in ms for "normal executions" (each cycle randomly chosen over modes)
anom_time=10000  #function execution time in ms for anomalous execution
############## END OF USER VARIABLES #################

rm -rf chimbuko
export CHIMBUKO_CONFIG=chimbuko_config.sh
source ${CHIMBUKO_CONFIG}

if (( 1 )); then
    echo "Running services"
    ${backend_root}/scripts/launch/run_services.sh 2>&1 | tee services.log &
    echo "Waiting"
    while [ ! -f chimbuko/vars/chimbuko_ad_cmd.var ]; do sleep 1; done
    ad_cmd=$(cat chimbuko/vars/chimbuko_ad_cmd.var)
fi

if (( 1 )); then
    echo "Instantiating AD"
    eval "${ad_cmd} &"
    sleep 2
fi

#Run the main program
if (( 1 )); then
    echo "Running main"
    mpirun --allow-run-as-root -n ${ranks} ${TAU_EXEC} ./main ${cycles} ${modetimes} ${anom_time} ${anom_freq} 2>&1 | tee main.log
fi

wait
