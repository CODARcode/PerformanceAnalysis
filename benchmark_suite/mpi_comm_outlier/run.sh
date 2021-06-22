#!/bin/bash
##################### USER INPUTS #############################
ranks=3   #MPI ranks
cycles=10000   #how many loop cycles
base_MB=10   #default size of comm in MB
anom_rank=1   #which rank has the anomaly
anom_mult=50  #how much larger the data packet is for the anomaly
anom_freq=1000  #how many cycles between anomalies
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
    mpirun --allow-run-as-root -n ${ranks} ${TAU_EXEC} ./main ${cycles} ${base_MB} ${anom_rank} ${anom_mult} ${anom_freq} 2>&1 | tee main.log
fi


wait
