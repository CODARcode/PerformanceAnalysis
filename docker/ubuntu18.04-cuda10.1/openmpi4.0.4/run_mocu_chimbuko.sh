#!/bin/bash                                                                                                                                                                                                   

############################# START OF USER INPUT ########################
ntasks=2  #Requires 2 GPUs; reduce to 1 if only 1 GPU!
############################# END OF USER INPUT ########################
                                                                            
export TAU_MAKEFILE=/opt/tau2/x86_64/lib/Makefile.tau-papi-pthread-python-cupti-pdt-adios2
export TAU_ADIOS2_PERIODIC=1
export TAU_ADIOS2_PERIOD=1000000
export TAU_ADIOS2_ENGINE=SST
#export CHIMBUKO_VERBOSE=1
 
source /spack/spack/share/spack/setup-env.sh
spack load py-mochi-sonata

rm -f ./results/*

#Start the provenance database
extra_args=""
ps_extrargs=""

if (( 1 )); then
    rm provdb.*.unqlite  provider.address

    ip=eth0
    port=1234

    echo "Instantiating provenance database"
    provdb_admin ${ip}:${port} 2>&1 | tee provdb.log &

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
    if [ -z "${CHIMBUKO_VERBOSE}x" ]; then
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

#Start the parameter server
if (( 1 )); then
    ip=$(hostname -i)
    pserver_port=1235
    pserver_addr=tcp://${ip}:${pserver_port}
    pserver_nt=1
    pserver_logdir="."
    echo "Instantiating pserver"
    echo "Pserver $pserver_addr with extra args: '${ps_extraargs}'"
    pserver -nt ${pserver_nt} -logdir ${pserver_logdir} -port ${pserver_port} -stat_outputdir . ${ps_extraargs} 2>&1 | tee pserver.log  &
    extra_args="${extra_args} -pserver_addr ${pserver_addr}"
    sleep 2
fi

#Start the AD and application
rm -rf *.bp *.sst

for (( i=0; i<${ntasks}; i++ ));	  	 
do
    #Make sure each instance has a separate file to write to as this is not an MPI process and hence all the ranks will be 0
    filename_tau=tau-metrics-${i}
    filename_chimbuko=tau-metrics-${i}-python3.6

    #Overwrite the rank index in the data (which is always 0 here) with a new rank index labeling the instances
    driver ${TAU_ADIOS2_ENGINE} . ${filename_chimbuko} -prov_outputpath . ${extra_args} -perf_outputpath . -rank ${i} -override_rank 0 2>&1 | tee ad_${i}.log &
    #sstSinker ${TAU_ADIOS2_ENGINE} . ${filename_chimbuko} 0 2>&1 | tee sinker_${i}.log &
    sleep 1
    TAU_ADIOS2_FILENAME=${filename_tau} CUDA_DEVICE=$i tau_python -T serial,papi,pthread,python,cupti,pdt,adios2 -cupti -um -v -tau-python-interpreter=python3 -adios2_trace runMainForPerformanceMeasure.py -n ${ntasks} -i $i 2>&1 | tee main_${i}.log &
done

echo "Waiting for processes to end"
wait $(pidof tau_python)

#Shutdown the viz
if (( ${using_viz} == 1 )); then
    echo "Shutting down viz"
    cd /opt/chimbuko/viz
    ./webserver/shutdown_webserver.sh
fi

wait

