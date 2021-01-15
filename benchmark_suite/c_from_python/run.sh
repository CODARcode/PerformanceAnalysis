#!/bin/bash

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
    ${TAU_PYTHON} test.py 2>&1 | tee run.log
fi

wait
