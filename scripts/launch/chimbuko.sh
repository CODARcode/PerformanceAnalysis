#!/bin/bash

source ${CHIMBUKO_CONFIG}

#export HG_LOG_LEVEL=debug
#export HG_LOG_SUBSYS=cls
#export FI_LOG_LEVEL=debug

rank=${SLURM_PROCID}

narg=$#
if [[ $narg -lt 1 ]]; then
    echo "Require at least one argument: the number of application tasks/ranks"
    exit 1
fi

app_tasks=$1
echo "rank \"${rank}\" app_tasks \"${app_tasks}\" node \"${SLURMD_NODENAME}\""
if [[ ${rank} -ge ${app_tasks} ]]; then
    #Service task
    if [[ ${SLURM_LOCALID} -eq 0 ]]; then
	echo "Launching service task on node ${SLURMD_NODENAME} local ID ${SLURM_LOCALID}"
	echo "Environment:"
	printenv
	
	exec ${chimbuko_services} 2>&1 | tee services.log
    fi
else
    echo "Driver rank ${rank} waiting for service spin-up"
    while [ ! -f chimbuko/vars/chimbuko_ad_cmdline.var ]; do sleep 1; done
    ad_opts=$(cat chimbuko/vars/chimbuko_ad_opts.var)
    ad_cmd="driver ${TAU_ADIOS2_ENGINE} ${TAU_ADIOS2_PATH} ${TAU_ADIOS2_FILE_PREFIX}-${EXE_NAME} ${ad_opts} -rank ${rank}"

    a=1
    args=("$@")

    preop=""
    
    while [ $a -lt $narg ]; do
	arga=${args[$a]}
	if [[ "$arga" == "--core_bind" ]]; then
	    ap1=$(( a + 1 ))
	    bnd=${args[$ap1]}

	    rbnd="None"
	    cnt=0
	    for i in $(echo $bnd | tr ":" "\n")
	    do
		if [[ $cnt -eq $rank ]]; then
		    rbnd=$i
		fi
		cnt=$(( cnt + 1 ))
	    done

	    if [[ "$rbnd" == "None" ]]; then
		echo "Could not parse binding for rank ${rank}"
		exit 1
	    fi

	    cmask=`python3 <<EOF
import sys

#Provide a comma-separated list of CPUs
#Output: binary and hex masks corresponding to the list
bnd="${rbnd}"
lst=bnd.split(',')

val=0
for v in lst:
    val = val | (1<<int(v))

print(hex(val))    
EOF`
	    
	    echo "Binding rank ${rank} to ${rbnd} with core mask ${cmask}"
	    preop="taskset ${cmask} ${preop}"
	    a=$(( a + 2 ))
	else
	    echo "Unrecognized argument \"$arga\""
	    exit 1
	fi    
    done #end while
   
    exec ${preop} ${ad_cmd} 2>&1 | tee chimbuko/logs/ad.${SLURM_PROCID}.log
fi
