#!/bin/bash
### Begin BSUB Options
#BSUB -P CSC299
#BSUB -J provdb-benchmark
#BSUB -W 00:30
#BSUB -nnodes 97
#BSUB -N
#BSUB -alloc_flags "smt4 pmcd"
#BSUB -alloc_flags "nvme"
### End BSUB Options and begin shell commands

ulimit -c unlimited
set -e
set -o pipefail

BASE=$(pwd)

n_nodes_total=97 #First will be used for services
n_nodes=$(( n_nodes_total - 1 ))

n_mpi_ranks_per_node=42
ncores_per_host_ad=42

#n_mpi_ranks_per_node=20
#ncores_per_host_ad=20


export CHIMBUKO_CONFIG=${BASE}/chimbuko_config.sh
source /autofs/nccs-svm1_home1/ckelly/setup_chimbuko_noAD.sh
source ${CHIMBUKO_CONFIG}

SERVICES=${chimbuko_services}

echo "Job starting"
date
echo "Driver is : " $(which driver)

WORKDIR=/gpfs/alpine/csc299/scratch/ckelly/tmp_benchmark_provdb_run.${LSB_JOBID}
rm -rf $WORKDIR
mkdir -p $WORKDIR
cd $WORKDIR

${BASE}/gen_urs.pl ${n_nodes_total} ${n_mpi_ranks_per_node} 1 0 ${provdb_ninstances};

cp ${BASE}/run_services*.sh .
cp ${BASE}/path_check.sh .

#Generate ERF file for head node and AD/main
#rm -f services.erf ad.erf main.erf
#${BASE}/gen_erf.pl ${n_nodes_total} ${n_mpi_ranks_per_node} 0 0 ${ncores_per_host_ad};

if (( 1 )); then
    echo "Launching services"

    #Run the services
    rm -f chimbuko/vars/chimbuko_ad_cmd.var
    ./run_services.sh &
    
    #Wait for the services to start and generate their outputs
    while [ ! -f chimbuko/vars/chimbuko_ad_cmdline.var ]; do sleep 1; done
fi

provdb_addr_dir=chimbuko/provdb
exe=/autofs/nccs-svm1_home1/ckelly/bld/AD/benchmark_suite/benchmark_provdb/benchmark_client
cycles=500
callstack_size=2
ncounters=5
winsize=5
comm_messages_per_winevent=1
anomalies_per_cycle=1
normal_events_per_cycle=1
cycle_time_ms=1000
nshards=${provdb_nshards}
ninstances=${provdb_ninstances}
perf_write_freq=5   #cycles
perf_dir=chimbuko/logs
do_state_dump=0
# -max_outstanding_sends 50

client_cmd="$exe ${provdb_addr_dir} -cycles ${cycles} -callstack_size ${callstack_size} -ncounters ${ncounters} -winsize ${winsize} -comm_messages_per_winevent ${comm_messages_per_winevent} -anomalies_per_cycle ${anomalies_per_cycle} -normal_events_per_cycle ${normal_events_per_cycle} -cycle_time_ms ${cycle_time_ms} -nshards ${nshards} -ninstances ${ninstances} -perf_write_freq ${perf_write_freq} -perf_dir ${perf_dir} -do_state_dump ${do_state_dump} 2>&1 | tee chimbuko/logs/client.log"

if (( 1 )); then
    echo "Instantiating client"
    echo "Command is " $client_cmd
    #eval "jsrun --erf_input=ad.erf -e prepended ${client_cmd} &"
    eval "jsrun -U main.urs -e prepended ${client_cmd} &"
fi

echo "Waiting for job completion"
jswait all

echo "Copying back chimbuko dir (minus the actual provdb)"
rm -rf chimbuko/provdb/*.unqlite
outputdir=${BASE}/chimbuko.${LSB_JOBID}
mv chimbuko ${outputdir}
cp ${BASE}/run_main.sh ${BASE}/chimbuko_config.sh ${outputdir}

echo "Done"
date
