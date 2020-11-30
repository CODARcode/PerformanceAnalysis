#!/bin/bash

export TAU_ROOT=/opt/tau2/x86_64
export TAU_MAKEFILE=$TAU_ROOT/lib/Makefile.tau-papi-pthread-python-pdt-adios2
export TAU_PLUGINS_PATH=$TAU_ROOT/lib/shared-papi-pthread-python-pdt-adios2
export TAU_PLUGINS=libTAU-adios2-trace-plugin.so

export TAU_ADIOS2_PERIODIC=1
export TAU_ADIOS2_PERIOD=1000000
export TAU_ADIOS2_ENGINE=SST
export TAU_VERBOSE=1
rm -rf profile.* tau-metrics*

admin=-1
extra_args=""
ps_extra_args=""
if (( 1 )); then
    rm provdb.unqlite  provider.address

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
fi

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

#Instantiate the application alongside an instance of the AD
#We should use different filenames for the main app to prevent them from writing to the same file (the rank index is 0 always as no MPI)
#Alternatively could use different directories
instances=2

for (( i=0; i<${instances}; i++ ));
do
    fn_tau=tau-metrics-${i}
    fn_driver=tau-metrics-${i}-main

    if (( 1 )); then
	#We will ask Chimbuko to associate a rank index with the trace data according to the instance index
	#This can be achieved by combining -rank with -override_rank <orig rank index = 0>
	driver ${TAU_ADIOS2_ENGINE} . ${fn_driver} . ${extra_args} -rank ${i} -override_rank 0 2>&1 | tee ad_${i}.log &
    fi

    cycles=500
    anom_freq=200
    base_time=10
    anom_time=100

    TAU_ADIOS2_FILENAME=${fn_tau} ./main ${i} ${cycles} ${base_time} ${anom_time} ${anom_freq} 2>&1 | tee main_${i}.log &
done

wait
