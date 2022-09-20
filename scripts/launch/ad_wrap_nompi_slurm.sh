#!/bin/bash
set -e

ad_args=$(cat chimbuko/vars/chimbuko_ad_args.var)
ad_opts=$(cat chimbuko/vars/chimbuko_ad_opts.var)

rank=${SLURM_PROCID}
ad_opts+=" -rank ${rank}"

ad_cmd="driver ${ad_args} ${ad_opts} 2>&1 | tee chimbuko/logs/ad.${rank}.log"

echo "Starting AD rank ${rank} with command \"${ad_cmd}\""
eval "${ad_cmd} &"

echo "Starting application rank ${rank}"
echo "Application command: $@"
$@

wait
