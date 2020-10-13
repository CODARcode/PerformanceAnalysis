#!/bin/bash
ranks=64
cycles=60
cycle_time_ms=1000
anom_per_cyc=10
norm_per_cyc=10
shards=2
threads=2
#rm -f ps_perf.txt ps_perf_stats.txt

export CHIMBUKO_VERBOSE=1
export MARGO_ENABLE_PROFILING=1

rm -f provdb.*.unqlite* provider.address
ip=$(hostname -i)
provdb_addr=${ip}:5000

# -engine "na+sm"
#-db_type "null"
provdb_admin ${provdb_addr} -autoshutdown true -nshards ${shards} -nthreads ${threads} -db_commit_freq 10000 2>&1 | tee provdb.log &
admin=$!

sleep 5
provdb_inaddr=`cat provider.address`
echo "Provdb addr ${provdb_inaddr}"


mpirun --oversubscribe --allow-run-as-root -n ${ranks} ./benchmark_client ${provdb_inaddr} -cycles ${cycles} -cycle_time_ms ${cycle_time_ms} -anomalies_per_cycle ${anom_per_cyc} -normal_events_per_cycle ${norm_per_cyc} -nshards ${shards} 2>&1 | tee client.log

wait $admin

dir=${ranks}rank_cyc${cycle_time_ms}ms_anom${anom_per_cyc}_norm${norm_per_cyc}_shards${shards}_threads${threads}_multithread_prdcommit
mkdir ${dir}
mv client_stats.json profile* ${dir}
echo "ADMIN is $admin"
