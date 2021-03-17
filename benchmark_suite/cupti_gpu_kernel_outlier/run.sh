#!/bin/bash

####################### START OF USER VARIABLES ####################
cycles=1000 #Total number of loop iterations
freq=100 #How often to introduce anomalies into the loop
startcyc=20  #which cycle to introduce the first anomaly
base_cycles=10000000 #number of clock cycles in a normal kernel
anom_mult=100 #multiplier for anomalies
device_max=2 #maximum number of GPUs to run on
####################### END OF USER VARIABLES ####################

export TAU_ROOT=/opt/tau2/x86_64
export TAU_MAKEFILE=$TAU_ROOT/lib/Makefile.tau-papi-mpi-pthread-python-cupti-pdt-adios2
export TAU_PLUGINS_PATH=$TAU_ROOT/lib/shared-papi-mpi-pthread-python-cupti-pdt-adios2

export TAU_ADIOS2_PERIODIC=1
export TAU_ADIOS2_PERIOD=1000000
#export TAU_ADIOS2_ENGINE=BPFile
export TAU_ADIOS2_ENGINE=SST
export TAU_ADIOS2_FILENAME=tau-metrics
export TAU_VERBOSE=1

extra_args=""
ps_extra_args=""

#Instantiate provdb
if (( 1 )); then
    rm provdb.*.unqlite  provider.address

    ip=$(hostname -i)
    port=1234

    echo "Instantiating provenance database"
    provdb_admin ${ip}:${port} &

    sleep 1
    if ! [[ -f provider.address ]]; then
	echo "Provider address file not created after 1 second"
	exit 1
    fi

    prov_add=$(cat provider.address)
    extra_args="-provdb_addr ${prov_add}"
    ps_extra_args+=" -provdb_addr ${prov_add}"
    echo "Enabling provenance database with arg: ${extra_args}"
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
    mpirun --allow-run-as-root -n 1 driver SST . tau-metrics-main -prov_outputpath . ${extra_args} 2>&1 | tee ad.log &
    sleep 2
fi

#Run the main program
if (( 1 )); then
    echo "Running main"
    EXEC_OPTS="-cupti -um -adios2_trace"
    EXEC_T="-T papi,mpi,pthread,cupti,pdt,adios2"
    mpirun --allow-run-as-root -n 1 tau_exec ${EXEC_OPTS} ${EXEC_T} ./main ${cycles} ${freq} ${startcyc} -device_max ${device_max} -mult ${anom_mult} -base ${base_cycles} 2>&1 | tee main.log
fi

#Shutdown the viz
if (( ${using_viz} == 1 )); then
    echo "Shutting down viz"
    cd /opt/chimbuko/viz
    ./webserver/shutdown_webserver.sh
fi

wait
