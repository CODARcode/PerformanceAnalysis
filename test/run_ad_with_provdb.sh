#!/bin/bash
#Fail if any test fails
set -e
set -o pipefail

if [ -f "../bin/provdb_admin" ]; then
    rm -f provdb.*.unqlite*  provider.address

    ip=$(hostname -i)
    port=1234

    ../bin/provdb_admin ${ip}:${port} &
    admin=$!
    while [ ! -f provider.address ]; do
	echo "Waiting for provider address"
	sleep 1;
    done

    mpirun --oversubscribe --allow-run-as-root -n 4 ./mainADwithProvDB $(cat provider.address)

    kill $admin
else 
    echo "Provenance DB was not built"
fi
