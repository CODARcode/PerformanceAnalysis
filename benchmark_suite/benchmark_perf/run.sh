#!/bin/bash
set -e

rank=1
client_cmd=./benchmark_perf

if (( 1 )); then
    echo "Instantiating client"
    echo "Command is " $client_cmd
    eval "mpirun --allow-run-as-root -n ${ranks} ${client_cmd} &"
fi

wait
