#!/bin/bash

export TAU_ROOT=/opt/tau2/x86_64
export TAU_MAKEFILE=$TAU_ROOT/lib/Makefile.tau-papi-mpi-pthread-python-cupti-pdt-adios2
export TAU_PLUGINS_PATH=$TAU_ROOT/lib/shared-papi-mpi-pthread-python-cupti-pdt-adios2
export TAU_PLUGINS=libTAU-adios2-trace-plugin.so

export TAU_ADIOS2_PERIODIC=1
export TAU_ADIOS2_PERIOD=1000000
#export TAU_ADIOS2_ENGINE=BPFile
export TAU_ADIOS2_ENGINE=SST
export TAU_ADIOS2_FILENAME=tau-metrics
export TAU_VERBOSE=1
rm -rf profile.* tau-metrics* 0*

admin=-1
extra_args=""

if (( 1 )); then
if [ -x "$(command -v provdb_admin)" ]; then
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
    echo "Enabling provenance database with arg: ${extra_args}"

else
    echo "Not using provenance database"
fi
fi

if (( 1 )); then
echo "Instantiating AD"
mpirun --allow-run-as-root -n 1 driver SST . tau-metrics-main . ${extra_args} 2>&1 | tee ad.log &
#mpirun --allow-run-as-root -n 1 sst_view tau-metrics-main-0.bp -nsteps_show_variable_values -1 2>&1 | tee view.log &
ad=$!

sleep 2
fi

EXEC_OPTS="-cupti -env -um" #    -monitoring -env -adios2 -adios2_trace -um -env
#EXEC_OPTS="-cupti"
#EXEC_OPTS=""
EXEC_T="-T papi,mpi,pthread,cupti,pdt,adios2"

#papi-mpi-pthread-cupti-pdt-adios2

if (( 1 )); then
echo "Running main"
cycles=300
freq=30
startcyc=20
mpirun --allow-run-as-root -n 1 tau_exec ${EXEC_OPTS} ${EXEC_T} ./main ${cycles} ${freq} ${startcyc} -device_max 2 -mult 100 -base 10000000 2>&1 | tee main.log
fi
