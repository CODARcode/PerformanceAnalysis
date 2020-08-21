#!/bin/bash
ranks=1
ps_threads=1
rm -f ps_perf.txt
pserver ${ranks} -nt ${ps_threads} 2>&1 | tee ps.log &

ip=$(hostname -i)
pserver_addr=tcp://${ip}:5559
mpirun --allow-run-as-root -n ${ranks} ./benchmark_client ${pserver_addr} -cycles 20 2>&1 | tee client.log
