#!/bin/bash
#Fail if any test fails
set -e
set -o pipefail

if [ -f "../bin/provdb_admin" ]; then
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
    
    #Connect via tcp
    echo "Test 4 clients 1 shard ${protocol}"
    rm -f provdb.*.unqlite*  provider.address* margo_dump*.state

    ip=$(hostname -i)
    port=1234
    shards=1

    ../bin/provdb_admin ${ip}:${port} -engine "${protocol}" -autoshutdown false -nshards ${shards} &
    admin=$!
    while [ ! -f provider.address.0 ]; do
	echo "Waiting for provider address"
	sleep 1;
    done

    mpirun -n 4 --allow-run-as-root --oversubscribe ./mainProvDBclient $(cat provider.address.0) ${shards}

    kill $admin
    while [[ $(ps $admin | wc -l) == 2 ]]; do
	echo "Waiting for shutdown"
	sleep 1
    done
    
    #Repeat with multiple clients and multiple shards
    #sleep 3
    echo "Test 4 clients 2 shards ${protocol}"
    rm -f provdb.*.unqlite*  provider.address*  margo_dump*.state
    shards=2

    ../bin/provdb_admin ${ip}:${port} -engine "${protocol}" -autoshutdown false -nshards ${shards}  &
    admin=$!
    while [ ! -f provider.address.0 ]; do
	echo "Waiting for provider address"
	sleep 1;
    done

    mpirun -n 4 --allow-run-as-root --oversubscribe ./mainProvDBclient $(cat provider.address.0) ${shards}

    kill $admin
    while [[ $(ps $admin | wc -l) == 2 ]]; do
	echo "Waiting for shutdown"
	sleep 1
    done
   
    #Connect via na+sm
    echo "Test 1 clients 1 shards na+sm"
    rm -f provdb.*.unqlite*  provider.address*  margo_dump*.state
    shards=1

    ../bin/provdb_admin "" -engine "na+sm" -autoshutdown false  -nshards ${shards} &
    admin=$!
    while [ ! -f provider.address.0 ]; do
	echo "Waiting for provider address"
	sleep 1;
    done

    ./mainProvDBclient $(cat provider.address.0) ${shards}

    kill $admin

else 
    echo "Provenance DB was not built"
fi

