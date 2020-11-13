#!/bin/bash
#Fail if any test fails
set -e
set -o pipefail

if [ -f "../bin/provdb_admin" ]; then
    echo "Test 1 clients 1 pserver TCP"
    rm -f provdb.*.unqlite*  provider.address

    ip=$(hostname -i)
    port=1234
    shards=1

    ../bin/provdb_admin ${ip}:${port}  -autoshutdown true -nshards 1 &
    admin=$!
    sleep 4

    mpirun -n 2 --allow-run-as-root --oversubscribe ./mainProvDBpserverClient $(cat provider.address)

    wait $admin
else 
    echo "Provenance DB was not built"
fi

