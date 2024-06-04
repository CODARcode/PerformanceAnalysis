#!/bin/bash

set -e
set -u
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
mkdir chimbuko/provdb    #directory in which provDB is run. Also the default directory for the database shards
mkdir chimbuko/viz
mkdir chimbuko/vars      #directory for files containing variables for inter-script communication
mkdir chimbuko/adios2
mkdir chimbuko/pserver   #directory for storing data produced by the pserver

#Set absolute paths
base=$(pwd)
viz_dir=${base}/chimbuko/viz
log_dir=${base}/chimbuko/logs
provdb_dir=${base}/chimbuko/provdb
var_dir=${base}/chimbuko/vars
bp_dir=${base}/chimbuko/adios2
ps_dir=${base}/chimbuko/pserver

#Check tau adios2 path is writable; by default this is ${bp_dir} but it can be overridden by users, eg for offline analysis
#touch ${TAU_ADIOS2_PATH}/write_check
#if [[ $? == 1 ]]; then
#    echo "Chimbuko Services: Could not write to ADIOS2 output path ${TAU_ADIOS2_PATH}, check permissions"
#    exit 1
#fi
#rm -f ${TAU_ADIOS2_PATH}/write_check

####################################
#Run the Chimbuko head node services
####################################

echo "==========================================="
echo "Starting Chimbuko services with module: ${module}"
echo "==========================================="


