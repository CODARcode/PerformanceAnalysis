#!/bin/bash

export TAU_ROOT=/opt/tau2/x86_64
export TAU_MAKEFILE=$TAU_ROOT/lib/Makefile.tau-papi-mpi-pthread-pdt-adios2
export TAU_PLUGINS_PATH=$TAU_ROOT/lib/shared-papi-mpi-pthread-pdt-adios2
export TAU_PLUGINS=libTAU-adios2-trace-plugin.so

export TAU_ADIOS2_PERIODIC=1
export TAU_ADIOS2_PERIOD=100000
export TAU_ADIOS2_ENGINE=SST
export TAU_ADIOS2_FILENAME=tau-metrics
export TAU_VERBOSE=1
rm -rf profile.* tau-metrics* 0*

admin=-1
extra_args=""

if [ -x "$(command -v provdb_admin)" ]; then
    rm provdb.unqlite  provider.address

    ip=$(hostname -i)
    port=1234

    echo "Instantiating provenance database"
    provdb_admin ${ip}:${port} -autoshutdown &
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

appdir=$(which pserver | sed 's/pserver//')

ip=$(hostname -i)
echo "Instantiating web server with print dump"
python3 ${appdir}/ws_flask_stat.py -host ${ip} -port 5000 -print_msg  2>&1 | tee ws.log  &
ws_pid=$!
ws_addr=http://${ip}:5000/post
sleep 1

ranks=3

pserver_addr=tcp://${ip}:5559
pserver_nt=1
pserver_logdir="."
echo "Instantiating pserver"
echo "Pserver $pserver_addr"
pserver ${ranks} -nt ${pserver_nt} -logdir ${pserver_logdir} -ws_addr ${ws_addr} 2>&1 | tee pserver.log  &
extra_args="${extra_args} -pserver_addr ${pserver_addr}"
sleep 2

echo "Instantiating AD"
mpirun --allow-run-as-root -n ${ranks} driver SST . tau-metrics-main . ${extra_args} 2>&1 | tee ad.log &
ad=$!

sleep 2

echo "Running main"
cycles=1000
anom_freq=300
modetimes="10,20,30"
anom_time=100

mpirun --allow-run-as-root -n ${ranks} tau_exec ${EXEC_OPTS} ${EXEC_T} ./main ${cycles} ${modetimes} ${anom_time} ${anom_freq} 2>&1 | tee main.log

wait $ad
kill ${ws_pid}

# if [ $admin != -1 ]; then
#     wait $ad
#     echo "Terminating provenance DB admin"
#     kill $admin
# fi
