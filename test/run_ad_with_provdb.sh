#!/bin/bash
#Fail if any test fails
set -e
set -o pipefail

if [ -f "../bin/provdb_admin" ]; then
    rm -f provdb.*.unqlite*  provider.address*

    #Find a protocol supported by libfabric
    protocol=
    if [ $(fi_info -l | grep ofi_rxm | wc -l) == "1" ]; then
	protocol="ofi+tcp"
    elif [ $(fi_info -l | grep sockets | wc -l) == "1" ]; then
	protocol="sockets"
    else
	"Neither tcp;ofi_rxm or sockets fabrics are supported by the install of libfabrics"
	exit 1
    fi
    
    ip=$(hostname -i)
    port=1234

    ../bin/provdb_admin performance_analysis ${ip}:${port} -engine "$protocol" &
    while [ ! -f provider.address.0 ]; do
	echo "Waiting for provider address"
	sleep 1;
    done

    mpirun --oversubscribe --allow-run-as-root -n 4 ./mainADwithProvDB "." 1 1

    wait
  
    #Do a run with 2 shards and 1 server instance
    rm -f provdb.*.unqlite*  provider.address*

    ../bin/provdb_admin performance_analysis ${ip}:${port} -engine "$protocol" -nshards 2 &
    while [ ! -f provider.address.0 ]; do
	echo "Waiting for provider address"
	sleep 1;
    done

    mpirun --oversubscribe --allow-run-as-root -n 4 ./mainADwithProvDB "." 2 1

    wait

    #Do a run with multiple shards and 2 server instances
    rm -f provdb.*.unqlite*  provider.address*

    port2=1235

    ../bin/provdb_admin performance_analysis ${ip}:${port} -engine "$protocol" -nshards 2 -server_instance 0 2 &
    ../bin/provdb_admin performance_analysis ${ip}:${port2} -engine "$protocol" -nshards 2 -server_instance 1 2 &

    while [ ! -f provider.address.0 ]; do
	echo "Waiting for provider address 0"
	sleep 1;
    done
    while [ ! -f provider.address.1 ]; do
	echo "Waiting for provider address 1"
	sleep 1;
    done

    mpirun --oversubscribe --allow-run-as-root -n 4 ./mainADwithProvDB "." 2 2
 
    wait
else 
    echo "Provenance DB was not built"
fi
