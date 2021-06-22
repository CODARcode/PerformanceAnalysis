#!/bin/bash

set -e
ulimit -s unlimited

##################### USER INPUTS #############################
ranks=4   #MPI ranks
base_MB=10   #default size of comm in MB
anom_mult=50  #how much larger the data packet is for the anomaly
anom_chance=0.05  #how many cycles between anomalies
calls=1000
######################END OF USER INPUTS #########################

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
    eval "mpirun --allow-run-as-root -n ${ranks} ${ad_cmd} &"
    sleep 2
fi

#Run the main program
if (( 1 )); then
    echo "Running main"
    mpirun --allow-run-as-root -n ${ranks} ${TAU_EXEC} ./main ${base_MB} ${anom_mult} ${anom_chance} ${calls} 2>&1 | tee main.log
fi

wait

