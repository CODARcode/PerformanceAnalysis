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

ws_addr=$(cat chimbuko/vars/chimbuko_webserver.url)

#Run the simulation 
if (( 1 )); then
    echo "Running sim"
    ./example3 -remote_provdb chimbuko/provdb ${provdb_nshards} ${provdb_ninstances} -enable_viz ${ws_addr} 2>&1 | tee chimbuko/logs/sim.log
fi

wait
