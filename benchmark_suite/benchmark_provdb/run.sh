#!/bin/bash
ranks=4
cycles=60
cycle_time_ms=1000
#rm -f ps_perf.txt ps_perf_stats.txt

rm -f provdb.unqlite provider.address
ip=$(hostname -i)
provdb_addr=${ip}:5000

provdb_admin ${provdb_addr} -autoshutdown 2>&1 | tee provdb.log &

sleep 2
provdb_inaddr=`cat provider.address`
echo "Provdb addr ${provdb_inaddr}"


mpirun --oversubscribe --allow-run-as-root -n ${ranks} ./benchmark_client ${provdb_inaddr} -cycles ${cycles} -cycle_time_ms ${cycle_time_ms} 2>&1 | tee client.log

dir=${ranks}rank_cyc${cycle_time_ms}ms
mkdir ${dir}
mv client_stats.json ${dir}
