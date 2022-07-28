#!/bin/bash
set -e

################## USER VARIABLES ###################
ranks=1  #How many MPI ranks for each workflow component
cycles=3000 #Number of loop cycles
anom_freq=300 #Frequency of anomaly
base_time=20 #Base cycle time in ms
anom_time=200 #Anomaly cycle time in ms
##################END OF USER VARIABLES #############

rm -rf chimbuko
export CHIMBUKO_CONFIG=chimbuko_config.sh
source ${CHIMBUKO_CONFIG}

if (( 1 )); then
    echo "Running services"
    ${chimbuko_services} 2>&1 | tee services.log &
    echo "Waiting"
    while [ ! -f chimbuko/vars/chimbuko_ad_opts.var ]; do sleep 1; done
fi

#Instantiate the AD
if (( 1 )); then
    echo "Instantiating AD"
    for i in ${!EXE_NAME[@]}; do
	exe=${EXE_NAME[$i]}
	ad_cmd=$(cat chimbuko/vars/chimbuko_ad_cmdline.${exe}.var)
	run_cmd="mpirun --allow-run-as-root -n ${ranks} ${ad_cmd} &"
	echo "Executing: $run_cmd"
	eval "$run_cmd"
    done	
    sleep 2
fi

#Run the workflow
r1=""
r2=""
if (( 1 )); then
    echo "Running main"
    mpirun --allow-run-as-root -n ${ranks} ${TAU_EXEC} ./main1 ${cycles} ${base_time} ${anom_time} ${anom_freq} 2>&1 | tee chimbuko/logs/main1.log &
    r1=$!
    mpirun --allow-run-as-root -n ${ranks} ${TAU_EXEC} ./main2 ${cycles} ${base_time} ${anom_time} ${anom_freq} 2>&1 | tee chimbuko/logs/main2.log &
    r2=$!
fi

#Wait for the workflow to finish
wait $r1
wait $r2

wait
