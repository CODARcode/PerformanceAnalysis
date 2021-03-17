#!/bin/bash

set -e
set -o pipefail

#---------------------------------------------------------------------------
#Run Chimbuko's services
#
# This script should be executed on a dedicated node
#
# The command line arguments allowing the AD to interact with the services
# is output to chimbuko/vars/chimbuko_ad_args.var
#
# If launched with a separate launch command (jsrun, mpirun) the main script
# should wait for the above .var output to be generated before continuing
#
# Provenance database is stored in chimbuko/provdb
#
# REQUIREMENT: Environment variable CHIMBUKO_CONFIG must point to the configuration file
#
# To connect to the viz server, take note of the host and port given in
# chimbuko/vars/chimbuko_webserver.host  and chimbuko/vars/chimbuko_webserver.port
# then connect via ssh port forwarding (eg. Summit):
# ssh -t -L ${LOCAL_PORT}:${HOST}.summit.olcf.ornl.gov:${PORT} ${USERNAME}@summit.olcf.ornl.gov
# where LOCAL_PORT is arbitrary
#---------------------------------------------------------------------------

if [[ "${CHIMBUKO_CONFIG}x" == "x" ]]; then
    echo "Chimbuko Services: Error: CHIMBUKO_CONFIG must be set"
    exit 1
fi
echo "CHIMBUKO_CONFIG=${CHIMBUKO_CONFIG}"
if [[ ! -f ${CHIMBUKO_CONFIG} ]]; then
    echo "Chimbuko Services: Error: CHIMBUKO_CONFIG is not accessible"
    exit 1
fi

source ${CHIMBUKO_CONFIG}

#Create directories; assumed script is executing from work directory
#rm -rf chimbuko
if [ -d "chimbuko" ]; then
    echo "Chimbuko Services: 'chimbuko' directory already exists, please delete or move it!"
    exit 1
fi

mkdir chimbuko
mkdir chimbuko/logs
mkdir chimbuko/provdb
mkdir chimbuko/viz
mkdir chimbuko/vars      #directory for files containing variables for inter-script communication
mkdir chimbuko/adios2

#Set absolute paths
base=$(pwd)
viz_dir=${base}/chimbuko/viz
log_dir=${base}/chimbuko/logs
provdb_dir=${base}/chimbuko/provdb
var_dir=${base}/chimbuko/vars
bp_dir=${base}/chimbuko/adios2

#Check tau adios2 path is writable; by default this is ${bp_dir} but it can be overridden by users, eg for offline analysis
touch ${TAU_ADIOS2_PATH}/write_check
if [[ $? == 1 ]]; then
    echo "Chimbuko Services: Could not write to ADIOS2 output path ${TAU_ADIOS2_PATH}, check permissions"
    exit 1
fi
rm -f ${TAU_ADIOS2_PATH}/write_check

#Get head node IP
if command -v ip &> /dev/null
then
    ip=$(ip -4 addr show ${service_node_iface} | grep -oP '(?<=inet\s)\d+(\.\d+){3}')
elif command -v iconfig &> /dev/null
then
    ip=$(ifconfig 2>&1 ${service_node_iface} | grep -E -o 'inet [0-9.]+' | awk '{print $2}')
else
    echo "Chimbuko services: Neither ifconfig or ip commands exists; cannot infer ip for interface ${service_node_iface}"
    exit 1
fi

echo "Chimbuko Services: Launching Chimbuko services on interface ${service_node_iface} with host IP" ${ip}


####################################
#Run the Chimbuko head node services
####################################

#Provenance database
extra_args=${ad_extra_args}
ps_extra_args=${pserver_extra_args}
if (( 1 )); then
    echo "==========================================="
    echo "Instantiating provenance database"
    echo "==========================================="
    cd ${provdb_dir}
    rm -f provdb.*.unqlite*  provider.address

    provdb_addr="${service_node_iface}:${provdb_port}"    #can be IP:PORT or ADAPTOR:PORT per libfabric conventions
    if [[ ${provdb_engine} == "verbs" ]]; then
	provdb_addr="${provdb_domain}/${provdb_addr}"
    fi

    provdb_admin "${provdb_addr}" ${provdb_extra_args} -engine ${provdb_engine} -nshards ${provdb_nshards} -nthreads ${provdb_nthreads} 2>&1 | tee ${log_dir}/provdb.log &
    provdb_pid=$!

    start_time=$SECONDS
    while [ ! -f provider.address ]; do
	now=$SECONDS
	elapsed=$(( now - start_time ))
	if [[ ${elapsed} -gt 30 ]]; then
	    echo "Chimbuko Services: ERROR: Provider address file not created after ${elapsed} seconds"
	    exit 1
	fi
	sleep 1;
    done

    prov_add=$(cat provider.address)
    extra_args+=" -provdb_addr \"${prov_add}\" -nprovdb_shards ${provdb_nshards}"
    ps_extra_args+=" -provdb_addr \"${prov_add}\""
    echo "Chimbuko Services: Enabling provenance database with arg: ${extra_args}"
    cd -
fi

