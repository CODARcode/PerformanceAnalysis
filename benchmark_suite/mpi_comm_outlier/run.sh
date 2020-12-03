#!/bin/bash
##################### USER INPUTS #############################
ranks=3   #MPI ranks
cycles=10000   #how many loop cycles
base_MB=10   #default size of comm in MB
anom_rank=1   #which rank has the anomaly
anom_mult=50  #how much larger the data packet is for the anomaly
anom_freq=1000  #how many cycles between anomalies
######################END OF USER INPUTS #########################

export TAU_ROOT=/opt/tau2/x86_64
export TAU_MAKEFILE=$TAU_ROOT/lib/Makefile.tau-papi-mpi-pthread-pdt-adios2
export TAU_PLUGINS_PATH=$TAU_ROOT/lib/shared-papi-mpi-pthread-pdt-adios2
export TAU_PLUGINS=libTAU-adios2-trace-plugin.so

export TAU_ADIOS2_PERIODIC=1
export TAU_ADIOS2_PERIOD=1000000
export TAU_ADIOS2_ENGINE=SST
export TAU_ADIOS2_FILENAME=tau-metrics
#export TAU_VERBOSE=1
rm -rf profile.* tau-metrics* 0*

extra_args=""
ps_extra_args=""

#Instantiate the provenance database
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
    extra_args+=" -provdb_addr ${prov_add}"
    ps_extra_args+=" -provdb_addr ${prov_add}"
    echo "Enabling provenance database with arg: ${extra_args}"
fi

#For testing, this webserver just dumps what it receives from the pserver
if (( 0 )); then
    appdir=$(which pserver | sed 's/pserver//')
    ip=$(hostname -i)
    echo "Instantiating web server with print dump"
    python3 ${appdir}/ws_flask_stat.py -host ${ip} -port 5000 -print_msg  2>&1 | tee ws.log  &
    ws_pid=$!
    ws_addr=http://${ip}:5000/post
    ps_extra_args+=" -ws_addr ${ws_addr}"
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
    extra_args+=" -pserver_addr ${pserver_addr}"
    sleep 2
fi

#Instantiate the AD
if (( 1 )); then    
    echo "Instantiating AD"
    mpirun --allow-run-as-root -n ${ranks} driver SST . tau-metrics-main "" ${extra_args} 2>&1 | tee ad.log &    
    sleep 2
fi

#Run the main program
if (( 1 )); then
    echo "Running main"
    mpirun --allow-run-as-root -n ${ranks} tau_exec ${EXEC_OPTS} ${EXEC_T} ./main ${cycles} ${base_MB} ${anom_rank} ${anom_mult} ${anom_freq} 2>&1 | tee main.log
fi

#Shutdown the viz
if (( ${using_viz} == 1 )); then
    echo "Shutting down viz"
    cd /opt/chimbuko/viz
    ./webserver/shutdown_webserver.sh
fi


wait
