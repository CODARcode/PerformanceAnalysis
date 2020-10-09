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
    sleep 4

    mpirun -n 4 --allow-run-as-root --oversubscribe ./mainProvDBclient $(cat provider.address) ${shards}

    kill $admin

    #Repeat with multiple clients and multiple shards
    sleep 3
    echo "Test 4 clients 2 shards TCP"
    rm -f provdb.*.unqlite*  provider.address
    shards=2

    ../bin/provdb_admin ${ip}:${port}  -autoshutdown false -nshards ${shards}  &
    admin=$!
    sleep 4

    mpirun -n 4 --allow-run-as-root --oversubscribe ./mainProvDBclient $(cat provider.address) ${shards}

    kill $admin

    #Connect via na+sm
    sleep 3
    echo "Test 1 clients 1 shards na+sm"
    rm -f provdb.*.unqlite*  provider.address
    shards=1

    ../bin/provdb_admin "" -engine "na+sm" -autoshutdown false  -nshards ${shards} &
    admin=$!
    sleep 4

    ./mainProvDBclient $(cat provider.address) ${shards}

    kill $admin

else 
    echo "Provenance DB was not built"
fi