#Visualization
if (( ${use_viz} )); then
    cd ${viz_dir}
    HOST=`hostname`
    export SHARDED_NUM=${provdb_nshards}
    export PROVDB_ADDR=${prov_add}

    export SERVER_CONFIG="production"
    export DATABASE_URL="sqlite:///${viz_dir}/main.sqlite"
    export ANOMALY_STATS_URL="sqlite:///${viz_dir}/anomaly_stats.sqlite"
    export ANOMALY_DATA_URL="sqlite:///${viz_dir}/anomaly_data.sqlite"
    export FUNC_STATS_URL="sqlite:///${viz_dir}/func_stats.sqlite"
    export PROVENANCE_DB=${provdb_dir}
    export CELERY_BROKER_URL="redis://${HOST}:${viz_worker_port}"

    #Setup redis
    cp -r $viz_root/redis-stable/redis.conf .
    sed -i "s|^dir ./|dir ${viz_dir}/|" redis.conf
    sed -i "s|^bind 127.0.0.1|bind 0.0.0.0|" redis.conf
    sed -i "s|^daemonize no|daemonize yes|" redis.conf
    sed -i "s|^pidfile /var/run/redis_6379.pid|pidfile ${viz_dir}/redis.pid|" redis.conf
    sed -i "s|^logfile "\"\""|logfile ${log_dir}/redis.log|" redis.conf
    sed -i "s|.*syslog-enabled no|syslog-enabled yes|" redis.conf

    echo "==========================================="
    echo "Chimbuko Services: Launch Chimbuko visualization server"
    echo "==========================================="
    cd ${viz_root}

    echo "Chimbuko Services: create db ..."
    python3 manager.py createdb

    echo "Chimbuko Services: run redis ..."
    redis-server ${viz_dir}/redis.conf
    sleep 5

    echo "Chimbuko Services: run celery ..."
    CELERY_ARGS="--loglevel=info --concurrency=1"
    python3 manager.py celery ${CELERY_ARGS} 2>&1 | tee "${log_dir}/celery.log" &
    sleep 10

    echo "Chimbuko Services: run webserver ..."
    python3 run_server.py $HOST $viz_port 2>&1 | tee "${log_dir}/webserver.log" &
    sleep 2

    echo "Chimbuko Services: redis ping-pong ..."
    redis-cli -h $HOST -p ${viz_worker_port} ping

    cd ${base}

    ws_addr="http://${HOST}:${viz_port}/api/anomalydata"
    ps_extra_args+=" -ws_addr ${ws_addr}"

    echo $HOST > ${var_dir}/chimbuko_webserver.host
    echo $viz_port > ${var_dir}/chimbuko_webserver.port
    echo "Chimbuko Services: Webserver is running on ${HOST}:${viz_port} and is ready for the user to connect"
fi

if (( 1 )); then
    echo "==========================================="
    echo "Chimbuko Services: Instantiating pserver"
    echo "==========================================="
    echo "Chimbuko Services: Pserver $pserver_addr"

    pserver_addr="tcp://${ip}:${pserver_port}"  #address for parameter server in format "tcp://IP:PORT"
    pserver -nt ${pserver_nt} -logdir ${log_dir} -port ${pserver_port} ${ps_extra_args} 2>&1 | tee ${log_dir}/pserver.log  &

    ps_pid=$!
    extra_args+=" -pserver_addr ${pserver_addr}"
    sleep 2
fi

#echo "Chimbuko Services: Processes are: " $(ps)


############################################
#Generate the command to launch the AD module
ad_opts="${extra_args} -err_outputpath ${log_dir} -outlier_sigma ${ad_outlier_sigma} -anom_win_size ${ad_win_size}"
ad_cmd="driver ${TAU_ADIOS2_ENGINE} ${TAU_ADIOS2_PATH} ${TAU_ADIOS2_FILE_PREFIX}-${EXE_NAME} ${ad_opts} 2>&1 | tee ${log_dir}/ad.log"
echo ${ad_cmd} > ${var_dir}/chimbuko_ad_cmdline.var

echo "Command to launch AD module: " ${ad_cmd}
echo "Please execute as:"
echo "    ad_cmd=\$(cat ${var_dir}/chimbuko_ad_cmdline.var)"
echo "    eval \"mpirun -n \${TASKS} \${ad_cmd} &\""
echo "Or equivalent"

#For use in other scripts, output the AD cmdline options to file
echo ${ad_opts} > ${var_dir}/chimbuko_ad_opts.var



#Wait for pserver to exit, which means it's time to end the viz
echo "Chimbuko Services: waiting for pserver (pid ${ps_pid}) to terminate"
wait ${ps_pid}

if (( ${use_viz} == 1 )); then
    echo "Chimbuko Services: Terminating Chimbuko visualization"
    cd ${viz_root}

    echo "Chimbuko Services: redis ping-pong"
    redis-cli -h $HOST -p ${viz_worker_port} ping

    sleep 10

    echo "Chimbuko Services: celery task inspect"
    curl -X GET "http://${HOST}:${viz_port}/tasks/inspect"
    sleep 5

    echo "Chimbuko Services: shutdown webserver & celery workers!"
    curl -X GET "http://${HOST}:${viz_port}/stop"
    sleep 10

    echo "Chimbuko Services: redis ping-pong"
    redis-cli -h $HOST -p ${viz_worker_port} ping
    sleep 1

    echo "Chimbuko Services: shutdown redis server!"
    redis-cli -h $HOST -p ${viz_worker_port} shutdown

    cd -
fi

echo "Chimbuko Services: Service script complete" $(date)