#Provenance database
extra_args=${ad_extra_args}
ps_extra_args=${pserver_extra_args}
if (( ${use_provdb} == 1 )); then
    echo "==========================================="
    echo "Instantiating provenance database"
    echo "==========================================="

    #If provdb_writedir is a relative path it should be relative to the run directory
    #Turn it into an absolute path before entering the subdirectory
    provdb_writedir=$(readlink -f ${provdb_writedir})

    #Provdb will write out address information into the directory in which it is started
    provdb_addr_dir=$(readlink -f ${provdb_dir})

    cd ${provdb_dir}
    rm -f ${provdb_writedir}/provdb.*.unqlite*  provider.address*

    #Enable better error reporting from Mercury
    if [[ -z "${HG_LOG_SUBSYS:-}" ]]; then
	export HG_LOG_SUBSYS=hg
    fi
    if [[ -z "${HG_LOG_LEVEL:-}" ]]; then
	export HG_LOG_LEVEL=error
    fi

    #provdb_interface has several options:
    #auto  :   let Mercury automatically choose an interface
    #<iface> : a single interface used for all instances
    #<iface1>:<iface2>:<iface3> ....  :  a colon-separated list of interfaces, one per instance

    IFS=':' read -a iface_in_lst <<< "$provdb_interface"
    iface_in_lst_len=${#iface_in_lst[@]}
    ifaces=()
    auto_iface=0
    if [[ ${iface_in_lst_len} -eq 1 ]]; then
	if [[ "${iface_in_lst[0]}" == "auto" ]]; then
	    auto_iface=1
	    for ((i=0;i<provdb_ninstances;i++)); do
		ifaces+=("")
	    done
	else
	    for ((i=0;i<provdb_ninstances;i++)); do
		ifaces+=("${iface_in_lst[0]}")
	    done   
	fi	
    elif [[ ${iface_in_lst_len} -eq ${provdb_ninstances} ]]; then
	for ((i=0;i<provdb_ninstances;i++)); do
	    ifaces+=("${iface_in_lst[i]}")
	done   
    else
	echo "Chimbuko Services: ERROR: provdb_interface colon-separated list '${provdb_interface}' should have either 1 entry or provdb_ninstances=${provdb_ninstances} entries"
	exit 1
    fi

    #provdb_numa_bind : specify NUMA domain binding for the provdb instances (requires numactl)
    # This variable has several options:
    # <blank>  - if left blank, no binding will be performed
    # <index>  - a single NUMA domain for all instances
    # <idx1>:<idx2>:<idx3> ...  - a colon-separated list of NUMA domains, one per instance
    do_numa_bind=0
    numas=()
    if [[ ! -z "${provdb_numa_bind:-}" ]]; then    
	IFS=':' read -a numa_in_lst <<< "$provdb_numa_bind"
	numa_in_lst_len=${#numa_in_lst[@]}
	if [[ ${numa_in_lst_len} -eq 1 ]]; then
	    for ((i=0;i<provdb_ninstances;i++)); do
		numas+=("${numa_in_lst[0]}")
	    done          
	elif [[ ${numa_in_lst_len} -eq ${provdb_ninstances} ]]; then
	    for ((i=0;i<provdb_ninstances;i++)); do
		numas+=("${numa_in_lst[i]}")
	    done	
	else
	    echo "Chimbuko Services: ERROR: provdb_numa_bind colon-separated list '${provdb_numa_bind}' should have either 1 entry or provdb_ninstances=${provdb_ninstances} entries"
	    exit 1
	fi
	do_numa_bind=1
    fi
    
    for((i=0;i<provdb_ninstances;i++)); do
	port=$((provdb_port+i))
	iface=${ifaces[i]}
	provdb_addr="${iface}:${port}"    #for conventions, cf https://mercury-hpc.github.io/user/na/)
	if [[ ${provdb_engine} == "verbs" && ${provdb_domain} != "" ]]; then
	    provdb_addr="${provdb_domain}/${provdb_addr}"
	fi
	echo "Chimbuko services launching provDB instance ${i} of ${provdb_ninstances} on address '${provdb_addr}' and engine '${provdb_engine}'"
	cmd="provdb_admin ${module} "${provdb_addr}" ${provdb_extra_args} -engine '${provdb_engine}' -nshards ${provdb_nshards} -db_write_dir ${provdb_writedir} -db_commit_freq 0 -server_instance ${i} ${provdb_ninstances} 2>&1 | tee ${log_dir}/provdb_${i}.log &"
	if [[ ${do_numa_bind} -eq 1 ]]; then
	    numa=${numas[i]}
	    echo "Chimbuko services binding provDB instance ${i} to NUMA domain ${numa}"
	    cmd="numactl -N ${numa} ${cmd}"
	fi
	echo "${cmd}"
	eval ${cmd}
	sleep 1
    done

    #Wait for provdb to be ready
    for((i=0;i<provdb_ninstances;i++)); do
	start_time=$SECONDS
	while [ ! -f provider.address.${i} ]; do
	    now=$SECONDS
	    elapsed=$(( now - start_time ))
	    if [[ ${elapsed} -gt 30 ]]; then
		echo "Chimbuko Services: ERROR: Provider address file not created after ${elapsed} seconds for instance ${i}"
		exit 1
	    fi
	    sleep 1;
	done
    done

    extra_args+=" -provdb_addr_dir ${provdb_addr_dir} -nprovdb_instances ${provdb_ninstances} -nprovdb_shards ${provdb_nshards}"
    ps_extra_args+=" -provdb_addr_dir ${provdb_addr_dir}"
    echo "Chimbuko Services: Enabling provenance database with arg: ${extra_args}"
    cd -

    #Committer
    echo "==========================================="
    echo "Instantiating committer"
    echo "==========================================="
    for((i=0;i<provdb_ninstances;i++)); do
	echo "Chimbuko services launching provDB committer ${i} of ${provdb_ninstances} using extra args \"${commit_extra_args}\""
	cmd="provdb_commit "${provdb_addr_dir}" -instance ${i} -ninstances ${provdb_ninstances} -nshards ${provdb_nshards} -freq_ms ${provdb_commit_freq} ${commit_extra_args} > ${log_dir}/committer_${i}.log 2>&1 &"
	if [[ ${do_numa_bind} -eq 1 ]]; then
	    #Put the committer in the same NUMA domain as its parent instance
	    numa=${numas[i]}
	    echo "Chimbuko services binding provDB committer instance ${i} to NUMA domain ${numa}"
	    cmd="numactl -N ${numa} ${cmd}"
	fi
	echo $cmd
	eval $cmd
	sleep 1
    done
    sleep 3  
else
    echo "Chimbuko Services: Provenance database is not in use, provenance data will be stored in ASCII format at ${provdb_writedir}"
    extra_args+=" -prov_outputpath ${provdb_writedir}"
    ps_extra_args+=" -prov_outputpath ${provdb_writedir}"
fi

#Visualization
if (( ${use_viz} == 1 )); then
    # Pserver is not needed when running the viz in offline simulation mode
    # if (( ${use_pserver} != 1 )); then
    # 	echo "Chimbuko Services: Error - cannot use viz without the pserver"
    # 	exit 1
    # fi
    # if (( ${use_provdb} == 0 )); then
    # 	echo "Chimbuko Services: Error - cannot use viz without the provDB"
    # 	exit 1
    # fi

    #Provide parameters for provenance database
    export PROVDB_NINSTANCE=${provdb_ninstances}
    export SHARDED_NUM=${provdb_nshards}
    export PROVENANCE_DB=${provdb_writedir}/ #already an absolute path

    #If provdb server is used, tell the viz how to connect to it; otherwise it will use its own internal server
    if (( ${use_provdb} )); then
	if (( ${provdb_ninstances} == 1 )); then
	    #Simpler instantiation if a single server
	    export PROVDB_ADDR=$(cat ${provdb_dir}/provider.address.0)
	    echo "Chimbuko Services: viz is connecting to provDB provider 0 on address" $PROVDB_ADDR
	else
	    export PROVDB_ADDR_PATH=${provdb_addr_dir}
	    echo "Chimbuko Services: viz is obtaining provDB addresses from path" $PROVDB_ADDR_PATH
	fi
    fi

    cd ${viz_dir}
    HOST=`hostname`
    export SERVER_CONFIG="production"
    export DATABASE_URL="sqlite:///${viz_dir}/main.sqlite"
    export ANOMALY_STATS_URL="sqlite:///${viz_dir}/anomaly_stats.sqlite"
    export ANOMALY_DATA_URL="sqlite:///${viz_dir}/anomaly_data.sqlite"
    export FUNC_STATS_URL="sqlite:///${viz_dir}/func_stats.sqlite"
    export CELERY_BROKER_URL="redis://${HOST}:${viz_worker_port}"

    #Setup redis
    cp -r $viz_root/redis-stable/redis.conf .
    sed -i "s|^dir ./|dir ${viz_dir}/|" redis.conf
    sed -i "s|^bind 127.0.0.1|bind 0.0.0.0|" redis.conf
    sed -i "s|^daemonize no|daemonize yes|" redis.conf
    sed -i "s|^pidfile /var/run/redis_6379.pid|pidfile ${viz_dir}/redis.pid|" redis.conf
    sed -i "s|^logfile "\"\""|logfile ${log_dir}/redis.log|" redis.conf
    sed -i "s|.*syslog-enabled no|syslog-enabled yes|" redis.conf
    sed -i "s|^protected-mode yes|protected-mode no|" redis.conf
    
    echo "==========================================="
    echo "Chimbuko Services: Launch Chimbuko visualization server"
    echo "==========================================="
    cd ${viz_root}

    echo "Chimbuko Services: create db ..."
    python3 -u manager.py createdb

    echo "Chimbuko Services: run redis ..."
    redis-server ${viz_dir}/redis.conf
    sleep 5

    echo "Chimbuko Services: run celery ..."
    CELERY_ARGS="--loglevel=info --concurrency=1"
    python3 manager.py celery ${CELERY_ARGS} 2>&1 | tee "${log_dir}/celery.log" &
    sleep 10

    echo "Chimbuko Services: run webserver ..."
    #python3 run_server.py $HOST $viz_port 2>&1 | tee "${log_dir}/webserver.log" &
    python3 manager.py runserver --host 0.0.0.0 --port ${viz_port} --debug 2>&1 | tee "${log_dir}/webserver.log" &
    sleep 2

    echo "Chimbuko Services: redis ping-pong ..."
    redis-cli -h $HOST -p ${viz_worker_port} ping

    cd ${base}

    #Write a termination script for the viz that can be called either manually or automatically
    cat <<EOF > ${viz_dir}/shutdown_webserver.sh

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

EOF
    chmod u+x ${viz_dir}/shutdown_webserver.sh

    ws_addr="http://${HOST}:${viz_port}/api/anomalydata"
    ps_extra_args+=" -ws_addr ${ws_addr}"

    echo $HOST > ${var_dir}/chimbuko_webserver.host
    echo $viz_port > ${var_dir}/chimbuko_webserver.port
    echo $ws_addr > ${var_dir}/chimbuko_webserver.url
    echo "Chimbuko Services: Webserver is running on ${HOST}:${viz_port} and is ready for the user to connect"
fi

if (( ${use_pserver} == 1 )); then   
    echo "==========================================="
    echo "Chimbuko Services: Instantiating pserver"
    echo "==========================================="

    #Get head node IP
    if command -v ip &> /dev/null
    then
	ip=$(ip -4 addr show ${pserver_interface} | grep -oP '(?<=inet\s)\d+(\.\d+){3}')
    elif command -v ifconfig &> /dev/null
    then
	ip=$(ifconfig 2>&1 ${pserver_interface} | grep -E -o 'inet [0-9.]+' | awk '{print $2}')
    else
	echo "Chimbuko services: Neither ifconfig or ip commands exists; cannot infer ip for interface ${pserver_interface}"
	exit 1
    fi

    echo "Chimbuko Services: Launching pserver on interface ${pserver_interface} with host IP" ${ip}

    pserver_addr="tcp://${ip}:${pserver_port}"  #address for parameter server in format "tcp://IP:PORT"    
    
    echo "Chimbuko Services: Pserver $pserver_addr"

    pserver_alg=${ad_alg} #Pserver AD algorithm choice must match that used for the driver
    pserver_addr="tcp://${ip}:${pserver_port}"  #address for parameter server in format "tcp://IP:PORT"
    cmd="pserver ${module} -ad ${pserver_alg} -nt ${pserver_nt} -logdir ${log_dir} -port ${pserver_port} -save_params ${ps_dir}/global_model.json ${ps_extra_args} 2>&1 | tee ${log_dir}/pserver.log  &"
    if [[ ! -z "${pserver_numa_bind:-}" ]]; then
	echo "Chimbuko Services binding pserver to NUMA domain ${pserver_numa_bind}"
	cmd="numactl -N ${pserver_numa_bind} ${cmd}"
    fi
    echo $cmd
    eval $cmd
    ps_pid=$!
    extra_args+=" -pserver_addr ${pserver_addr}"
    sleep 2
fi

######################################################################
#Generate the default configuration file for the TAU monitoring plugin
if [[ "${tau_monitoring_conf}" != "default" ]]; then
    if [[ ! -f "${tau_monitoring_conf}" ]]; then
	echo "Chimbuko Services: Error: TAU monitoring configuration file is not accessible"
	exit 1
    fi
    to_file="${base}/tau_monitoring.json"
    full_from=$(readlink -f ${tau_monitoring_conf})
    full_to=$(readlink -f ${to_file})

    if [[ "${full_from}" != "${full_to}" ]]; then
	cp ${full_from} ${full_to}
	echo "Chimbuko Services: copied tau_monitoring.json from ${tau_monitoring_conf}"
    else
	echo "Chimbuko Services: tau_monitoring.json source and destination files are the same"
    fi

else
    to_file="${base}/tau_monitoring.json"

    cat <<EOF > ${to_file}
{
  "periodic": true,
  "node_data_from_all_ranks": true,
  "monitor_counter_prefix": "monitoring: ",
  "periodicity seconds": 1.0,
  "PAPI metrics": [],
  "/proc/stat": {
    "disable": false,
    "comment": "This will exclude all core-specific readings.",
    "exclude": ["^cpu[0-9]+.*"]
  },
  "/proc/meminfo": {
    "disable": false,
    "comment": "This will include three readings.",
    "include": [".*MemAvailable.*", ".*MemFree.*", ".*MemTotal.*"]
  },
  "/proc/net/dev": {
    "disable": false,
    "comment": "This will include only the first ethernet device.",
    "include": [".*eno1.*"]
  },
  "/proc/self/net/dev": {
    "disable": true,
    "comment": "This will include only the first ethernet device (from network namespace of which process is a member).",
    "include": [".*eno1.*"]
  },
  "lmsensors": {
    "disable": true,
    "comment": "This will include all power readings.",
    "include": [".*power.*"]
  },
  "net": {
    "disable": true,
    "comment": "This will include only the first ethernet device.",
    "include": [".*eno1.*"]
  },
  "nvml": {
    "disable": true,
    "comment": "This will include only the utilization metrics.",
    "include": [".*utilization.*"]
  }
}
EOF
    echo "Chimbuko Services: generated default tau_monitoring.json"

fi


######################################################################
#echo "Chimbuko Services: Processes are: " $(ps)

#Check that the variables passed to the AD from the config file are defined
testit=${ad_outlier_sstd_sigma}
testit=${ad_win_size}
testit=${ad_alg}
testit=${ad_outlier_hbos_threshold}

############################################
#Generate the command to launch the AD module
ad_opts="${extra_args} -err_outputpath ${log_dir} -outlier_sigma ${ad_outlier_sstd_sigma} -anom_win_size ${ad_win_size} -ad_algorithm ${ad_alg} -hbos_threshold ${ad_outlier_hbos_threshold}"

if [[ "$(declare -p EXE_NAME)" =~ "declare -a" ]]; then
    echo "The user has specified a workflow comprising multiple components. Chimbuko will generate separate files containing pre-generated launch commands."
    for i in ${!EXE_NAME[@]}; do
	exe=${EXE_NAME[$i]}
	ad_cmd="driver ${module} ${TAU_ADIOS2_ENGINE} ${TAU_ADIOS2_PATH} ${TAU_ADIOS2_FILE_PREFIX}-${exe} -program_idx ${i} ${ad_opts} 2>&1 | tee ${log_dir}/ad.${exe}.log"
	echo ${ad_cmd} > ${var_dir}/chimbuko_ad_cmdline.${exe}.var

	echo "---------------------------"
	echo "Command to launch AD module for program index ${i} with executable ${exe}: " ${ad_cmd}
	echo "Please execute as:"
	echo "    ad_cmd=\$(cat ${var_dir}/chimbuko_ad_cmdline.${exe}.var)"
	echo "    eval \"mpirun -n \${TASKS} \${ad_cmd} &\""
	echo "Or equivalent"

	#For use in other scripts, output the AD cmdline options to file
	echo "${module} ${TAU_ADIOS2_ENGINE} ${TAU_ADIOS2_PATH} ${TAU_ADIOS2_FILE_PREFIX}-${exe} -program_idx ${i}" > ${var_dir}/chimbuko_ad_args.${exe}.var
    done
    #AD option set is generic
    echo ${ad_opts} > ${var_dir}/chimbuko_ad_opts.var    
else
    ad_cmd="driver ${module} ${TAU_ADIOS2_ENGINE} ${TAU_ADIOS2_PATH} ${TAU_ADIOS2_FILE_PREFIX}-${EXE_NAME} ${ad_opts} 2>&1 | tee ${log_dir}/ad.log"
    echo ${ad_cmd} > ${var_dir}/chimbuko_ad_cmdline.var

    echo "Command to launch AD module: " ${ad_cmd}
    echo "Please execute as:"
    echo "    ad_cmd=\$(cat ${var_dir}/chimbuko_ad_cmdline.var)"
    echo "    eval \"mpirun -n \${TASKS} \${ad_cmd} &\""
    echo "Or equivalent"

    #For use in other scripts, output the AD cmdline options to file
    echo "${module} ${TAU_ADIOS2_ENGINE} ${TAU_ADIOS2_PATH} ${TAU_ADIOS2_FILE_PREFIX}-${EXE_NAME}" > ${var_dir}/chimbuko_ad_args.var
    echo ${ad_opts} > ${var_dir}/chimbuko_ad_opts.var
fi

if (( ${use_pserver} == 1 )); then
    #Wait for pserver to exit, which means it's time to end the viz
    echo "Chimbuko Services: waiting for pserver (pid ${ps_pid}) to terminate"
    wait ${ps_pid}
fi

if (( ${use_viz} == 1 )); then
    if (( ${use_pserver} == 1 )); then
	${viz_dir}/shutdown_webserver.sh
    else
	echo "Chimbuko Services: Visualization was run without the pserver. It will remain active until the user executes script ${viz_dir}/shutdown_webserver.sh"
	disown -ah
    fi
fi

if (( ${use_provdb} == 1 )); then
    echo "Chimbuko Services: waiting for provdb to terminate"
    wait
fi


echo "Chimbuko Services: Service script complete" $(date)

