#!/bin/bash

################## USER VARIABLES ###################
ranks=1  #How many MPI ranks for each workflow component
cycles=3000 #Number of loop cycles
anom_freq=300 #Frequency of anomaly
base_time=20 #Base cycle time in ms
anom_time=200 #Anomaly cycle time in ms
##################END OF USER VARIABLES #############


export TAU_ROOT=/opt/tau2/x86_64
export TAU_MAKEFILE=$TAU_ROOT/lib/Makefile.tau-papi-mpi-pthread-pdt-adios2
export TAU_PLUGINS_PATH=$TAU_ROOT/lib/shared-papi-mpi-pthread-pdt-adios2
export TAU_PLUGINS=libTAU-adios2-trace-plugin.so

export TAU_ADIOS2_PERIODIC=1
export TAU_ADIOS2_PERIOD=1000000
export TAU_ADIOS2_ENGINE=SST
export TAU_ADIOS2_FILENAME=tau-metrics
export TAU_VERBOSE=1
rm -rf profile.* tau-metrics* 1* 2*

extra_args=""
ps_extra_args=""

if (( 1 )); then
    rm -f provdb.*.unqlite  provider.address

    ip=$(hostname -i)
    port=1234

    echo "Instantiating provenance database"
    provdb_admin ${ip}:${port} &
    admin=$!

    sleep 1
    if ! [[ -f provider.address ]]; then
	echo "Provider address file not created after 1 second"
	exit 1
    fi

    prov_add=$(cat provider.address)
    extra_args+=" -provdb_addr ${prov_add}"
    ps_extra_args+=" -provdb_addr ${prov_add}"
    echo "Enabling provenance database with arg: ${extra_args}"
fi

#For testing, this webserver replaces the viz and just dumps the data it receives to stdout
if (( 0 )); then
    appdir=$(which pserver | sed 's/pserver//')

    ip=$(hostname -i)
    echo "Instantiating web server with print dump"
    python3 ${appdir}/ws_flask_stat.py -host ${ip} -port 5000 -print_msg  2>&1 | tee ws.log  &
    ws_pid=$!
    ws_addr=http://${ip}:5000/post
    sleep 1
fi

#Run the viz
using_viz=0
if (( 1 )); then
    using_viz=1
    ip=$(hostname -i)
    run_dir=$(pwd)
    echo "Run dir ${run_dir}"
    
    rm -rf viz_data
    mkdir viz_data
    viz_dir=$(readlink -f viz_data)
    provdb_dir=$(pwd)
    viz_install=/opt/chimbuko/viz
    
    export SERVER_CONFIG="production"
    if [ -z "${CHIMBUKO_VERBOSE}x" ]; then
	export SERVER_CONFIG="development"
    fi
    
    export DATABASE_URL="sqlite:///${viz_dir}/main.sqlite"
    export ANOMALY_STATS_URL="sqlite:///${viz_dir}/anomaly_stats.sqlite"
    export ANOMALY_DATA_URL="sqlite:///${viz_dir}/anomaly_data.sqlite"
    export FUNC_STATS_URL="sqlite:///${viz_dir}/func_stats.sqlite"
    export PROVENANCE_DB="${provdb_dir}/"
    export PROVDB_ADDR=$(cat provider.address)
    export SHARDED_NUM=1
    export C_FORCE_ROOT=1

    cd ${viz_install}
    
    echo "run redis ..."
    redis-stable/src/redis-server redis-stable/redis.conf 2>&1 | tee ${run_dir}/redis.log &
    sleep 5

    echo "run celery ..."
    python3 manager.py celery --loglevel=info 2>&1 | tee ${run_dir}/celery.log &
    sleep 5

    echo "create db ..."
    python3 manager.py createdb 2>&1 | tee ${run_dir}/create_db.log 
    sleep 2
    
    echo "run webserver (server config ${SERVER_CONFIG}) with provdb on ${PROVDB_ADDR}...  Logging  to ${run_dir}/viz.log"
    python3 manager.py runserver --host 0.0.0.0 --port 5002 --debug 2>&1 | tee ${run_dir}/viz.log &
    sleep 2
    
    cd -
    
    ws_addr="http://${ip}:5002/api/anomalydata"
    ps_extra_args+=" -ws_addr ${ws_addr}"    
fi

#Instantiate the pserver
if (( 1 )); then
    pserver_addr=tcp://${ip}:5559
    pserver_nt=1
    pserver_logdir="."
    echo "Instantiating pserver"
    echo "Pserver $pserver_addr"
    pserver -nt ${pserver_nt} -logdir ${pserver_logdir} ${ps_extra_args} 2>&1 | tee pserver.log  &
    extra_args="${extra_args} -pserver_addr ${pserver_addr}"
    sleep 2
fi

#Instantiate the AD
if (( 1 )); then
    echo "Instantiating AD"
    #The two components should be differentiated adding a program index
    #The program names are different (main1, main2) and thus have different tau trace files
    mpirun --allow-run-as-root -n ${ranks} driver SST . tau-metrics-main1 -prov_outputpath . -program_idx 1 ${extra_args} 2>&1 | tee ad1.log &
    mpirun --allow-run-as-root -n ${ranks} driver SST . tau-metrics-main2 -prov_outputpath . -program_idx 2 ${extra_args} 2>&1 | tee ad2.log &
    sleep 2
fi

#Run the workflow
r1=""
r2=""
if (( 1 )); then
    echo "Running main"
    mpirun --allow-run-as-root -n ${ranks} tau_exec ${EXEC_OPTS} ${EXEC_T} ./main1 ${cycles} ${base_time} ${anom_time} ${anom_freq} 2>&1 | tee main1.log &
    r1=$!
    mpirun --allow-run-as-root -n ${ranks} tau_exec ${EXEC_OPTS} ${EXEC_T} ./main2 ${cycles} ${base_time} ${anom_time} ${anom_freq} 2>&1 | tee main2.log &
    r2=$!
fi

#Wait for the workflow to finish
wait $r1
wait $r2

#Shutdown the viz
if (( ${using_viz} == 1 )); then
    echo "Shutting down viz"
    cd /opt/chimbuko/viz
    ./webserver/shutdown_webserver.sh
fi

wait
