#!/bin/bash

export PATH=/opt/chimbuko/ad/bin:${PATH}

export TAU_ADIOS2_PERIODIC=1
export TAU_ADIOS2_PERIOD=100000
#export TAU_ADIOS2_ENGINE=BPFile
export TAU_ADIOS2_ENGINE=SST
export TAU_ADIOS2_FILENAME=tau-metrics
#export CHIMBUKO_VERBOSE=1

rm -rf profile.* tau-metrics* 0*

admin=-1
extra_args=""

if [ -x "$(command -v provdb_admin)" ]; then
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
    echo "Enabling provenance database with arg: ${extra_args}"

else
    echo "Not using provenance database"
fi

echo "Instantiating AD"
mpirun --allow-run-as-root -n 1 driver SST . tau-metrics-python3.6 . ${extra_args} 2>&1 | tee ad.log &
#mpirun --allow-run-as-root -n 1 sst_view tau-metrics-python3.6-0.bp -nsteps_show_variable_values -1 2>&1 | tee view.log &
ad=$!

sleep 2

echo "Running main"
tau_python -vv -tau-python-interpreter=python3 -adios2_trace test.py 2>&1 | tee run.log

if [ $admin != -1 ]; then
    wait $ad
    echo "Terminating provenance DB admin"
    kill $admin
fi
