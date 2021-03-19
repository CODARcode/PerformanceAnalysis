#!/bin/bash
#Fail if any test fails
set -e
set -o pipefail

if [ -f "../bin/provdb_admin" ]; then
    #Connect via tcp
    echo "Test 4 clients 1 shard TCP"
    rm -f provdb.*.unqlite*  provider.address

    ip=$(hostname -i)
    port=1234
    shards=1

    ../bin/provdb_admin ${ip}:${port}  -autoshutdown false -nshards ${shards} &
    admin=$!
    while [ ! -f provider.address ]; do
	echo "Waiting for provider address"
	sleep 1;
    done

    mpirun -n 4 --allow-run-as-root --oversubscribe ./mainProvDBclient $(cat provider.address) ${shards}

    kill $admin
    while [[ $(ps $admin | wc -l) == 2 ]]; do
	echo "Waiting for shutdown"
	sleep 1
    done
    
    #Repeat with multiple clients and multiple shards
    #sleep 3
    echo "Test 4 clients 2 shards TCP"
    rm -f provdb.*.unqlite*  provider.address
    shards=2

    ../bin/provdb_admin ${ip}:${port}  -autoshutdown false -nshards ${shards}  &
    admin=$!
    while [ ! -f provider.address ]; do
	echo "Waiting for provider address"
	sleep 1;
    done

    mpirun -n 4 --allow-run-as-root --oversubscribe ./mainProvDBclient $(cat provider.address) ${shards}

    kill $admin
    while [[ $(ps $admin | wc -l) == 2 ]]; do
	echo "Waiting for shutdown"
	sleep 1
    done
   
    #Connect via na+sm
    echo "Test 1 clients 1 shards na+sm"
    rm -f provdb.*.unqlite*  provider.address
    shards=1

    ../bin/provdb_admin "" -engine "na+sm" -autoshutdown false  -nshards ${shards} &
    admin=$!
    while [ ! -f provider.address ]; do
	echo "Waiting for provider address"
	sleep 1;
    done

    ./mainProvDBclient $(cat provider.address) ${shards}

    kill $admin

else 
    echo "Provenance DB was not built"
fi

