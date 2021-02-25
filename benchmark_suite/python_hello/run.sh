#!/bin/bash

export TAU_ADIOS2_PERIODIC=1
export TAU_ADIOS2_PERIOD=1000000
#export TAU_ADIOS2_ENGINE=BPFile
export TAU_ADIOS2_ENGINE=SST
export TAU_ADIOS2_FILENAME=tau-metrics

rm -rf profile.* tau-metrics* 0*

extra_args=""
ps_extra_args=""

#Run the provenance database
if (( 1 )); then
    rm provdb.*.unqlite  provider.address

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
    extra_args="-provdb_addr ${prov_add}"
    ps_extra_args="-provdb_addr ${prov_add}"
    echo "Enabling provenance database with arg: ${extra_args}"
    sleep 2
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

#Run the pserver
if (( 1 )); then
    rm pserver*.json
    ip=$(hostname -i)
    pserver_port=9999
    pserver_addr=tcp://${ip}:${pserver_port}
    pserver_nt=1
    pserver_logdir="."
    echo "Instantiating pserver"
    echo "Pserver $pserver_addr with extra args: '${ps_extra_args}'"
    pserver -nt ${pserver_nt} -stat_outputdir ${pserver_logdir}  -logdir ${pserver_logdir} -port ${pserver_port} ${ps_extra_args} 2>&1 | tee pserver.log  &
    extra_args="${extra_args} -pserver_addr ${pserver_addr}"
    sleep 2
fi

#Run the AD
if (( 1 )); then
    echo "Instantiating AD"
    rm *.sst
    #Use inclusive runtime so that the anomaly is assigned to the function and not to the stdlib sleep function it calls
    mpirun --allow-run-as-root -n 1 driver SST . tau-metrics-python3.6 -prov_outputpath . ${extra_args} -outlier_statistic "inclusive_runtime" 2>&1 | tee ad.log &
    sleep 2
fi

#Run the main program
if (( 1 )); then
    echo "Running main"
    #Note that passing -u to python forces it to not buffer stdout so we can pipe it to tee in realtime
    tau_python -tau-python-interpreter=python3 -adios2_trace -tau-python-args=-u test.py 2>&1 | tee run.log
fi

#Shutdown the viz
if (( ${using_viz} == 1 )); then
    echo "Shutting down viz"
    cd /opt/chimbuko/viz
    ./webserver/shutdown_webserver.sh
fi

wait
