#!/bin/bash
############## USER VARIABLES ####################
ranks=4   #number of MPI ranks
cycles=200 #number of loops in main program
anom_freq=30 #how many loop cycles beween anomalies
modetimes="200,400"  #function execution times in ms for "normal executions" (each cycle randomly chosen over modes)
anom_time=10000  #function execution time in ms for anomalous execution
############## END OF USER VARIABLES #################

export TAU_ROOT=/opt/tau2/x86_64
export TAU_MAKEFILE=$TAU_ROOT/lib/Makefile.tau-papi-mpi-pthread-pdt-adios2
export TAU_PLUGINS_PATH=$TAU_ROOT/lib/shared-papi-mpi-pthread-pdt-adios2
export TAU_PLUGINS=libTAU-adios2-trace-plugin.so

export TAU_ADIOS2_PERIODIC=1
export TAU_ADIOS2_PERIOD=1000000
export TAU_ADIOS2_ENGINE=SST
export TAU_ADIOS2_FILENAME=tau-metrics
#export TAU_VERBOSE=1
#export CHIMBUKO_VERBOSE=1

admin=-1
extra_args=""
ps_extrargs=""

#Run the provenance database
if (( 1 )); then
    rm provdb.*.unqlite  provider.address

    ip=eth0
    port=1234

    echo "Instantiating provenance database"
    provdb_admin ${ip}:${port} 2>&1 | tee provdb.log &
    admin=$!

    sleep 1
    if ! [[ -f provider.address ]]; then
	echo "Provider address file not created after 1 second"
	exit 1
    fi

    prov_add=$(cat provider.address)
    extra_args="-provdb_addr ${prov_add}"
    ps_extraargs="-provdb_addr ${prov_add}"
    echo "Enabling provenance database in AD/PS with arg: ${extra_args}"
fi

#(For testing, this webserver just dumps the data it receives from the pserver)
appdir=$(which pserver | sed 's/pserver//')
if (( 0 )); then
    ip=$(hostname -i)
    echo "Instantiating web server with print dump"
    python3 ${appdir}/ws_flask_stat.py -host ${ip} -port 5000 -print_msg  2>&1 | tee ws.log  &
    ws_pid=$!
    ws_addr=http://${ip}:5000/post
    ps_extraargs+=-ws_addr ${ws_addr}
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
    PWD=$(pwd)
    viz_install=/opt/chimbuko/viz
    
    export SERVER_CONFIG="production"
    if (( $CHIMBUKO_VERBOSE == 1 )); then
	export SERVER_CONFIG="development"
    fi
    
    export DATABASE_URL="sqlite:///${viz_dir}/main.sqlite"
    export ANOMALY_STATS_URL="sqlite:///${viz_dir}/anomaly_stats.sqlite"
    export ANOMALY_DATA_URL="sqlite:///${viz_dir}/anomaly_data.sqlite"
    export FUNC_STATS_URL="sqlite:///${viz_dir}/func_stats.sqlite"
    export PROVENANCE_DB="${PWD}/"
    export PROVDB_ADDR=$(cat provider.address)
    export SHARDED_NUM=1
    export C_FORCE_ROOT=1
    #export SIMULATION_JSON="${WORK_DIR}/${DATA_NAME}/stats/"

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
    ps_extraargs+=" -ws_addr ${ws_addr}"    
fi

#Run the pserver
if (( 1 )); then
    rm pserver*.json
    ip=$(hostname -i)
    pserver_port=9999
    pserver_addr=tcp://${ip}:${pserver_port}
    pserver_nt=1
    pserver_logdir="."
    echo "Instantiating pserver"
    echo "Pserver $pserver_addr with extra args: '${ps_extraargs}'"
    pserver -nt ${pserver_nt} -stat_outputdir ${pserver_logdir}  -logdir ${pserver_logdir} -port ${pserver_port} ${ps_extraargs} 2>&1 | tee pserver.log  &
    extra_args="${extra_args} -pserver_addr ${pserver_addr}"
    sleep 2
fi

#Run the AD
if (( 1 )); then
    rm -rf 0 *.sst
    echo "Instantiating AD"
    mpirun --allow-run-as-root -n ${ranks} driver ${TAU_ADIOS2_ENGINE} . tau-metrics-main . ${extra_args} -perf_outputpath . -parser_beginstep_timeout 30 -trace_connect_timeout 30 -perf_step 1 2>&1 | tee ad.log &
    ad=$!
    sleep 2
fi

#Run the main program
if (( 1 )); then
    rm -rf profile.* tau-metrics*
    echo "Running main"
    mpirun --allow-run-as-root -n ${ranks} tau_exec ${EXEC_OPTS} ${EXEC_T} ./main ${cycles} ${modetimes} ${anom_time} ${anom_freq} 2>&1 | tee main.log
fi

#Shutdown the viz
if (( using_viz == 1 )); then
    echo "Shutting down viz"
    cd /opt/chimbuko/viz
    ./webserver/shutdown_webserver.sh
fi
