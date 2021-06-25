#!/bin/bash
#Fail if any test fails
set -e
set -o pipefail

if [ -f "../bin/provdb_admin" ]; then
    rm -f provdb.*.unqlite*  provider.address

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

    echo "Test 1 clients 1 pserver ${protocol}"

    
    ip=$(hostname -i)
    port=1234
    shards=1

    ../bin/provdb_admin ${ip}:${port}  -autoshutdown true -nshards 1 -engine "${protocol}" &
    admin=$!
    while [ ! -f provider.address ]; do
	echo "Waiting for provider address"
	sleep 1;
    done

    mpirun -n 2 --allow-run-as-root --oversubscribe ./mainProvDBpserverClient $(cat provider.address)

    wait $admin
else 
    echo "Provenance DB was not built"
fi

