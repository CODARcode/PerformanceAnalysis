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
    
    ip=$(hostname -i)
    port=1234

    ../bin/provdb_admin ${ip}:${port} -engine "$protocol" &
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
