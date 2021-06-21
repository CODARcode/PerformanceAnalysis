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

if (( 1 )); then
    #Use inclusive runtime so that the anomaly is assigned to the function and not to the stdlib sleep function it calls
    echo "Instantiating AD"
    eval "mpirun --allow-run-as-root -n 1 ${ad_cmd} &"
    sleep 2
fi

#Run the main program
if (( 1 )); then
    echo "Running main"
    ${TAU_PYTHON} test.py 2>&1 | tee chimbuko/logs/main.log
fi

wait
