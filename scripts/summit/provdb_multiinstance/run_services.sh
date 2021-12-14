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

#Set absolute paths
base=$(pwd)
viz_dir=${base}/chimbuko/viz
log_dir=${base}/chimbuko/logs
provdb_dir=${base}/chimbuko/provdb
var_dir=${base}/chimbuko/vars
bp_dir=${base}/chimbuko/adios2

#Check tau adios2 path is writable; by default this is ${bp_dir} but it can be overridden by users, eg for offline analysis
#Also get the provdb absolute path if provDB is in use
#These need to be performed under jsrun as they may be on a path only accessible on the compute nodes (eg the nvme)
echo "Chimbuko Services: checking paths of provDB and trace write directories"
jsrun -U services_main.urs ./path_check.sh
if (( ${use_provdb} == 1 )); then
    provdb_writedir=$(cat provdb_writedir.log) 
    echo "Chimbuko Services: absolute path of provDB write directory is ${provdb_writedir}"
fi

#Get head node IP
#jsrun -U services_main.urs ./get_head_ip.sh ${service_node_iface}
#ip=$(cat ip.addr)
#echo "Chimbuko Services: Launching Chimbuko services on interface ${service_node_iface} with host IP" ${ip}


####################################
#Run the Chimbuko head node services
####################################

#Provenance database
extra_args=${ad_extra_args}
ps_extra_args=${pserver_extra_args}
if (( ${use_provdb} == 1 )); then
    echo "==========================================="
    echo "Instantiating provenance database"
    echo "==========================================="
   
    cd ${provdb_dir}
    
    #Enable better error reporting from Mercury
    export HG_LOG_SUBSYS=hg
    export HG_LOG_LEVEL=error

    for((i=0;i<provdb_ninstances;i++)); do
	port=$((provdb_port+i))
	provdb_addr="${service_node_iface}:${port}"    #can be IP:PORT or ADAPTOR:PORT per libfabric conventions
	if [[ ${provdb_engine} == "verbs" ]]; then
	    provdb_addr="${provdb_domain}/${provdb_addr}"
	fi
	echo "Chimbuko services launching provDB instance ${i} of ${provdb_ninstances} on address ${provdb_addr}"
	jsrun -U ${base}/services_inst${i}.urs provdb_admin "${provdb_addr}" ${provdb_extra_args} -engine ${provdb_engine} -nshards ${provdb_nshards} -db_write_dir ${provdb_writedir} -server_instance ${i} ${provdb_ninstances} 2>&1 | tee ${log_dir}/provdb_${i}.log &
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

    extra_args+=" -provdb_addr_dir ${provdb_writedir} -nprovdb_instances ${provdb_ninstances} -nprovdb_shards ${provdb_nshards}"
    ps_extra_args+=" -provdb_addr_dir ${provdb_writedir}"
    echo "Chimbuko Services: Enabling provenance database with arg: ${extra_args}"
    cd -
else
    echo "Chimbuko Services: Provenance database is not in use, provenance data will be stored in ASCII format at ${provdb_writedir}"
    extra_args+=" -prov_outputpath ${provdb_writedir}"
    ps_extra_args+=" -prov_outputpath ${provdb_writedir}"
fi

#Launch the main services
jsrun -U services_main.urs ./run_services_main.sh ${ps_extra_args} ${extra_args} &
echo "Chimbuko Services : waiting for main services to launch"
while [ ! -f extra_args.out ]; do
    sleep 1;
done
extra_args=$(cat extra_args.out)


#Check that the variables passed to the AD from the config file are defined
testit=${ad_outlier_sstd_sigma}
testit=${ad_win_size}
testit=${ad_alg}
testit=${ad_outlier_hbos_threshold}

############################################
#Generate the command to launch the AD module
ad_opts="${extra_args} -err_outputpath ${log_dir} -outlier_sigma ${ad_outlier_sstd_sigma} -anom_win_size ${ad_win_size} -ad_algorithm ${ad_alg} -hbos_threshold ${ad_outlier_hbos_threshold}"
ad_cmd="driver ${TAU_ADIOS2_ENGINE} ${TAU_ADIOS2_PATH} ${TAU_ADIOS2_FILE_PREFIX}-${EXE_NAME} ${ad_opts} 2>&1 | tee ${log_dir}/ad.log"
echo ${ad_cmd} > ${var_dir}/chimbuko_ad_cmdline.var

echo "Command to launch AD module: " ${ad_cmd}
echo "Please execute as:"
echo "    ad_cmd=\$(cat ${var_dir}/chimbuko_ad_cmdline.var)"
echo "    eval \"mpirun -n \${TASKS} \${ad_cmd} &\""
echo "Or equivalent"

#For use in other scripts, output the AD cmdline options to file
echo ${ad_opts} > ${var_dir}/chimbuko_ad_opts.var

wait

echo "Chimbuko Services: Service script complete" $(date)
