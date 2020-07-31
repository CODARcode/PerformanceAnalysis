#!/bin/bash
#Fail if any test fails
set -e
set -o pipefail

if [ -f "../bin/provdb_admin" ]; then
    #Connect via tcp
    rm -f provdb.unqlite  provider.address

    ip=$(hostname -i)
    port=1234

    ../bin/provdb_admin ${ip}:${port} -autoshutdown &
    admin=$!
    sleep 1

    mpirun -n 3 --oversubscribe --allow-run-as-root ./provDBclientConnectDisconnect $(cat provider.address)

    sleep 5
    if [[ $(ps | grep $admin) != "" ]]; then
	echo "ERROR: provDB was not killed!"
	exit 1
    fi
else 
    echo "Provenance DB was not built"
fi
