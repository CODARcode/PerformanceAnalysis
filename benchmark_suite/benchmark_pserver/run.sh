#!/bin/bash
ranks=32
ps_threads=4
cycles=100
nfuncs=100
ncounters=100
nanomalies_per_func=2
cycle_time_ms=32
#rm -f ps_perf.txt ps_perf_stats.txt
pserver ${ranks} -nt ${ps_threads} 2>&1 | tee ps.log &

ip=$(hostname -i)
pserver_addr=tcp://${ip}:5559
mpirun --oversubscribe --allow-run-as-root -n ${ranks} ./benchmark_client ${pserver_addr} -cycles ${cycles} -nfuncs ${nfuncs} -ncounters ${ncounters} -nanomalies_per_func ${nanomalies_per_func} -cycle_time_ms ${cycle_time_ms} 2>&1 | tee client.log

dir=${ranks}rank_cyc${cycle_time_ms}ms_${ps_threads}psthr_stats
mkdir ${dir}
mv client_stats.json ps_perf* ${dir}
