#!/bin/bash
ranks=24
ps_threads=1
cycles=68
nfuncs=100
ncounters=100
nanomalies_per_func=2
cycle_time_ms=300
rm -f ps_perf.txt ps_perf_stats.txt
pserver ${ranks} -nt ${ps_threads} 2>&1 | tee ps.log &

ip=$(hostname -i)
pserver_addr=tcp://${ip}:5559
mpirun --oversubscribe --allow-run-as-root -n ${ranks} ./benchmark_client ${pserver_addr} -cycles ${cycles} -nfuncs ${nfuncs} -ncounters ${ncounters} -nanomalies_per_func ${nanomalies_per_func} -cycle_time_ms ${cycle_time_ms} 2>&1 | tee client.log
